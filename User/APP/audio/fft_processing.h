#ifndef FFT_PROCESSING_H
#define FFT_PROCESSING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "arm_math.h"

/* ================= 配置 ================= */

#define FFT_SIZE        1024        // 必须是 2 的幂
#define SAMPLE_RATE     44100

/* ================= 噪声状态 ================= */

typedef enum
{
    FFT_NOISE_IDLE = 0,
    FFT_NOISE_CALIBRATING,
    FFT_NOISE_READY,
} FFT_NoiseState;

/* ================= FFT 句柄 ================= */

typedef struct
{
    arm_rfft_fast_instance_f32 rfft;

    float32_t input[FFT_SIZE];
    float32_t output[FFT_SIZE];
    float32_t magnitude[FFT_SIZE / 2];

    /* 高通滤波状态 */
    float32_t hpf_x_prev;
    float32_t hpf_y_prev;

    /* 噪声相关 */
    FFT_NoiseState noise_state;
    float32_t noise_floor[FFT_SIZE / 2];
    float32_t noise_accum[FFT_SIZE / 2];
    uint16_t noise_count;

} FFT_Handle;

/* ================= 接口 ================= */

/**
 * @brief 初始化 FFT 模块
 */
void FFT_Init(FFT_Handle *h);

/**
 * @brief 执行 FFT 处理
 * @param input  原始 ADC / PCM 数据（长度 FFT_SIZE）
 */
void FFT_Process(FFT_Handle *h, const int32_t *input);

/**
 * @brief 获取幅值谱（只读）
 */
const float32_t *FFT_GetMagnitude(FFT_Handle *h);

/**
 * @brief 启动噪声校准
 */
void FFT_NoiseStart(FFT_Handle *h);

/**
 * @brief 获取噪声状态
 */
FFT_NoiseState FFT_NoiseGetState(FFT_Handle *h);

#ifdef __cplusplus
}
#endif

#endif /* FFT_PROCESSING_H */
