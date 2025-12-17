/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    i2s.c
 * @brief   This file provides code for the configuration
 *          of the I2S instances.
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
/* Includes ------------------------------------------------------------------*/
#include "i2s.h"

/* USER CODE BEGIN 0 */
#include <stdio.h>
#include <string.h>

static i2s2_buffer_t i2s2_buffer[2];
static volatile uint8_t current_buffer = 0;
static volatile uint8_t data_ready_flag = 0;

static i2s_dma_m0_m1_callback_func i2s_dma_m0_callback_func = NULL;
static i2s_dma_m0_m1_callback_func i2s_dma_m1_callback_func = NULL;

static void I2S2_Buffer_Init(void);
/* USER CODE END 0 */

I2S_HandleTypeDef hi2s2;
DMA_HandleTypeDef hdma_spi2_rx;

/* I2S2 init function */
void MX_I2S2_Init(void)
{

  /* USER CODE BEGIN I2S2_Init 0 */

  /* USER CODE END I2S2_Init 0 */

  /* USER CODE BEGIN I2S2_Init 1 */

  /* USER CODE END I2S2_Init 1 */
  hi2s2.Instance = SPI2;
  hi2s2.Init.Mode = I2S_MODE_MASTER_RX;
  hi2s2.Init.Standard = I2S_STANDARD_PHILIPS;
  hi2s2.Init.DataFormat = I2S_DATAFORMAT_32B;
  hi2s2.Init.MCLKOutput = I2S_MCLKOUTPUT_ENABLE;
  hi2s2.Init.AudioFreq = I2S_AUDIOFREQ_48K;
  hi2s2.Init.CPOL = I2S_CPOL_LOW;
  hi2s2.Init.FirstBit = I2S_FIRSTBIT_MSB;
  hi2s2.Init.WSInversion = I2S_WS_INVERSION_DISABLE;
  hi2s2.Init.Data24BitAlignment = I2S_DATA_24BIT_ALIGNMENT_RIGHT;
  hi2s2.Init.MasterKeepIOState = I2S_MASTER_KEEP_IO_STATE_DISABLE;
  if (HAL_I2S_Init(&hi2s2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2S2_Init 2 */
    I2S2_Buffer_Init();
    __HAL_I2S_ENABLE(&hi2s2);

    __HAL_DMA_DISABLE(&hdma_spi2_rx);

    HAL_I2S_Receive_DMA_modiy(&hi2s2,
                                (uint16_t *)i2s2_buffer[0].i2s2_data,
                                (uint16_t *)i2s2_buffer[1].i2s2_data,
                                I2S2_BUFFER_SIZE);

    __HAL_DMA_ENABLE(&hdma_spi2_rx);
    __HAL_DMA_ENABLE_IT(&hdma_spi2_rx, DMA_IT_TC | DMA_IT_DME);

  /* USER CODE END I2S2_Init 2 */

}

void HAL_I2S_MspInit(I2S_HandleTypeDef* i2sHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  if(i2sHandle->Instance==SPI2)
  {
  /* USER CODE BEGIN SPI2_MspInit 0 */

  /* USER CODE END SPI2_MspInit 0 */

  /** Initializes the peripherals clock
  */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SPI2;
    PeriphClkInitStruct.Spi123ClockSelection = RCC_SPI123CLKSOURCE_PLL;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

    /* I2S2 clock enable */
    __HAL_RCC_SPI2_CLK_ENABLE();

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**I2S2 GPIO Configuration
    PC2_C     ------> I2S2_SDI
    PB10     ------> I2S2_CK
    PB12     ------> I2S2_WS
    PC6     ------> I2S2_MCK
    */
    GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* I2S2 DMA Init */
    /* SPI2_RX Init */
    hdma_spi2_rx.Instance = DMA1_Stream1;
    hdma_spi2_rx.Init.Request = DMA_REQUEST_SPI2_RX;
    hdma_spi2_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_spi2_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_spi2_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_spi2_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
    hdma_spi2_rx.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
    hdma_spi2_rx.Init.Mode = DMA_CIRCULAR;
    hdma_spi2_rx.Init.Priority = DMA_PRIORITY_LOW;
    hdma_spi2_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_spi2_rx) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(i2sHandle,hdmarx,hdma_spi2_rx);

  /* USER CODE BEGIN SPI2_MspInit 1 */

  /* USER CODE END SPI2_MspInit 1 */
  }
}

void HAL_I2S_MspDeInit(I2S_HandleTypeDef* i2sHandle)
{

  if(i2sHandle->Instance==SPI2)
  {
  /* USER CODE BEGIN SPI2_MspDeInit 0 */

  /* USER CODE END SPI2_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_SPI2_CLK_DISABLE();

    /**I2S2 GPIO Configuration
    PC2_C     ------> I2S2_SDI
    PB10     ------> I2S2_CK
    PB12     ------> I2S2_WS
    PC6     ------> I2S2_MCK
    */
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_2|GPIO_PIN_6);

    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_10|GPIO_PIN_12);

    /* I2S2 DMA DeInit */
    HAL_DMA_DeInit(i2sHandle->hdmarx);
  /* USER CODE BEGIN SPI2_MspDeInit 1 */

  /* USER CODE END SPI2_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */
static void I2S_DMAM0RxHalfCplt(DMA_HandleTypeDef *hdma)
{
    return;
}

static void I2S_DMAM0Cplt(DMA_HandleTypeDef *hdma)
{
    if(i2s_dma_m0_callback_func != NULL)
    {
        i2s_dma_m0_callback_func();
    }
}

static void I2S_DMAM1RxHalfCplt(DMA_HandleTypeDef *hdma)
{
    return;
}

static void I2S_DMAM1Cplt(DMA_HandleTypeDef *hdma)
{
    if(i2s_dma_m1_callback_func != NULL)
    {
        i2s_dma_m1_callback_func();
    }
}

static void I2S_DMA_Error(DMA_HandleTypeDef *hdma)
{
    printf("%s\r\n", __func__);
}

HAL_StatusTypeDef HAL_I2S_Receive_DMA_modiy(I2S_HandleTypeDef *hi2s, uint16_t *pData0, uint16_t *pdata1, uint16_t Size)
{

    HAL_StatusTypeDef errorcode = HAL_OK;

    if ((pData0 == NULL) || (Size == 0UL))
    {
        return HAL_ERROR;
    }

    if (hi2s->State != HAL_I2S_STATE_READY)
    {
        return HAL_BUSY;
    }

    /* Process Locked */
    __HAL_LOCK(hi2s);

    /* Set state and reset error code */
    hi2s->State = HAL_I2S_STATE_BUSY_RX;
    hi2s->ErrorCode = HAL_I2S_ERROR_NONE;
    hi2s->pRxBuffPtr = pData0;
    hi2s->RxXferSize = Size;
    hi2s->RxXferCount = Size;

    /* Init field not used in handle to zero */
    hi2s->pTxBuffPtr = NULL;
    hi2s->TxXferSize = (uint16_t)0UL;
    hi2s->TxXferCount = (uint16_t)0UL;

    hi2s->hdmarx->XferHalfCpltCallback = I2S_DMAM0RxHalfCplt;

    hi2s->hdmarx->XferCpltCallback = I2S_DMAM0Cplt;

    hi2s->hdmarx->XferM1HalfCpltCallback = I2S_DMAM1RxHalfCplt;

    hi2s->hdmarx->XferM1CpltCallback = I2S_DMAM1Cplt;

    hi2s->hdmarx->XferErrorCallback = I2S_DMA_Error;

    /* Enable the Rx DMA Stream/Channel */
    if (HAL_OK != HAL_DMAEx_MultiBufferStart_IT(hi2s->hdmarx,
                                                (uint32_t)&hi2s->Instance->RXDR,
                                                (uint32_t)hi2s->pRxBuffPtr,
                                                (uint32_t)pdata1,
                                                hi2s->RxXferSize))
    {
        /* Update I2S error code */
        SET_BIT(hi2s->ErrorCode, HAL_I2S_ERROR_DMA);
        hi2s->State = HAL_I2S_STATE_READY;
        errorcode = HAL_ERROR;
        __HAL_UNLOCK(hi2s);
        return errorcode;
    }

    /* Check if the I2S Rx request is already enabled */
    if (HAL_IS_BIT_CLR(hi2s->Instance->CFG1, SPI_CFG1_RXDMAEN))
    {
        /* Enable Rx DMA Request */
        SET_BIT(hi2s->Instance->CFG1, SPI_CFG1_RXDMAEN);
    }

    /* Check if the I2S is already enabled */
    if (HAL_IS_BIT_CLR(hi2s->Instance->CR1, SPI_CR1_SPE))
    {
        /* Enable I2S peripheral */
        __HAL_I2S_ENABLE(hi2s);
    }

    /* Start the transfer */
    SET_BIT(hi2s->Instance->CR1, SPI_CR1_CSTART);

    __HAL_UNLOCK(hi2s);
    return errorcode;
}

static void I2S2_Buffer_Init(void)
{
    memset((void *)i2s2_buffer, 0, sizeof(i2s2_buffer));
    current_buffer = 0;
    data_ready_flag = 0;

    printf("i2s2_buffer Initialized - Sample Rate: %d Hz\r\n", I2S2_SAMPLE_RATE);
}

i2s2_buffer_t * I2S2_Get_Buffer_Addr(void)
{
    return i2s2_buffer;
}

int register_callback(i2s_dma_m0_m1_callback_func func, uint8_t mode)
{
    if(func == NULL)
    {
        return -1;      // failed
    }
    else if(mode == 0)
    {
        i2s_dma_m0_callback_func = func;
    }
    else if(mode == 1)
    {
        i2s_dma_m1_callback_func = func;
    }
    return 0;       // success
}

void i2s_dma_m0_callback(void)
{
    for(int i = 0; i < I2S2_BUFFER_SIZE; i++)
    {
        printf("0x%08lX ", i2s2_buffer[0].i2s2_data[i]);
    }
}

void i2s_dma_m1_callback(void)
{
    for(int i = 0; i < I2S2_BUFFER_SIZE; i++)
    {
        printf("0x%08lX ", i2s2_buffer[1].i2s2_data[i]);
    }
}
/* USER CODE END 1 */
