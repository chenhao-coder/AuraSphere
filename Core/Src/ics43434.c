#include "ics43434.h"
#include <string.h>
#include <stdio.h>

// 音频缓冲区
static ics43434_buffer_t ics43434_buffer[2];    // 双缓冲区
static volatile uint8_t current_buffer = 0;
static volatile uint8_t data_ready_flag = 0;

/**
 * @brief 初始化ICS43434麦克风
 */
void ICS43434_Init(void)
{
    // 初始化音频缓冲区
    memset((void *)ics43434_buffer, 0, sizeof(ics43434_buffer));
    current_buffer = 0;
    data_ready_flag = 0;

    printf("ICS43434 Initialized - Sample Rate: %d Hz\r\n", ICS43434_SAMPLE_RATE);
}

/**
 * @brief 启动音频采集
 */
void ICS43434_Start(void)
{
    // 启动I2S DMA接收
    if(HAL_I2S_Receive_DMA(&hi2s2, (uint16_t *)ics43434_buffer[0].ics43434_data,
                                    ICS43434_DMA_BUFFER_SIZE) != HAL_OK)
    {
        Error_Handler();
    }

    printf("ICS43434 Audio Capture Started\r\n");
}

/**
 * @brief 停止音频采集
 */
void ICS43434_Stop(void)
{
    HAL_I2S_DMAStop(&hi2s2);
    data_ready_flag = 0;
}

/**
 * @brief 检查是否有新的音频数据就绪
 */
uint8_t ICS43434_IsDataReady(void)
{
    return data_ready_flag;
}

/**
 * @brief 获取音频数据
 */
void ICS43434_GetData(int32_t *buffer, uint16_t size)
{
    if(size > ICS43434_BUFFER_SIZE)
    {
        size = ICS43434_BUFFER_SIZE;
    }

    // 复制当前就绪的缓冲区数据
    uint8_t ready_buffer = current_buffer ^ 1;  // 获取已完成采样的缓冲区

    memcpy(buffer,
            (void *)ics43434_buffer[ready_buffer].ics43434_data,
            size * sizeof(int32_t));

    data_ready_flag = 0;    // 清除就绪标志
}

/**
 * @brief DMA半传输完成回调
 */
void ICS43434_HalfTransferCallback(void)
{
    // 前半部分传输完成
    ics43434_buffer[current_buffer ^ 1].data_ready = 1;
    data_ready_flag = 1;
}

/**
 * @brief DMA传输完成回调
 */
void ICS43434_TransferCompleteCallback(void)
{
    ics43434_buffer[current_buffer].data_ready = 1;
    data_ready_flag = 1;
    current_buffer ^= 1;    // 切换当前缓冲区
}

/**
 * @brief 处理音频数据（用于测试）
 */
void ICS43434_ProcessData(void)
{
    if(ICS43434_IsDataReady())
    {
        int32_t t_ics43434_buffer[ICS43434_BUFFER_SIZE];

        // 获取音频数据
        ICS43434_GetData(t_ics43434_buffer, ICS43434_BUFFER_SIZE);

        // 计算音频数据
        int64_t sum_squares = 0;
        for(int i = 0; i < ICS43434_BUFFER_SIZE; i++)
        {
            // ICS43434数据是24位在32位中，需要右移8位获取有效数据
            int32_t sample = t_ics43434_buffer[i] >> 8;
            sum_squares += (int64_t)sample * sample;
        }

        float rms = sqrtf((float)sum_squares / ICS43434_BUFFER_SIZE);

        // 打印音量信息（用于调试）
        static uint32_t counter = 0;
        if(counter++ % 50 == 0)
        {
            printf("Audio RMS: %.1f\r\n", rms);
        }
    }
}

/**
 * @brief I2S DMA半传输完成回调
 */
void HAL_I2S_RxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
    if(hi2s->Instance == SPI2)
    {
        // 处理后半缓冲区数据就绪
        ICS43434_TransferCompleteCallback();
    }

}

/**
  * @brief I2S DMA传输完成回调
  */
// void HAL_I2S_RxCpltCallback(I2S_HandleTypeDef *hi2s)
// {
//     if (hi2s->Instance == SPI2)
//     {
//         // 处理后半缓冲区数据就绪
//         ICS43434_TransferCompleteCallback();
//     }
// }

/**
  * @brief I2S DMA错误回调
  */
void HAL_I2S_ErrorCallback(I2S_HandleTypeDef *hi2s)
{
    if (hi2s->Instance == SPI2) {
        printf("I2S DMA Error!\r\n");
        // 错误处理逻辑
    }
}





