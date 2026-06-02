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

static void AT_RestartRx(UART_HandleTypeDef *huart)
{
    HAL_UARTEx_ReceiveToIdle_DMA(huart, at_rx_dma_buf, AT_RX_BUF_SIZE);
    __HAL_DMA_DISABLE_IT(huart->hdmarx, DMA_IT_HT);
}

static void AT_AppendResponse(const uint8_t *data, uint16_t size)
{
    size_t used = strlen(at_response);
    size_t free_len = sizeof(at_response) - used - 1;

    if(size > free_len)
    {
        size = free_len;
    }

    memcpy(at_response + used, data, size);
    at_response[used + size] = '\0';
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

    if(Size >= AT_RX_BUF_SIZE)
    {
        AT_LOG_WARN("Rx size at limit: %d", Size);
        Size = AT_RX_BUF_SIZE - 1;
    }

    if(Size < at_rx_dma_pos)
    {
        at_rx_dma_pos = 0;
    }

    at_frame_len = Size - at_rx_dma_pos;
    AT_LOG_DEBUG("RX_Callback: Size=%d fresh=%d", Size, at_frame_len);
    AT_HexDump("RX New Data", &at_rx_dma_buf[at_rx_dma_pos], at_frame_len);

    AT_AppendResponse(&at_rx_dma_buf[at_rx_dma_pos], at_frame_len);
    at_rx_dma_pos = Size;

    AT_LOG_INFO("Response: %s", at_response);

    at_stats.rx_count++;
    at_stats.dma_restart_count++;

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

    memset(at_response, 0, sizeof(at_response));
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

        AT_LOG_DEBUG("Got response %s", at_response);

        if(strstr(at_response, "ERROR"))
        {
            AT_LOG_WARN("Response mismatch");
            result = HAL_ERROR;
            break;
        }

        if(strstr(at_response, expect))
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

    strncpy(buffer, at_response, buffer_size - 1);
    buffer[buffer_size - 1] = '\0';

    osMutexRelease(at_mutex);
    return HAL_OK;
}

/* 发送命令但不等待特定响应（用于查询类命令）*/
HAL_StatusTypeDef AT_SendQuery(const char *cmd, uint32_t timeout_ms)
{
    return AT_Send(cmd, "OK", timeout_ms);
}





