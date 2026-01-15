#ifndef FFT_PROCESSING_H
#define FFT_PROCESSING_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "arm_math.h"

#define SAMPLE_RATE     44100   // 采样率
#define FFT_SIZE        (512 * 2)    // FFT点数

// 函数声明
void FFT_Init(void);
void Process_Data(int32_t *data);
void Analyze_Frequency_Spectrum(float32_t *magnitude);
void Generate_Test_Signal(float32_t *buffer, float freq, uint32_t size);
void Test_FFT_With_Signal(float test_freq);  // 新增测试函数
void Test_FFT_Multiple_Frequencies(void);
void compute_rfft_magnitude(float32_t *fft_out, float32_t *mag, uint32_t fft_size);
float32_t *FFT_Get_Magnitude(void);
void Noise_Reset(void);
void Noise_Collect(float *magnitude);
int is_Noise_Calibrated(void);

#ifdef __cplusplus
}
#endif

#endif // FFT_PROCESSING_H