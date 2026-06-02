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
#include "spectrum.h"
#include "ui_task.h"
#include "at_driver_test.h"
#include "mqtt_task.h"
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
static FFT_Handle g_fft;

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
/* Definitions for UIProcessTask */
osThreadId_t UIProcessTaskHandle;
const osThreadAttr_t UIProcessTask_attributes = {
  .name = "UIProcessTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for SystemTask */
osThreadId_t SystemTaskHandle;
const osThreadAttr_t SystemTask_attributes = {
  .name = "SystemTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for OTATask */
osThreadId_t OTATaskHandle;
const osThreadAttr_t OTATask_attributes = {
  .name = "OTATask",
  .stack_size = 2048 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for FFT_queue */
osMessageQueueId_t FFT_queueHandle;
const osMessageQueueAttr_t FFT_queue_attributes = {
  .name = "FFT_queue"
};
/* Definitions for GeneralEvent */
osEventFlagsId_t GeneralEventHandle;
const osEventFlagsAttr_t GeneralEvent_attributes = {
  .name = "GeneralEvent"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void StartdataProcessTask(void *argument);
void StartUIProcessTask(void *argument);
void StartSystemTask(void *argument);
void StartOTATask(void *argument);

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
  // dataProcessTaskHandle = osThreadNew(StartdataProcessTask, NULL, &dataProcessTask_attributes);

  /* creation of UIProcessTask */
  // UIProcessTaskHandle = osThreadNew(StartUIProcessTask, NULL, &UIProcessTask_attributes);

  /* creation of SystemTask */
  // SystemTaskHandle = osThreadNew(StartSystemTask, NULL, &SystemTask_attributes);

  /* creation of OTATask */
  OTATaskHandle = osThreadNew(MQTT_Task, NULL, &OTATask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
    /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* Create the event(s) */
  /* creation of GeneralEvent */
  GeneralEventHandle = osEventFlagsNew(&GeneralEvent_attributes);

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
    printf(">>> StartDefaultTask: is run!\r\n");

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
    Ics43434_Frame_t *frame;
    SpectrumData out;
    const float32_t *t_magnitude = NULL;

    register_callback(I2S_DMAxM0Cplt_Callback, 0);
    register_callback(I2S_DMAxM1Cplt_Callback, 1);

    FFT_Init(&g_fft);
    FFT_NoiseStart(&g_fft);

    for (;;)
    {
        if (Ics43434_GetFrame(&frame))
        {
            // printf(">>>StartdataProcessTask: Ics43434_GetFrame is ok\r\n");
            FFT_Process(&g_fft, frame->pcm);

            if (FFT_NoiseGetState(&g_fft) == FFT_NOISE_READY)
            {
                // printf(">>>StartdataProcessTask: FFT_NoiseGetState is ok\r\n");
                t_magnitude = FFT_GetMagnitude(&g_fft);
                if (t_magnitude != NULL)
                {
                    // printf(">>> Ics43434_GetFrame: t_magnitude is not NULL!\r\n");
                    AVP_Process(t_magnitude, &out);
                    taskENTER_CRITICAL();
                    g_spectrum = out;
                    taskEXIT_CRITICAL();
                }
            }
        }
        osDelay(1);
    }
  /* USER CODE END StartdataProcessTask */
}

/* USER CODE BEGIN Header_StartUIProcessTask */
/**
 * @brief Function implementing the UIProcessTask thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartUIProcessTask */
void StartUIProcessTask(void *argument)
{
  /* USER CODE BEGIN StartUIProcessTask */
    UiContext ctx = {
        .current_page = UI_PAGE_STARTUP,
        .tick = 0,
    };

    UiMsg_t msg;

    ui_page_enter(ctx.current_page);

    /* Infinite loop */
    for (;;)
    {
        /* ���� UI ��Ϣ����������*/
        while(osMessageQueueGet(FFT_queueHandle, &msg, NULL, 0) == osOK)
        {
            ui_handle_msg(&ctx, &msg);
        }

        /* ���Ƶ�ǰҳ�� */
        ui_page_draw(&ctx);

        /* UI ���� */
        ctx.tick++;
        vTaskDelay(pdMS_TO_TICKS(30));
    }
  /* USER CODE END StartUIProcessTask */
}

/* USER CODE BEGIN Header_StartSystemTask */
/**
 * @brief Function implementing the SystemTask thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartSystemTask */
void StartSystemTask(void *argument)
{
  /* USER CODE BEGIN StartSystemTask */
    /* Infinite loop */
    for (;;)
    {
        osDelay(1);
    }
  /* USER CODE END StartSystemTask */
}

/* USER CODE BEGIN Header_StartOTATask */
/**
* @brief Function implementing the OTATask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartOTATask */
void StartOTATask(void *argument)
{
  /* USER CODE BEGIN StartOTATask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartOTATask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
static void I2S_DMAxM0Cplt_Callback(void)
{
    // printf("%s\r\n", __func__);
    i2s2_buffer_t *t_buffer = I2S2_Get_Buffer_Addr();
    // printf(">>> I2S_DMAxM0Cplt_Callback: is run\r\n");
    I2S_DMA_Callback(t_buffer[0].i2s2_data);
}

static void I2S_DMAxM1Cplt_Callback(void)
{
    // printf("%s\r\n", __func__);
    i2s2_buffer_t *t_buffer = I2S2_Get_Buffer_Addr();
    // printf(">>> I2S_DMAxM1Cplt_Callback: is run\r\n");
    I2S_DMA_Callback(t_buffer[1].i2s2_data);
}

/* USER CODE END Application */

