#include "audio_visual_processor.h"

// 频率分辨率
const float freq_resolution = (float)SAMPLE_RATE / FFT_SIZE;

// 12频段划分
const uint16_t freq_bands[FREQ_BINS] = {
    100, 200, 350, 500, 800, 1200, 2000, 3000, 5000, 8000, 12000, 18000
};

static uint8_t spectrum[FREQ_BINS];

void map_fft_spectrum(const float32_t *magnitude, uint8_t *spectrum)
{
    float32_t band_energy[FREQ_BINS] = {0};
    uint16_t band_counts[FREQ_BINS] = {0};

    for(int i = 0; i < FFT_SIZE / 2; i++)
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
    float current_max = 0.1f;

    for(uint8_t band = 0; band < FREQ_BINS; band++)
    {
        if(band_counts[band] > 0)
        {
            band_energy[band] /= band_counts[band];   // 计算平均值
        }

        if(band_energy[band] > current_max)
        {
            current_max = band_energy[band];
        }
    }

    // 更新最大值(带衰减)
    if(current_max > max_energy)
    {
        max_energy = current_max;
    }
    else
    {
        max_energy = max_energy * 0.995f + current_max * 0.005f;    // 缓慢衰减
    }

    // 防止除零
    if(max_energy < 0.0001f)
    {
        max_energy = 0.0001f;
    }

    // 映射到LED高度
    for(uint8_t band = 0; band < FREQ_BINS; band++)
    {
        float normalized = band_energy[band] / max_energy;

         // 应用飞线性映射(平方根让幅值更灵敏)
        normalized = sqrtf(normalized);

        float sensitivity = 1.5f;   // 可调节灵敏度
        uint8_t level = (uint8_t)(normalized * LED_ROWS * sensitivity);

        spectrum[band] = (level > LED_ROWS) ? LED_ROWS : level;
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

