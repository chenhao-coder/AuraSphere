#include "fft_test.h"
#include <stdio.h>
#include <math.h>
#include "arm_math.h"
#include "main.h"

#define PI 3.14159265358979f

/* ================= 内部工具函数 ================= */

/* 生成测试正弦波（float） */
static void generate_test_signal(float32_t *buffer, float freq)
{
    for (int i = 0; i < FFT_SIZE; i++)
    {
        buffer[i] = arm_sin_f32(2 * PI * freq * i / SAMPLE_RATE);
    }
}

/* float → 模拟 24bit ADC */
static void float_to_adc24(const float32_t *in, int32_t *out)
{
    for (int i = 0; i < FFT_SIZE; i++)
    {
        out[i] = (int32_t)(in[i] * 8388607.0f);
    }
}

/* 频谱分析（仅 debug 使用） */
static void analyze_spectrum(const float32_t *mag)
{
    float freq_res = (float)SAMPLE_RATE / FFT_SIZE;

    float32_t max_val;
    uint32_t  max_bin;

    arm_max_f32(mag, FFT_SIZE / 2, &max_val, &max_bin);

    float dominant_freq = max_bin * freq_res;

    float low = 0, mid = 0, high = 0;

    for (int i = 0; i < FFT_SIZE / 2; i++)
    {
        float freq = i * freq_res;

        if (freq < 300)
            low += mag[i];
        else if (freq < 3000)
            mid += mag[i];
        else
            high += mag[i];
    }

    printf("主频: %.1f Hz | 能量: 低=%.3f 中=%.3f 高=%.3f\n",
           dominant_freq, low, mid, high);
}

/* ================= 对外测试接口 ================= */

void FFT_Test_SingleFrequency(float freq)
{
    printf("\n=== FFT 单频测试 ===\n");
    printf("测试频率: %.1f Hz\n", freq);
    printf("采样率: %d Hz, FFT点数: %d\n", SAMPLE_RATE, FFT_SIZE);
    printf("频率分辨率: %.2f Hz\n",
           (float)SAMPLE_RATE / FFT_SIZE);

    static FFT_Handle fft;
    static int inited = 0;

    if (!inited)
    {
        FFT_Init(&fft);
        inited = 1;
    }

    float32_t test_signal[FFT_SIZE];
    int32_t   adc_data[FFT_SIZE];

    generate_test_signal(test_signal, freq);
    float_to_adc24(test_signal, adc_data);

    FFT_Process(&fft, adc_data);

    const float32_t *mag = FFT_GetMagnitude(&fft);

    analyze_spectrum(mag);

    float freq_res = (float)SAMPLE_RATE / FFT_SIZE;
    uint32_t expect_bin = (uint32_t)(freq / freq_res);

    printf("预期 bin: %lu (%.1f Hz)\n",
           expect_bin, expect_bin * freq_res);

    printf("峰值附近 bin:\n");
    for (int i = (int)expect_bin - 5; i <= (int)expect_bin + 5; i++)
    {
        if (i >= 0 && i < FFT_SIZE / 2)
        {
            printf("  bin %3d | %6.1f Hz | mag %.6f\n",
                   i, i * freq_res, mag[i]);
        }
    }

    printf("=== 测试结束 ===\n");
}

void FFT_Test_MultipleFrequencies(void)
{
    printf("\n=== FFT 多频测试 ===\n");

    FFT_Test_SingleFrequency(100.0f);
    HAL_Delay(1000);

    FFT_Test_SingleFrequency(1000.0f);
    HAL_Delay(1000);

    FFT_Test_SingleFrequency(2000.0f);
    HAL_Delay(1000);

    FFT_Test_SingleFrequency(5000.0f);
    HAL_Delay(1000);

    FFT_Test_SingleFrequency(15000.0f);
    HAL_Delay(1000);

    printf("=== 多频测试完成 ===\n");
}
