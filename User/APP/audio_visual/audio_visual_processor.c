#include "audio_visual_processor.h"
#include "fft_processing.h"
#include <string.h>

/* ===================== 可调参数 ===================== */

#define AMPLITUDE_SCALE    50      // 声音强度缩放
#define MATRIX_HEIGHT      8       // 矩阵高度（8 行）

/* ===================== 频段定义（Hz） ===================== */

static const uint16_t freq_bands[FREQ_BINS] =
{
     80,    // 0  超低频
    150,    // 1
    250,    // 2
    400,    // 3
    630,    // 4
    1000,   // 5  人声起点
    1600,   // 6
    2500,   // 7
    4000,   // 8
    6300,   // 9
    8000,   // 10
    10000,  // 11
    12000,  // 12
    14000,  // 13
    16000,  // 14
    18000   // 15
};

/* ===================== 内部状态 ===================== */

static uint8_t last_bar[FREQ_BINS];

/* ===================== 接口实现 ===================== */

void AVP_Init(void)
{
    memset(last_bar, 0, sizeof(last_bar));
}

void AVP_Process(const float32_t *magnitude, SpectrumData *out)
{
    float band_energy[FREQ_BINS] = {0};
    uint16_t band_count[FREQ_BINS] = {0};

    float freq_resolution = (float)SAMPLE_RATE / FFT_SIZE;

    /* ---------- 1. FFT bin → 频段能量 ---------- */
    for (int i = 2; i < FFT_SIZE / 2; i++)
    {
        float freq = i * freq_resolution;
        float mag  = magnitude[i];

        for (int b = 0; b < FREQ_BINS; b++)
        {
            float low  = (b == 0) ? 0.0f : freq_bands[b - 1];
            float high = freq_bands[b];

            if (freq > low && freq <= high)
            {
                band_energy[b] += mag;
                band_count[b]++;
                break;
            }
        }
    }

    /* ---------- 2. 能量 → 柱高（含平滑 & 峰值） ---------- */
    for (int b = 0; b < FREQ_BINS; b++)
    {
        uint8_t height = band_energy[b] / AMPLITUDE_SCALE;

        if (height > MATRIX_HEIGHT)
            height = MATRIX_HEIGHT;

        /* 一阶平滑 */
        height = (last_bar[b] + height) >> 1;

        out->bar[b] = height;

        /* 峰值处理 */
        if (height > out->peak[b])
        {
            out->peak[b] = height;
        }
        else if (out->peak[b] > 0)
        {
            out->peak[b]--;
        }

        last_bar[b] = height;
    }
}
