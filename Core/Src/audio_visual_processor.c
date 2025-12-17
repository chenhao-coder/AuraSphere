#include "audio_visual_processor.h"
#include "ws2812b.h"

#define NOISE_FLOOR 0.03f   // 需要你实际调

static float last_spectrum[FREQ_BINS] = {0};
// 频率分辨率
const float freq_resolution = (float)SAMPLE_RATE / FFT_SIZE;

// 12频段划分
// const uint16_t freq_bands[FREQ_BINS] = {
//     100, 200, 350, 500, 800, 1200, 2000, 3000, 5000, 8000, 12000, 18000
// };


const uint16_t freq_bands[FREQ_BINS] = {
    400, 800, 1200, 2000, 3500, 6000, 10000, 16000
};

static uint8_t spectrum[FREQ_BINS];

void map_fft_spectrum(const float32_t *magnitude, uint8_t *spectrum)
{
    float32_t band_energy[FREQ_BINS] = {0};
    uint16_t band_counts[FREQ_BINS] = {0};

    for(int i = 2; i < FFT_SIZE / 2; i++)
    {
        float freq = i * freq_resolution;

        for(uint8_t band =0; band < FREQ_BINS; band++)
        {
            if(freq <= freq_bands[band])
            {
                band_energy[band] += magnitude[i];
                band_counts[band]++;
                break;
            }
        }
    }

    // 动态范围调整
    static float max_energy = 0.1f;

    // 计算当前帧的最大值
    float current_max = 0.0f;

    for(uint8_t band = 0; band < FREQ_BINS; band++)
    {
        if(band_energy[band] < NOISE_FLOOR)
        {
            band_energy[band] = 0.0f;
        }

        if(band_counts[band] > 0)
        {
            band_energy[band] /= band_counts[band];   // 计算平均值

            if(band < 2)
            {
                band_energy[band] *= 0.3f;  // 降低权重
                band_energy[band] = fmaxf(0.0f, band_energy[band] - 0.02f);
            }
        }

        if(band_energy[band] > current_max)
        {
            current_max = band_energy[band];
        }
    }

    // 智能最大值跟踪
    if(current_max > max_energy * 2.0f)
    {
        max_energy = current_max * 0.7f + max_energy * 0.3f;
    }
    else if(current_max > max_energy) 
    {
        // 正常上升
        max_energy = max_energy * 0.8f + current_max * 0.2f;
    } 
    else 
    {
        // 下降时更慢
        max_energy = max_energy * 0.95f + current_max * 0.05f;
    }

    if(max_energy < 0.05f) 
    {
        memset(spectrum, 0, FREQ_BINS);
        return;
    }

    // 映射到LED高度
    for(uint8_t band = 0; band < FREQ_BINS; band++)
    {
        float normalized = band_energy[band] / max_energy;
        
        // 非线性映射
        normalized = sqrtf(normalized); // 平方根让响应更平缓
        
        // 低频段额外抑制
        if(band < 2) 
        {
            normalized = powf(normalized, 1.8f);
        }
         // 应用飞线性映射(平方根让幅值更灵敏)
        // normalized = sqrtf(normalized);

        uint8_t level = (uint8_t)(normalized * LED_ROWS);
            
        // 更强的平滑
        float smooth_factor = (band < 2) ? 0.8f : 0.6f;
        level = last_spectrum[band] * (1.0f - smooth_factor) + level * smooth_factor;
        
        // 设置显示阈值
        if(level < 2 || normalized < 0.2f) level = 0;
        if(level > LED_ROWS) level = LED_ROWS;

        spectrum[band] = level;
        last_spectrum[band] = level;
    }
}

void Test_map_fft_spectrum(void)
{
    const float32_t *p_magnitude;
    printf("正在进行多频率FFT测试...\n\n");

    // 测试不同频率
    Test_FFT_With_Signal(1000.0f);
    p_magnitude = FFT_Get_Magnitude();
    map_fft_spectrum(p_magnitude, spectrum);
    for(uint8_t band = 0; band < FREQ_BINS; band++)
    {
        PRINT("spectrum: %d", spectrum[band]);
    }
    HAL_Delay(1000);

    Test_FFT_With_Signal(2000.0f);
    p_magnitude = FFT_Get_Magnitude();
    map_fft_spectrum(p_magnitude, spectrum);
    for(uint8_t band = 0; band < FREQ_BINS; band++)
    {
        PRINT("spectrum: %d", spectrum[band]);
    }
    HAL_Delay(1000);

    Test_FFT_With_Signal(5000.0f);
    p_magnitude = FFT_Get_Magnitude();
    map_fft_spectrum(p_magnitude, spectrum);
    for(uint8_t band = 0; band < FREQ_BINS; band++)
    {
        PRINT("spectrum: %d", spectrum[band]);
    }
    HAL_Delay(1000);

    // 测试边界频率
    Test_FFT_With_Signal(100.0f);
    p_magnitude = FFT_Get_Magnitude();
    map_fft_spectrum(p_magnitude, spectrum);
    for(uint8_t band = 0; band < FREQ_BINS; band++)
    {
        PRINT("spectrum: %d", spectrum[band]);
    }
    HAL_Delay(1000);

    Test_FFT_With_Signal(15000.0f);
    p_magnitude = FFT_Get_Magnitude();
    map_fft_spectrum(p_magnitude, spectrum);
    for(uint8_t band = 0; band < FREQ_BINS; band++)
    {
        PRINT("spectrum: %d", spectrum[band]);
    }
}

uint8_t *WS2812B_map_fft_spectrum(const float32_t *magnitude)
{
    map_fft_spectrum(magnitude, spectrum);
    for(uint8_t band = 0; band < FREQ_BINS; band++)
    {
        PRINT("spectrum: %d", spectrum[band]);
    }

    return spectrum;
}

