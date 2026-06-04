#include "at_driver.h"
#include "cmsis_os.h"

#define AT_DEBUG

static  uint8_t     at_rx_dma_buf[AT_RX_BUF_SIZE];
static  uint8_t     at_frame_buf[AT_RX_BUF_SIZE];
static  uint16_t    at_frame_len = 0;
static  uint16_t    at_rx_dma_pos = 0;
static  osSemaphoreId_t    at_resp_sem;                 /* 等待响应的信号 */
static osMutexId_t  at_mutex;                           /* 保护共享资源 */
static  char        at_response[AT_RX_BUF_SIZE];        /* 最新响应内容 */
static  volatile uint16_t at_response_len = 0;

static void AT_RestartRx(UART_HandleTypeDef *huart)
{
    HAL_UARTEx_ReceiveToIdle_DMA(huart, at_rx_dma_buf, AT_RX_BUF_SIZE);
    __HAL_DMA_DISABLE_IT(huart->hdmarx, DMA_IT_HT);
}

static uint32_t AT_EnterCritical(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}

static void AT_ExitCritical(uint32_t primask)
{
    if(primask == 0U)
    {
        __enable_irq();
    }
}

static void AT_ClearResponse(void)
{
    uint32_t primask = AT_EnterCritical();
    at_response_len = 0;
    at_response[0] = '\0';
    AT_ExitCritical(primask);
}

static void AT_AppendResponse(const uint8_t *data, uint16_t size)
{
    uint16_t free_len;

    if((data == NULL) || (size == 0U))
    {
        return;
    }

    if(at_response_len >= (sizeof(at_response) - 1U))
    {
        return;
    }

    free_len = (uint16_t)(sizeof(at_response) - at_response_len - 1U);
    if(size > free_len)
    {
        size = free_len;
    }

    memcpy(&at_response[at_response_len], data, size);
    at_response_len += size;
    at_response[at_response_len] = '\0';
}

static void AT_CopyResponse(char *buffer, uint16_t buffer_size)
{
    uint16_t copy_len;
    uint32_t primask;

    if(buffer_size == 0U)
    {
        return;
    }

    primask = AT_EnterCritical();
    copy_len = at_response_len;
    if(copy_len >= buffer_size)
    {
        copy_len = buffer_size - 1U;
    }

    memcpy(buffer, at_response, copy_len);
    buffer[copy_len] = '\0';
    AT_ExitCritical(primask);
}
void AT_init(void)
{
    at_resp_sem = osSemaphoreNew(1, 0, NULL);
    at_mutex = osMutexNew(NULL);

    /* 启动 DMA + 空闲中断接收 */
    AT_RestartRx(&huart1);
}

