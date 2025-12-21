#ifndef AUDIO_VISUAL_PROCESSOR_H
#define AUDIO_VISUAL_PROCESSOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdio.h>
#include "fft_processing.h"

#define FREQ_BINS       8
#define LED_ROWS        8
#define LED_COLS        8

#define SMOOTH_FACTOR 0.9f  // LED平滑系数 0~1
#define MIN_MAX_ENERGY 0.0001f
#define SENSITIVITY 1.5f    // 可调节灵敏度

void map_fft_spectrum(const float32_t *magnitude, uint8_t *spectrum);
void Test_map_fft_spectrum(void);
uint8_t *WS2812B_map_fft_spectrum(const float32_t *magnitude);
void map_fft_spectrum_test(const float32_t *magnitude, uint8_t *spectrum);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_VISUAL_PROCESSOR_H