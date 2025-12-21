#include "audio_visual_processor.h"
#include "ws2812b.h"

/* ====================== 参数配置区 ====================== */

#define NOISE_LEARN_RATE     0.002f     // 噪声底学习速率
#define NOISE_GATE_GAIN     3.0f       // 相对噪声门限（≈ +9.5 dB）
#define SILENCE_FFT_ENERGY  0.0005f    // FFT 全局静音阈值

#define AGC_UP_RATE         0.30f
#define AGC_DOWN_RATE       0.97f

#define MID_KILL_THRESHOLD  0.35f      // 中频绝对熄灭阈值

/* ====================== 频段定义 ====================== */

const uint16_t freq_bands[FREQ_BINS] =
{
    400, 800, 1200, 2000, 3500, 6000, 10000, 16000
};

// const uint16_t freq_bands[FREQ_BINS] =
// {
//     2000, 4000, 6000, 8000, 10000, 12000, 14000, 16000
// };

/* ====================== 系统频谱校准（关键） ====================== */
/* 这是抑制 400/800/1200/2000 闪烁的决定性手段 */

static const float band_calib[FREQ_BINS] =
{
    0.25f,   // 400
    0.25f,   // 800
    0.25f,   // 1200
    0.25f,   // 2000
    1.3f,   // 3500
    1.3f,   // 6000
    1.3f,   // 10000
    1.3f    // 16000
};

/* ====================== 状态变量 ====================== */

static float band_noise[FREQ_BINS] = {0};      // 每频段噪声底
static uint8_t last_spectrum[FREQ_BINS] = {0};

/* 节奏相关 */
static float lf_env = 0.0f;          // 低频包络
static float agc_peak = 0.08f;

const float freq_resolution = (float)SAMPLE_RATE / FFT_SIZE;

/* ===================================================== */
/* ================== 核心函数 ========================= */
/* ===================================================== */
void map_fft_spectrum(const float32_t *magnitude, uint8_t *spectrum)
{
    /* ==================== 状态量 ==================== */
    static float band_noise[FREQ_BINS] = {0};
    static uint8_t last_spectrum[FREQ_BINS] = {0};
    static float lf_env = 0.0f;         // 低频瞬态包络
    static float agc_peak = 0.08f;

    float band_energy[FREQ_BINS] = {0};
    float band_peak[FREQ_BINS]   = {0};
    uint16_t band_cnt[FREQ_BINS] = {0};

    const float freq_res = (float)SAMPLE_RATE / FFT_SIZE;

    /* =================================================
     * 1?? FFT 全局静音检测（真·安静）
     * ================================================= */
    float fft_energy = 0.0f;
    for (int i = 2; i < FFT_SIZE / 2; i++)
        fft_energy += magnitude[i] * magnitude[i];

    if (fft_energy < 5e-4f)
    {
        memset(spectrum, 0, FREQ_BINS);
        memset(last_spectrum, 0, FREQ_BINS);
        lf_env = 0.0f;
        return;
    }

    /* =================================================
     * 2?? 频段能量统计
     * ================================================= */
    for (int i = 2; i < FFT_SIZE / 2; i++)
    {
        float freq = i * freq_res;
        float mag  = magnitude[i];

        for (uint8_t b = 0; b < FREQ_BINS; b++)
        {
            float low  = (b == 0) ? 0 : freq_bands[b - 1];
            float high = freq_bands[b];

            if (freq > low && freq <= high)
            {
                band_energy[b] += mag * mag;
                band_peak[b] = fmaxf(band_peak[b], mag);
                band_cnt[b]++;
                break;
            }
        }
    }

    /* =================================================
     * 3?? RMS + 建模（低频稳，高频亮）
     * ================================================= */
    for (uint8_t b = 0; b < FREQ_BINS; b++)
    {
        if (band_cnt[b] == 0) continue;

        float rms = sqrtf(band_energy[b] / band_cnt[b]);

        if (b < 4)
            band_energy[b] = rms;                       // 低频：稳
        else
            band_energy[b] = rms * 0.6f + band_peak[b] * 0.4f;
    }

    /* =================================================
     * 4?? 噪声底学习 + Gate（关键）
     * ================================================= */
    float total_energy = 0.0f;

    for (uint8_t b = 0; b < FREQ_BINS; b++)
    {
        if (band_noise[b] < 1e-6f)
            band_noise[b] = band_energy[b];

        /* 只在弱信号下学习 */
        if (band_energy[b] < band_noise[b] * 2.5f)
        {
            band_noise[b] =
                band_noise[b] * 0.998f +
                band_energy[b] * 0.002f;
        }

        float gate = (b < 4) ? 4.0f : 3.0f;

        if (band_energy[b] < band_noise[b] * gate)
            band_energy[b] = 0.0f;

        total_energy += band_energy[b];
    }

    if (total_energy < 0.01f)
    {
        memset(spectrum, 0, FREQ_BINS);
        return;
    }

    /* =================================================
     * 5?? 低频绝对抑制（安静不闪的保险）
     * ================================================= */
    for (uint8_t b = 0; b < 4; b++)
    {
        if (band_energy[b] < 0.02f)
            band_energy[b] = 0.0f;
    }

    /* =================================================
     * 6?? ? 低频瞬态包络（快起快灭）
     * ================================================= */
    float lf_energy = 0.0f;
    for (uint8_t b = 0; b < 4; b++)
        lf_energy += band_energy[b];

    if (lf_energy > lf_env)
        lf_env = lf_energy;     // 立刻跟上鼓点
    else
        lf_env *= 0.5f;         // 1~2 帧内熄灭

    /* =================================================
     * 7?? AGC（只做轻微，不吃节奏）
     * ================================================= */
    float frame_peak = 0.0f;
    for (uint8_t b = 0; b < FREQ_BINS; b++)
        frame_peak = fmaxf(frame_peak, band_energy[b]);

    agc_peak = agc_peak * 0.96f + frame_peak * 0.04f;
    if (agc_peak < 0.05f) agc_peak = 0.05f;

    /* =================================================
     * 8?? LED 映射（低频=鼓点，高频=装饰）
     * ================================================= */
    for (uint8_t b = 0; b < FREQ_BINS; b++)
    {
        float energy = (b < 4) ? lf_env : band_energy[b];

        float norm = energy / agc_peak;
        norm = sqrtf(norm);                 // 视觉非线性
        norm *= band_calib[b];

        if (norm > 1.0f) norm = 1.0f;
        if (norm < 0.02f) norm = 0.0f;

        uint8_t level = (uint8_t)(norm * LED_ROWS);

        /* ? 低频：禁止慢平滑（保留节奏） */
        if (b >= 4)
        {
            if (level > last_spectrum[b])
                level = last_spectrum[b] * 0.6f + level * 0.4f;
            else
                level = last_spectrum[b] * 0.85f;
        }

        if (level > LED_ROWS) level = LED_ROWS;

        spectrum[b] = level;
        last_spectrum[b] = level;
    }
}

