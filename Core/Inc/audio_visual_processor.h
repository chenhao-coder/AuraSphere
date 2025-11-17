#ifndef AUDIO_VISUAL_PROCESSOR_H
#define AUDIO_VISUAL_PROCESSOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdio.h>
#include "fft_processing.h"

#define FREQ_BINS       12
#define LED_ROWS        20

void map_fft_spectrum(const float32_t *magnitude, uint8_t *spectrum);
void Test_map_fft_spectrum(void);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_VISUAL_PROCESSOR_H