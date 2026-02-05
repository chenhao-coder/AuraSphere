#ifndef AUDIO_VISUAL_PROCESSOR_H
#define AUDIO_VISUAL_PROCESSOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "arm_math.h"

/* ===================== 参数定义 ===================== */

#define FREQ_BINS   16      // 16 列灯 = 16 个频段

typedef struct
{
    uint8_t bar[FREQ_BINS];     // 每个频段的柱高
    uint8_t peak[FREQ_BINS];    // 峰值
} SpectrumData;

/* ===================== 接口 ===================== */

/**
 * @brief 初始化音频可视化处理器
 */
void AVP_Init(void);

/**
 * @brief 处理 FFT 幅度谱，生成可视化数据
 * @param magnitude FFT 输出的幅度谱（FFT_SIZE/2）
 * @param out       输出的频谱数据（柱高 + 峰值）
 */
void AVP_Process(const float32_t *magnitude, SpectrumData *out);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_VISUAL_PROCESSOR_H */
