#include "at_driver.h"
#include "cmsis_os.h"
#include <stdlib.h>

#define AT_DEBUG

static  uint8_t     at_rx_dma_buf[AT_RX_BUF_SIZE];
static  uint8_t     at_frame_buf[AT_RX_BUF_SIZE];
static  uint16_t    at_frame_len = 0;
static  uint16_t    at_rx_dma_pos = 0;
static  osSemaphoreId_t    at_resp_sem;                 /* �ȴ���Ӧ���ź� */
static osMutexId_t  at_mutex;                           /* ����������Դ */
static  char        at_response[AT_RX_BUF_SIZE];        /* ������Ӧ���� */
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

    /* ���� DMA + �����жϽ��� */
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

/* ���� WiFi ���Ӻ���������м䲽�� */
HAL_StatusTypeDef AT_ConnectWiFi(const char *ssid, const char *pwd)
{
    if(!ssid || !pwd) return HAL_ERROR;
    /* ����1: ���Ի���ͨ�� */

    if(AT_Send("AT",                   "OK",           1000) != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* ���� 2: ���� Station ģʽ */
    if(AT_Send("AT+CWMODE=1",          "OK",           2000) != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* ���� 3: ���� WiFi */
    char cmd[128];
    int len = snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, pwd);
    if(len >= sizeof(cmd))
    {
        return HAL_ERROR;
    }
    
    return AT_Send(cmd,             "WIFI GOT IP",  20000);
}

/* �����Ի��Ƶ� WiFi ���� */
HAL_StatusTypeDef AT_ConnectWiFi_Retry(const char *ssid, const char *pwd, uint8_t max_retries)
{
    for(uint8_t i = 0; i < max_retries; ++i)
    {
        HAL_StatusTypeDef status = AT_ConnectWiFi(ssid, pwd);
        if(status == HAL_OK)
        {
            return HAL_OK;
        }

        /* ʧ�ܺ�ȴ������� */
        osDelay(5000);
    }

    return HAL_TIMEOUT;
}

/* ��ȡ�����Ӧ���ݵİ�ȫ�ӿ� */
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

HAL_StatusTypeDef AT_SendQuery(const char *cmd, uint32_t timeout_ms)
{
    return AT_Send(cmd, "OK", timeout_ms);
}

/* ---- HTTP 分块下载 (内部使用) ---- */

/* 发送 AT 命令并等待应答 - 调用者必须持有 at_mutex */
static HAL_StatusTypeDef AT_SendLocked(const char *cmd, const char *expect, uint32_t timeout_ms)
{
    char full_cmd[512];
    int len;

    if(!cmd || !expect) return HAL_ERROR;

    AT_ClearResponse();
    while(osSemaphoreAcquire(at_resp_sem, 0) == osOK);

    len = snprintf(full_cmd, sizeof(full_cmd), "%s\r\n", cmd);
    if(len >= (int)sizeof(full_cmd)) return HAL_ERROR;

    HAL_StatusTypeDef tx_status = HAL_UART_Transmit(&huart1, (uint8_t *)full_cmd, len, 200);
    if(tx_status != HAL_OK) return tx_status;

    uint32_t start = osKernelGetTickCount();
    HAL_StatusTypeDef result = HAL_TIMEOUT;

    while((osKernelGetTickCount() - start) < timeout_ms)
    {
        uint32_t remain = timeout_ms - (osKernelGetTickCount() - start);

        if(osSemaphoreAcquire(at_resp_sem, remain) != osOK) break;

        char snapshot[AT_RX_BUF_SIZE];
        AT_CopyResponse(snapshot, sizeof(snapshot));

        if(strstr(snapshot, "ERROR"))     { result = HAL_ERROR;   break; }
        if(strstr(snapshot, expect))      { result = HAL_OK;      break; }
        if(strstr(snapshot, "FAIL"))      { result = HAL_ERROR;   break; }
    }

    return result;
}

HAL_StatusTypeDef AT_HttpGetChunk(const char *url, uint32_t offset,
                                   uint8_t *chunk, uint32_t *chunk_len)
{
    char  host[128], path[512];
    uint16_t port = 80;
    uint32_t total_received = 0;
    const uint32_t max_chunk = chunk_len ? *chunk_len : 0;
    HAL_StatusTypeDef ret = HAL_ERROR;

    if(!url || !chunk || !chunk_len || max_chunk == 0) return HAL_ERROR;

    /* ---- 1. 解析 URL ---- */
    const char *p = url;
    if(strncmp(p, "https://", 8) == 0)      { p += 8; port = 443; }
    else if(strncmp(p, "http://", 7) == 0)  { p += 7; }

    const char *slash  = strchr(p, '/');
    const char *colon  = strchr(p, ':');

    if(colon && (!slash || colon < slash))
    {
        size_t hlen = (size_t)(colon - p);
        if(hlen >= sizeof(host)) hlen = sizeof(host) - 1;
        memcpy(host, p, hlen); host[hlen] = '\0';
        port = (uint16_t)atoi(colon + 1);
    }
    else if(slash)
    {
        size_t hlen = (size_t)(slash - p);
        if(hlen >= sizeof(host)) hlen = sizeof(host) - 1;
        memcpy(host, p, hlen); host[hlen] = '\0';
    }
    else
    {
        strncpy(host, p, sizeof(host) - 1);
        host[sizeof(host) - 1] = '\0';
    }

    if(slash) strncpy(path, slash, sizeof(path) - 1);
    else     { path[0] = '/'; path[1] = '\0'; }
    path[sizeof(path) - 1] = '\0';

    /* ---- 2. 获取互斥锁 ---- */
    if(osMutexAcquire(at_mutex, 30000) != osOK) return HAL_TIMEOUT;

    /* ---- 3. 启用多连接模式 ---- */
    if(AT_SendLocked("AT+CIPMUX=1", "OK", 2000) != HAL_OK) goto exit;
    AT_ClearResponse();
    while(osSemaphoreAcquire(at_resp_sem, 0) == osOK);

    /* ---- 4. 建立 TCP/SSL 连接 ---- */
    {
        char cmd[256];
        const char *proto = (port == 443) ? "SSL" : "TCP";

        /* SSL 需要设置缓冲区大小 */
        if(port == 443) {
            AT_SendLocked("AT+CIPSSLSIZE=4096", "OK", 2000);
            AT_ClearResponse();
            while(osSemaphoreAcquire(at_resp_sem, 0) == osOK);
        }

        int n = snprintf(cmd, sizeof(cmd), "AT+CIPSTART=1,\"%s\",\"%s\",%u", proto, host, port);
        if(n >= (int)sizeof(cmd)) goto exit;

        HAL_StatusTypeDef s = AT_SendLocked(cmd, "OK", 15000);
        if(s != HAL_OK) {
            /* 再尝试匹配 ALREADY CONNECTED */
            AT_ClearResponse();
            while(osSemaphoreAcquire(at_resp_sem, 0) == osOK);
            s = AT_SendLocked(cmd, "ALREADY CONNECTED", 5000);
        }
        if(s != HAL_OK) {
            /* HTTPS 失败时尝试 HTTP 降级 */
            if(port == 443) {
                port = 80; proto = "TCP";
                n = snprintf(cmd, sizeof(cmd), "AT+CIPSTART=1,\"%s\",\"%s\",%u", proto, host, port);
                if(n < (int)sizeof(cmd)) {
                    AT_ClearResponse();
                    while(osSemaphoreAcquire(at_resp_sem, 0) == osOK);
                    s = AT_SendLocked(cmd, "OK", 15000);
                }
            }
        }
        if(s != HAL_OK) goto exit_clip;
    }

    /* ---- 5. 构造 HTTP GET 请求 ---- */
    {
        char http_req[1024];
        uint32_t range_end = offset + max_chunk - 1;
        int req_len = snprintf(http_req, sizeof(http_req),
            "GET %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Range: bytes=%lu-%lu\r\n"
            "Connection: close\r\n"
            "\r\n",
            path, host, (unsigned long)offset, (unsigned long)range_end);

        if(req_len < 0 || req_len >= (int)sizeof(http_req)) goto exit_clip;

        /* ---- 6. AT+CIPSEND ---- */
        {
            char cipsend[64];
            snprintf(cipsend, sizeof(cipsend), "AT+CIPSEND=1,%d", req_len);
            if(AT_SendLocked(cipsend, ">", 5000) != HAL_OK) goto exit_clip;

            /* 短暂延时等待 '>' 稳定 */
            osDelay(10);

            /* ---- 7. 发送 HTTP 数据 (不加 CRLF) ---- */
            if(HAL_UART_Transmit(&huart1, (uint8_t *)http_req, req_len, 5000) != HAL_OK)
                goto exit_clip;

            /* 等待 SEND OK */
            {
                char snap[AT_RX_BUF_SIZE];
                uint32_t t0 = osKernelGetTickCount();
                int ok = 0;
                AT_ClearResponse();
                while(osSemaphoreAcquire(at_resp_sem, 0) == osOK);
                while((osKernelGetTickCount() - t0) < 10000)
                {
                    if(osSemaphoreAcquire(at_resp_sem, 1000) == osOK)
                    {
                        AT_CopyResponse(snap, sizeof(snap));
                        if(strstr(snap, "SEND OK")) { ok = 1; break; }
                        if(strstr(snap, "ERROR"))   break;
                    }
                }
                if(!ok) goto exit_clip;
            }
        }
    }

    /* ---- 8. 接收 HTTP 响应 ---- */
    {
        int headers_done = 0;
        uint8_t *body_start = NULL;
        uint32_t content_length = 0;
        uint32_t recv_deadline = osKernelGetTickCount() + 20000;
        int connection_closed = 0;

        AT_ClearResponse();
        while(osSemaphoreAcquire(at_resp_sem, 0) == osOK);

        while(!connection_closed && (osKernelGetTickCount() < recv_deadline))
        {
            if(osSemaphoreAcquire(at_resp_sem, 3000) != osOK) break;

            /* 原子复制并清空 at_response */
            char snapshot[AT_RX_BUF_SIZE];
            uint32_t primask = AT_EnterCritical();
            uint16_t snap_len = at_response_len;
            if(snap_len >= sizeof(snapshot)) snap_len = sizeof(snapshot) - 1;
            memcpy(snapshot, at_response, snap_len);
            snapshot[snap_len] = '\0';
            at_response_len = 0;
            at_response[0] = '\0';
            AT_ExitCritical(primask);

            /* 检查 CLOSED */
            if(strstr(snapshot, "CLOSE")) connection_closed = 1;

            /* 第一次先分离 HTTP 头 */
            if(!headers_done)
            {
                char *header_end = strstr(snapshot, "\r\n\r\n");
                if(header_end)
                {
                    body_start = (uint8_t *)header_end + 4;
                    headers_done = 1;

                    char *cl = strstr(snapshot, "Content-Length:");
                    if(cl) content_length = (uint32_t)atoi(cl + 15);
                }
            }

            if(headers_done && body_start)
            {
                uint32_t avail = (uint32_t)((uint8_t *)snapshot + snap_len - body_start);
                uint32_t copy = avail;
                if(total_received + copy > max_chunk) copy = max_chunk - total_received;
                if(copy > 0)
                {
                    memcpy(chunk + total_received, body_start, copy);
                    total_received += copy;
                }
                body_start = NULL;
            }
            else if(headers_done)
            {
                uint32_t copy = snap_len;
                if(total_received + copy > max_chunk) copy = max_chunk - total_received;
                if(copy > 0)
                {
                    memcpy(chunk + total_received, snapshot, copy);
                    total_received += copy;
                }
            }

            if(total_received >= max_chunk) break;
            if(content_length > 0 && total_received >= content_length) break;
        }
    }

    ret = (total_received > 0) ? HAL_OK : HAL_ERROR;

exit_clip:
    /* ---- 9. 关闭 TCP 连接 ---- */
    AT_SendLocked("AT+CIPCLOSE=1", "OK", 3000);
    AT_ClearResponse();
    while(osSemaphoreAcquire(at_resp_sem, 0) == osOK);

exit:
    osMutexRelease(at_mutex);
    *chunk_len = total_received;
    return ret;
}





