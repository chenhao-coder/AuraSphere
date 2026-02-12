/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    i2s.h
  * @brief   This file contains all the function prototypes for
  *          the i2s.c file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __I2S_H__
#define __I2S_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

extern I2S_HandleTypeDef hi2s2;

/* USER CODE BEGIN Private defines */

#define I2S2_SAMPLE_RATE        44100   
#define I2S2_BUFFER_SIZE        1024 * 2    


typedef void (* i2s_dma_m0_m1_callback_func)(void);

typedef struct {
    int32_t i2s2_data[I2S2_BUFFER_SIZE + 2];
}i2s2_buffer_t;
/* USER CODE END Private defines */

void MX_I2S2_Init(void);

/* USER CODE BEGIN Prototypes */
HAL_StatusTypeDef HAL_I2S_Receive_DMA_modiy(I2S_HandleTypeDef *hi2s, uint16_t *pData0, uint16_t *pdata1, uint16_t Size);
i2s2_buffer_t * I2S2_Get_Buffer_Addr(void);
int register_callback(i2s_dma_m0_m1_callback_func func, uint8_t mode);
/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __I2S_H__ */

