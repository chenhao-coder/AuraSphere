#ifndef ICS43434_H
#define ICS43434_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "i2s.h"

// 音频配置
#define ICS43434_SAMPLE_RATE        16000   // 16KHz采样频率
#define ICS43434_BUFFER_SIZE        512     // 音频缓冲区大小
#define ICS43434_DMA_BUFFER_SIZE    (ICS43434_BUFFER_SIZE * 2)  // DMA双缓冲区

// ICS43434数据格式 (24位数据在32位帧中)
typedef struct {
    int32_t ics43434_data[ICS43434_BUFFER_SIZE];
    volatile uint8_t data_ready;
}ics43434_buffer_t;

// 函数声明
void ICS43434_Init(void);
void ICS43434_Start(void);
void ICS43434_Stop(void);
uint8_t ICS43434_IsDataReady(void);
void ICS43434_GetData(int32_t *buffer, uint16_t size);
void ICS43434_ProcessData(void);

// void HAL_I2S_RxHalfCpltCallback(I2S_HandleTypeDef *hi2s);
// void HAL_I2S_RxCpltCallback(I2S_HandleTypeDef *hi2s);
// void HAL_I2S_ErrorCallback(I2S_HandleTypeDef *hi2s);

#ifdef __cplusplus
}
#endif

#endif // ICS43434_H