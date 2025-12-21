/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name          : freertos.c
 * Description        : Code for freertos applications
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <stdint.h>
#include "arm_math.h"
#include "i2s.h"
#include "queue.h"
#include "ics43434.h"
#include "fft_processing.h"
#include "audio_visual_processor.h"
#include "ws2812b.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
static void I2S_DMAxM0Cplt_Callback(void);
static void I2S_DMAxM1Cplt_Callback(void);
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for dataProcessTask */
osThreadId_t dataProcessTaskHandle;
const osThreadAttr_t dataProcessTask_attributes = {
  .name = "dataProcessTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for FFT_queue */
osMessageQueueId_t FFT_queueHandle;
const osMessageQueueAttr_t FFT_queue_attributes = {
  .name = "FFT_queue"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void StartdataProcessTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
    /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
    /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
    /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of FFT_queue */
  FFT_queueHandle = osMessageQueueNew (3, sizeof(i2s2_buffer_t *), &FFT_queue_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
    /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of dataProcessTask */
  dataProcessTaskHandle = osThreadNew(StartdataProcessTask, NULL, &dataProcessTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
    /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
    /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
 * @brief  Function implementing the defaultTask thread.
 * @param  argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
    register_callback(I2S_DMAxM0Cplt_Callback, 0);
    register_callback(I2S_DMAxM1Cplt_Callback, 1);
    /* Infinite loop */
    for (;;)
    {
        osDelay(1);
    }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_StartdataProcessTask */
/**
* @brief Function implementing the dataProcessTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartdataProcessTask */
void StartdataProcessTask(void *argument)
{
  /* USER CODE BEGIN StartdataProcessTask */
  /* Infinite loop */
  i2s2_buffer_t * t_i2s2_buffer = NULL;
  int32_t *t_output_ptr = NULL;
  float32_t *t_magnitude = NULL;
  uint8_t *spectrum = NULL;

  for(;;)
  {
    osMessageQueueGet(FFT_queueHandle, &t_i2s2_buffer, NULL, osWaitForever);
    Ics43434_Out_Process(t_i2s2_buffer->i2s2_data);
    t_i2s2_buffer->data_ready = 0;
    t_output_ptr = Ics43434_Get_Output_Ptr();
    Process_Data(t_output_ptr);
    t_magnitude = FFT_Get_Magnitude();
    // for(int i = 0; i < 512; i++) 
    // {
    //     PRINT("magnitude: %f", t_magnitude[i]);
    // }
    spectrum = WS2812B_map_fft_spectrum(t_magnitude);
    LED_DisplaySpectrum(spectrum);
  }
  /* USER CODE END StartdataProcessTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
static void I2S_DMAxM0Cplt_Callback(void)
{
    // printf("%s\r\n", __func__);
    i2s2_buffer_t *t_buffer = I2S2_Get_Buffer_Addr();
    if(t_buffer[0].data_ready != 0)
    {
        printf("error: There are unprocessed data in i2s2_buffer[0]\n");
        return;
    }

    t_buffer[0].data_ready = 1;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    i2s2_buffer_t *buffer_to_send = &t_buffer[0];
    if(xQueueSendFromISR(FFT_queueHandle, &buffer_to_send, &xHigherPriorityTaskWoken) != pdTRUE)
    {
        printf("FFT_queueHandle is full\n");
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static void I2S_DMAxM1Cplt_Callback(void)
{
    // printf("%s\r\n", __func__);
    i2s2_buffer_t *t_buffer = I2S2_Get_Buffer_Addr();
    if(t_buffer[1].data_ready != 0)
    {
        printf("error: There are unprocessed data in i2s2_buffer[1]\n");
        return;
    }

    t_buffer[1].data_ready = 1;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    i2s2_buffer_t *buffer_to_send = &t_buffer[1];
    if(xQueueSendFromISR(FFT_queueHandle, &buffer_to_send, &xHigherPriorityTaskWoken) != pdTRUE)
    {
        printf("FFT_queueHandle is full\n");
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* USER CODE END Application */

