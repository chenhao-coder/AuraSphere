#include "fft_processing.h"
#include <string.h>
#include <math.h>

/* ================= 参数 ================= */

#define HPF_ALPHA                   0.995f
#define NOISE_GAIN                  2.0f
#define NOISE_CALIBRATION_FRAMES    100

#define PI 3.14159265358979f

/* ================= 内部函数 ================= */

static void apply_hann_window(float32_t *buf)
{
    for (int i = 0; i < FFT_SIZE; i++)
    {
        buf[i] *= 0.5f * (1.0f - arm_cos_f32(2 * PI * i / (FFT_SIZE - 1)));
    }
}

static void compute_magnitude(const float32_t *fft_out, float32_t *mag)
{
    mag[0] = fabsf(fft_out[0]);

    for (uint32_t i = 1; i < FFT_SIZE / 2; i++)
    {
        float32_t real = fft_out[2 * i];
        float32_t imag = fft_out[2 * i + 1];
        mag[i] = sqrtf(real * real + imag * imag);
    }
}

/* ================= 接口实现 ================= */

void FFT_Init(FFT_Handle *h)
{
    if (!h) return;

    memset(h, 0, sizeof(FFT_Handle));

    arm_rfft_fast_init_f32(&h->rfft, FFT_SIZE);

    h->noise_state = FFT_NOISE_IDLE;
}

void FFT_Process(FFT_Handle *h, const int32_t *input)
{
    if (!h || !input) return;

    float32_t mean = 0.0f;

    /* ========== 1. ADC → float ========== */
    for (int i = 0; i < FFT_SIZE; i++)
    {
        h->input[i] = (float32_t)input[i] / 8388608.0f; // 24bit
    }

    /* ========== 2. DC 去除 ========== */
    arm_mean_f32(h->input, FFT_SIZE, &mean);
    for (int i = 0; i < FFT_SIZE; i++)
    {
        h->input[i] -= mean;
    }

    /* ========== 3. 一阶高通 ========== */
    for (int i = 0; i < FFT_SIZE; i++)
    {
        float32_t y = h->input[i]
                    - h->hpf_x_prev
                    + HPF_ALPHA * h->hpf_y_prev;

        h->hpf_x_prev = h->input[i];
        h->hpf_y_prev = y;
        h->input[i] = y;
    }

    /* ========== 4. 窗函数 ========== */
    apply_hann_window(h->input);

    /* ========== 5. FFT ========== */
    arm_rfft_fast_f32(&h->rfft, h->input, h->output, 0);

    /* ========== 6. 幅值谱 ========== */
    compute_magnitude(h->output, h->magnitude);

    /* ========== 7. 噪声处理 ========== */
    if (h->noise_state == FFT_NOISE_CALIBRATING)
    {
        for (int i = 0; i < FFT_SIZE / 2; i++)
        {
            h->noise_accum[i] += h->magnitude[i];
        }

        h->noise_count++;

        if (h->noise_count >= NOISE_CALIBRATION_FRAMES)
        {
            for (int i = 0; i < FFT_SIZE / 2; i++)
            {
                h->noise_floor[i] = h->noise_accum[i] / h->noise_count;
            }
            h->noise_state = FFT_NOISE_READY;
        }
    }

    if (h->noise_state == FFT_NOISE_READY)
    {
        for (int i = 0; i < FFT_SIZE / 2; i++)
        {
            float32_t clean =
                h->magnitude[i] - h->noise_floor[i] * NOISE_GAIN;

            if (clean < 0) clean = 0;
            h->magnitude[i] = clean;
        }
    }
}

const float32_t *FFT_GetMagnitude(FFT_Handle *h)
{
    return h ? h->magnitude : NULL;
}

void FFT_NoiseStart(FFT_Handle *h)
{
    if (!h) return;

    memset(h->noise_accum, 0, sizeof(h->noise_accum));
    memset(h->noise_floor, 0, sizeof(h->noise_floor));

    h->noise_count = 0;
    h->noise_state = FFT_NOISE_CALIBRATING;
}

FFT_NoiseState FFT_NoiseGetState(FFT_Handle *h)
{
    return h ? h->noise_state : FFT_NOISE_IDLE;
}