void Test_map_fft_spectrum(void)
{
    const float32_t *p_magnitude;
    printf("正在进行多频率FFT测试...\n\n");

    // 测试不同频率
    Test_FFT_With_Signal(1000.0f);
    p_magnitude = FFT_Get_Magnitude();
    map_fft_spectrum(p_magnitude, last_spectrum);
    for(uint8_t band = 0; band < FREQ_BINS; band++)
    {
        PRINT("spectrum: %d", last_spectrum[band]);
    }
    HAL_Delay(1000);

    Test_FFT_With_Signal(2000.0f);
    p_magnitude = FFT_Get_Magnitude();
    map_fft_spectrum(p_magnitude, last_spectrum);
    for(uint8_t band = 0; band < FREQ_BINS; band++)
    {
        PRINT("spectrum: %d", last_spectrum[band]);
    }
    HAL_Delay(1000);

    Test_FFT_With_Signal(5000.0f);
    p_magnitude = FFT_Get_Magnitude();
    map_fft_spectrum(p_magnitude, last_spectrum);
    for(uint8_t band = 0; band < FREQ_BINS; band++)
    {
        PRINT("spectrum: %d", last_spectrum[band]);
    }
    HAL_Delay(1000);

    // 测试边界频率
    Test_FFT_With_Signal(100.0f);
    p_magnitude = FFT_Get_Magnitude();
    map_fft_spectrum(p_magnitude, last_spectrum);
    for(uint8_t band = 0; band < FREQ_BINS; band++)
    {
        PRINT("spectrum: %d", last_spectrum[band]);
    }
    HAL_Delay(1000);

    Test_FFT_With_Signal(15000.0f);
    p_magnitude = FFT_Get_Magnitude();
    map_fft_spectrum(p_magnitude, last_spectrum);
    for(uint8_t band = 0; band < FREQ_BINS; band++)
    {
        PRINT("spectrum: %d", last_spectrum[band]);
    }
}

uint8_t *WS2812B_map_fft_spectrum(const float32_t *magnitude)
{
    map_fft_spectrum(magnitude, last_spectrum);
    for(uint8_t band = 0; band < FREQ_BINS; band++)
    {
        // PRINT("spectrum: %d", last_spectrum[band]);
    }

    return last_spectrum;
}