#ifdef AT_DEBUG
#include "at_driver_debug.h"
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if(huart->Instance != USART1) return;

    if(Size > AT_RX_BUF_SIZE)
    {
        return;
    }

    if(Size == at_rx_dma_pos)
    {
        return;
    }

    if(Size > at_rx_dma_pos)
    {
        at_frame_len = Size - at_rx_dma_pos;
        AT_AppendResponse(&at_rx_dma_buf[at_rx_dma_pos], at_frame_len);
    }
    else
    {
        at_frame_len = AT_RX_BUF_SIZE - at_rx_dma_pos;
        AT_AppendResponse(&at_rx_dma_buf[at_rx_dma_pos], at_frame_len);
        AT_AppendResponse(at_rx_dma_buf, Size);
    }

    at_rx_dma_pos = Size;

    at_stats.rx_count++;

    osSemaphoreRelease(at_resp_sem);
}
HAL_StatusTypeDef AT_Send(const char *cmd, const char *expect, uint32_t timeout_ms)
{
    if(!cmd || !expect) return HAL_ERROR;

    AT_LOG_INFO("Sending: %s (expect: %s, timeout: %lums)", cmd, expect, timeout_ms);
    at_stats.tx_count++;

    if(osMutexAcquire(at_mutex, timeout_ms) != osOK)
    {
        return HAL_TIMEOUT;
    }

    AT_ClearResponse();
    while(osSemaphoreAcquire(at_resp_sem, 0) == osOK);

    char full_cmd[512];
    int len = snprintf(full_cmd, sizeof(full_cmd), "%s\r\n", cmd);
    if(len >= sizeof(full_cmd))
    {
        osMutexRelease(at_mutex);
        return HAL_ERROR;
    }

    AT_LOG_DEBUG("TX: %s", full_cmd);
    AT_HexDump("TX Data", (uint8_t *)full_cmd, strlen(full_cmd));

    HAL_StatusTypeDef tx_status = HAL_UART_Transmit(&huart1,
                                                        (uint8_t *)full_cmd,
                                                        len,
                                                        200);

    if(tx_status != HAL_OK)
    {
        osMutexRelease(at_mutex);
        return tx_status;
    }

    HAL_StatusTypeDef result = HAL_TIMEOUT;
    uint32_t start = osKernelGetTickCount();

    while((osKernelGetTickCount() - start) < timeout_ms)
    {
        uint32_t elapsed = osKernelGetTickCount() - start;
        uint32_t remain = timeout_ms - elapsed;

        if(osSemaphoreAcquire(at_resp_sem, remain) != osOK)
        {
            break;
        }

        char response_snapshot[AT_RX_BUF_SIZE];
        AT_CopyResponse(response_snapshot, sizeof(response_snapshot));

        AT_LOG_DEBUG("Got response %s", response_snapshot);

        if(strstr(response_snapshot, "ERROR"))
        {
            AT_LOG_WARN("Response mismatch");
            result = HAL_ERROR;
            break;
        }

        if(strstr(response_snapshot, expect))
        {
            AT_LOG_INFO("Command success");
            at_stats.success_count++;
            result = HAL_OK;
            break;
        }
    }

    if(result == HAL_TIMEOUT)
    {
        AT_LOG_ERROR("Timeout waiting for response");
        at_stats.timeout_count++;
    }

    osMutexRelease(at_mutex);
    return result;
}
#endif

/* 增加 WiFi 连接函数，检查中间步骤 */
HAL_StatusTypeDef AT_ConnectWiFi(const char *ssid, const char *pwd)
{
    if(!ssid || !pwd) return HAL_ERROR;
    /* 步骤1: 测试基础通信 */

    if(AT_Send("AT",                   "OK",           1000) != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* 步骤 2: 设置 Station 模式 */
    if(AT_Send("AT+CWMODE=1",          "OK",           2000) != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* 步骤 3: 连接 WiFi */
    char cmd[128];
    int len = snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, pwd);
    if(len >= sizeof(cmd))
    {
        return HAL_ERROR;
    }
    
    return AT_Send(cmd,             "WIFI GOT IP",  20000);
}

/* 带重试机制的 WiFi 连接 */
HAL_StatusTypeDef AT_ConnectWiFi_Retry(const char *ssid, const char *pwd, uint8_t max_retries)
{
    for(uint8_t i = 0; i < max_retries; ++i)
    {
        HAL_StatusTypeDef status = AT_ConnectWiFi(ssid, pwd);
        if(status == HAL_OK)
        {
            return HAL_OK;
        }

        /* 失败后等待再重试 */
        osDelay(5000);
    }

    return HAL_TIMEOUT;
}

/* 获取最后响应内容的安全接口 */
HAL_StatusTypeDef AT_GetLastResponse(char *buffer, uint16_t buffer_size)
{
    if(!buffer || buffer_size == 0) return HAL_ERROR;

    if(osMutexAcquire(at_mutex, 1000) != osOK)
    {
        return HAL_TIMEOUT;
    }

    AT_CopyResponse(buffer, buffer_size);

    osMutexRelease(at_mutex);
    return HAL_OK;
}

/* 发送命令但不等待特定响应（用于查询类命令）*/
HAL_StatusTypeDef AT_SendQuery(const char *cmd, uint32_t timeout_ms)
{
    return AT_Send(cmd, "OK", timeout_ms);
}





