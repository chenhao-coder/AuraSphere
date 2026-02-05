#ifndef FFT_TEST_H
#define FFT_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

#include "fft_processing.h"

/**
 * @brief µ•∆µ FFT ≤‚ ‘
 * @param freq ≤‚ ‘∆µ¬ £®Hz£©
 */
void FFT_Test_SingleFrequency(float freq);

/**
 * @brief ∂‡∆µ FFT ≤‚ ‘
 */
void FFT_Test_MultipleFrequencies(void);

#ifdef __cplusplus
}
#endif

#endif /* FFT_TEST_H */
