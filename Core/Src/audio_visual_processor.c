#include "audio_visual_processor.h"
#include "ws2812b.h"

/* ====================== 参数配置区 ====================== */

#define NOISE_LEARN_RATE     0.002f     // 噪声底学习速率
#define NOISE_GATE_GAIN     3.0f       // 相对噪声门限（≈ +9.5 dB）
#define SILENCE_FFT_ENERGY  0.0005f    // FFT 全局静音阈值

#define AGC_UP_RATE         0.30f
#define AGC_DOWN_RATE       0.97f

#define MID_KILL_THRESHOLD  0.35f      // 中频绝对熄灭阈值

#define AMPLITUDE             50      //声音强度调整倍率（柱状高度倍率）
#define MATRIX_SIDE           8         //每个矩阵的边长

static uint8_t old_band_energy[FREQ_BINS];
static uint8_t peak[FREQ_BINS];
unsigned long peek_decay_time;
unsigned long color_time;
unsigned long change_color_time;

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
    0.35f,   // 400 Hz   （低频压一点，防常亮）
    0.55f,   // 800 Hz
    0.75f,   // 1200 Hz
    0.95f,   // 2000 Hz  （人声基准）
    1.20f,   // 3500 Hz
    1.45f,   // 6000 Hz
    1.75f,   // 10000 Hz
    2.10f    // 16000 Hz （只补偿，不夸张）
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
                band_energy[b] += mag;
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

    // TODO: 什么是AGC
    /* =================================================
     * 7?? AGC（只做轻微，不吃节奏）
     * ================================================= */
    /* ================= AGC ================= */
    float frame_peak = 0.0f;
    for (uint8_t b = 0; b < FREQ_BINS; b++)
        frame_peak = fmaxf(frame_peak, band_peak[b]);

    if (frame_peak > agc_peak)
        agc_peak = agc_peak * 0.6f + frame_peak * 0.4f;
    else
        agc_peak *= 0.98f;

    if (agc_peak < 0.02f)
        agc_peak = 0.02f;

    /* ================= LED 映射 ================= */
    for (uint8_t b = 0; b < FREQ_BINS; b++)
    {
        float energy;
        if (b < 4)
            energy = band_peak[b];      // ★ 关键：低频用峰值
        else
            energy = band_energy[b];
        float norm = energy / agc_peak;

        if (b < 2)
        {
            PRINT("norm: %f", norm);
            if (norm < 0.5f) norm = 0.0f;
            else norm = norm * norm;
        }
        else
        {
            norm = sqrtf(norm);
        }

        norm *= band_calib[b];
        if (norm > 1.3f) norm = 1.3f;

        uint8_t level = (uint8_t)(norm * LED_ROWS / 1.3f);

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


void map_fft_spectrum_test(const float32_t *magnitude, uint8_t *spectrum)
{
    /* ==================== 状态量 ==================== */
    static float band_env[FREQ_BINS] = {0};       // 包络（时间平滑）
    static float peak_hold[FREQ_BINS] = {0};      // 峰值保持

    static float agc_peak = 0.08f;

    float band_energy[FREQ_BINS] = {0};
    float band_peak[FREQ_BINS]   = {0};
    uint16_t band_cnt[FREQ_BINS] = {0};

    const float freq_res = (float)SAMPLE_RATE / FFT_SIZE;

    /* =================================================
     * 1. 频段能量统计
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
                band_energy[b] += mag;
                band_peak[b] = fmaxf(band_peak[b], mag);
                band_cnt[b]++;
                break;
            }
        }
    }

    /* =================================================
     * 2. RMS + 低频稳 / 高频亮
     * ================================================= */
    for (uint8_t b = 0; b < FREQ_BINS; b++)
    {
        if (band_cnt[b] == 0) continue;

        float rms = sqrtf(band_energy[b] / band_cnt[b]);

        if (b < 4)
            band_energy[b] = rms;
        else
            band_energy[b] = rms * 0.6f + band_peak[b] * 0.4f;
    }

    /* =================================================
     * 3. AGC（你原逻辑保留）
     * ================================================= */
    float frame_peak = 0.0f;
    for (uint8_t b = 0; b < FREQ_BINS; b++)
        frame_peak = fmaxf(frame_peak, band_peak[b]);

    if (frame_peak > agc_peak)
        agc_peak = agc_peak * 0.6f + frame_peak * 0.4f;
    else
        agc_peak *= 0.98f;

    if (agc_peak < 0.02f)
        agc_peak = 0.02f;

    /* =================================================
     * 4. 显示建模（核心灯效层）
     * ================================================= */

    #define ATTACK       0.35f
    #define RELEASE      0.90f
    #define LF_ATTACK    0.55f
    #define LF_RELEASE   0.85f
    #define PEAK_FALL    0.92f

    for (uint8_t b = 0; b < FREQ_BINS; b++)
    {
        float energy;

        if (b < 4)
            energy = band_peak[b];   // 低频用峰值
        else
            energy = band_energy[b];

        float raw = energy / agc_peak;

        /* ========== Attack / Release ========== */
        if (b < 3)
        {
            if (raw > band_env[b])
                band_env[b] = band_env[b] * (1.0f - LF_ATTACK) + raw * LF_ATTACK;
            else
                band_env[b] *= LF_RELEASE;
        }
        else
        {
            if (raw > band_env[b])
                band_env[b] = band_env[b] * (1.0f - ATTACK) + raw * ATTACK;
            else
                band_env[b] *= RELEASE;
        }

        float norm = band_env[b];

        /* 你的原校准 */
        norm *= band_calib[b];

        if (norm > 1.3f) norm = 1.3f;
        if (norm < 0.0f) norm = 0.0f;

        uint8_t level = (uint8_t)(norm * LED_ROWS / 1.3f);

        /* ========== 峰值保持 ========== */
        if (level > peak_hold[b])
            peak_hold[b] = level;
        else
            peak_hold[b] *= PEAK_FALL;

        uint8_t peak_level = (uint8_t)(peak_hold[b]);

        if (peak_level >= LED_ROWS)
            peak_level = LED_ROWS - 1;

        spectrum[b] = level;
        last_spectrum[b] = level;
    }
}
void magnitude_to_ws2812b(const float32_t *magnitude)
{
    static float env[FREQ_BINS] = {0};

    float band_energy[FREQ_BINS] = {0};
    uint16_t band_cnt[FREQ_BINS] = {0};

    const float freq_res = (float)SAMPLE_RATE / FFT_SIZE;

    /* ============ 1. 分频段 ============ */
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
                band_energy[b] += mag;
                band_cnt[b]++;
                break;
            }
        }
    }

    for(int band = 0; band < FREQ_BINS; band++)
    {
        // 根据倍率缩放条形图高度
        int bar_height = band_energy[band] / AMPLITUDE;
        // PRINT("bar_height: %d", bar_height);
        if(bar_height > MATRIX_SIDE) bar_height = MATRIX_SIDE;
        // 旧高度与新高度值平均一下
        bar_height = ((old_band_energy[band] * 1) + bar_height) / 2;

        if(bar_height > peak[band]) 
        {
            peak[band] = bar_height;
        }
        old_band_energy[band] = bar_height;
        
    }
    
    // 10毫秒变换一次颜色
    if((HAL_GetTick() - change_color_time) >= 10)
    {
        color_time++;
        change_color_time = HAL_GetTick();
    }
}

uint8_t *WS2812B_map_fft_spectrum(const float32_t *magnitude)
{
    magnitude_to_ws2812b(magnitude);
    for(uint8_t band = 0; band < FREQ_BINS; band++)
    {
        // PRINT("spectrum: %d", last_spectrum[band]);
    }

    return old_band_energy;
}

