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
        /* 缓冲区溢出, 丢失部分数据. 重新启动 DMA */
        Size = (uint16_t)(AT_RX_BUF_SIZE - 1);
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

/* 清空 AT 接收缓冲区 (用于处理 unsolicited 消息后防止重复) */
void AT_ClearRxBuffer(void)
{
    if(osMutexAcquire(at_mutex, 1000) != osOK) return;
    AT_ClearResponse();
    osMutexRelease(at_mutex);
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

        char snapshot[512];
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
    uint32_t content_length = 0; /* HTTP Content-Length, 跨代码块使用 */

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

    /* ---- 4. 建立 TCP 连接 (跳过 SSL, 直接 HTTP:80) ---- */
    {
        char cmd[256];
        port = 80;  /* 强制 HTTP, ESP8266 不支持 SSL */

        int n = snprintf(cmd, sizeof(cmd), "AT+CIPSTART=1,\"TCP\",\"%s\",%u", host, port);
        if(n >= (int)sizeof(cmd)) goto exit;

        HAL_StatusTypeDef s = AT_SendLocked(cmd, "OK", 15000);
        if(s != HAL_OK) {
            AT_ClearResponse();
            while(osSemaphoreAcquire(at_resp_sem, 0) == osOK);
            s = AT_SendLocked(cmd, "ALREADY CONNECTED", 5000);
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
    content_length = 0; /* 外层作用域, 供后续完整性检查使用 */
    {
        int headers_done = 0;
        uint8_t *body_start = NULL;
        uint32_t recv_deadline = osKernelGetTickCount() + 20000;
        int connection_closed = 0;

        /* 不清空 at_response! SEND OK 阶段可能已有 HTTP 响应数据 */
        osDelay(100); /* 给 ESP8266 一点时间输出 HTTP 响应 */
        while(osSemaphoreAcquire(at_resp_sem, 0) == osOK); /* drain stale sem */

        while(!connection_closed && (osKernelGetTickCount() < recv_deadline))
        {
            /* 检查 at_response 是否已有数据 (不清空后再等) */
            uint32_t primask;
            uint16_t snap_len;
            primask = AT_EnterCritical();
            snap_len = at_response_len;
            AT_ExitCritical(primask);

            if(snap_len == 0) {
                if(osSemaphoreAcquire(at_resp_sem, 3000) != osOK) break;
                primask = AT_EnterCritical();
                snap_len = at_response_len;
                AT_ExitCritical(primask);
                if(snap_len == 0) continue;
            }

            /* 拷贝并清空 at_response */
            {
                static char g_snap[4096]; /* static 省栈 */
                uint16_t copy_len = snap_len;
                if(copy_len > sizeof(g_snap) - 1) copy_len = sizeof(g_snap) - 1;
                primask = AT_EnterCritical();
                memcpy(g_snap, at_response, copy_len);
                g_snap[copy_len] = '\0';
                at_response_len = 0;
                at_response[0] = '\0';
                AT_ExitCritical(primask);

                /* 检查 CLOSED */
                if(strstr(g_snap, "CLOSE")) connection_closed = 1;

                /* 第一次先分离 HTTP 头 */
                if(!headers_done)
                {
                    char *header_end = strstr(g_snap, "\r\n\r\n");
                    if(header_end)
                    {
                        headers_done = 1;
                        char *cl = strstr(g_snap, "Content-Length:");
                        if(cl) content_length = (uint32_t)atoi(cl + 15);
                        /* body 数据从 header_end+4 开始 */
                        uint32_t body_len = (uint32_t)((uint8_t*)g_snap + copy_len - (uint8_t*)header_end - 4);
                        uint32_t take = body_len;
                        if(total_received + take > max_chunk) take = max_chunk - total_received;
                        if(take > 0) {
                            memcpy(chunk + total_received, header_end + 4, take);
                            total_received += take;
                        }
                    }
                    else if(total_received == 0)
                    {
                        /* 还没收到头部, 继续等 */
                        continue;
                    }
                }
                else
                {
                    /* 头部已解析, 后续所有数据都是 body */
                    uint32_t take = copy_len;
                    if(total_received + take > max_chunk) take = max_chunk - total_received;
                    if(take > 0) {
                        memcpy(chunk + total_received, g_snap, take);
                        total_received += take;
                    }
                }
            }

            if(total_received >= max_chunk) break;
            if(content_length > 0 && total_received >= content_length) break;
        }
    }

    /* 验证接收完整性: 必须等于 Content-Length (或至少达到 max_chunk) */
    if(content_length > 0 && total_received < content_length) {
        /* 收到的不完整! 可能是 ESP8266 提前截断 */
        ret = HAL_ERROR;
    } else {
        ret = (total_received > 0) ? HAL_OK : HAL_ERROR;
    }

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

/* ============ 流式 HTTP 下载 (单连接, 避免 auth_key 重复使用失效) ============ */

static int      g_http_open = 0;        /* 是否有打开的流 */
static uint8_t  g_http_buf[4096];       /* 头部解析后剩余的 body 数据 */
static uint32_t g_http_buf_len = 0;
static uint32_t g_http_total = 0;        /* Content-Length */
static uint32_t g_http_received = 0;

HAL_StatusTypeDef AT_HttpOpen(const char *url, uint32_t *total_size)
{
    char  host[128], path[512];
    uint16_t port = 80;
    char cmd[256];
    HAL_StatusTypeDef s;

    if(!url || !total_size) return HAL_ERROR;
    if(g_http_open) return HAL_ERROR;    /* 不允许嵌套 */

    /* ---- 1. 解析 URL ---- */
    const char *p = url;
    if(strncmp(p, "https://", 8) == 0)      { p += 8; port = 443; }
    else if(strncmp(p, "http://", 7) == 0)  { p += 7; }

    const char *slash = strchr(p, '/');
    const char *colon = strchr(p, ':');
    if(colon && (!slash || colon < slash)) {
        size_t hlen = (size_t)(colon - p);
        if(hlen >= sizeof(host)) hlen = sizeof(host) - 1;
        memcpy(host, p, hlen); host[hlen] = '\0';
        port = (uint16_t)atoi(colon + 1);
    } else if(slash) {
        size_t hlen = (size_t)(slash - p);
        if(hlen >= sizeof(host)) hlen = sizeof(host) - 1;
        memcpy(host, p, hlen); host[hlen] = '\0';
    } else {
        strncpy(host, p, sizeof(host) - 1);
        host[sizeof(host) - 1] = '\0';
    }
    if(slash) strncpy(path, slash, sizeof(path) - 1);
    else      { path[0] = '/'; path[1] = '\0'; }
    path[sizeof(path) - 1] = '\0';

    printf("[HTTP] Target: %s:%u%s\r\n", host, port, path);

    /* ---- 2. 获取互斥锁 (一直持有直到 AT_HttpClose) ---- */
    if(osMutexAcquire(at_mutex, 60000) != osOK) {
        printf("[HTTP] Mutex timeout\r\n");
        return HAL_TIMEOUT;
    }

    /* ---- 3. 启用多连接模式 ---- */
    if(AT_SendLocked("AT+CIPMUX=1", "OK", 2000) != HAL_OK) {
        printf("[HTTP] CIPMUX failed\r\n");
        goto exit;
    }
    AT_ClearResponse();
    while(osSemaphoreAcquire(at_resp_sem, 0) == osOK);

    /* ---- 4. 建立 TCP 连接 (ESP8266 不支持 SSL, 直接走 HTTP:80) ---- */
    {
        /* 强制 HTTP, 跳过 SSL. 不设 CIPRECVMODE, 使用 ESP8266 默认 +IPD 自动转发 */
        port = 80;

        int n = snprintf(cmd, sizeof(cmd), "AT+CIPSTART=1,\"TCP\",\"%s\",%u", host, port);
        if(n >= (int)sizeof(cmd)) goto exit_clip;
        printf("[HTTP] Connecting TCP://%s:%u ...\r\n", host, port);
        s = AT_SendLocked(cmd, "OK", 15000);
        if(s != HAL_OK) {
            AT_ClearResponse();
            while(osSemaphoreAcquire(at_resp_sem, 0) == osOK);
            s = AT_SendLocked(cmd, "ALREADY CONNECTED", 5000);
        }
        if(s != HAL_OK) {
            printf("[HTTP] CIPSTART failed!\r\n");
            goto exit_clip;
        }
        printf("[HTTP] Connected OK\r\n");
    }

    /* ---- 5. 构造 HTTP GET 请求 (无 Range, 请求完整文件) ---- */
    {
        char http_req[1024];
        int req_len = snprintf(http_req, sizeof(http_req),
            "GET %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Connection: close\r\n"
            "\r\n",
            path, host);

        if(req_len < 0 || req_len >= (int)sizeof(http_req)) goto exit_clip;

        /* HTTP GET request 内容已通过 AT+CIPSEND 发送 */
        printf("[HTTP] Sending GET...\r\n");
        {
            char cipsend[64];
            snprintf(cipsend, sizeof(cipsend), "AT+CIPSEND=1,%d", req_len);
            if(AT_SendLocked(cipsend, ">", 5000) != HAL_OK) {
                printf("[HTTP] CIPSEND failed\r\n");
                goto exit_clip;
            }
            osDelay(10);
            printf("[HTTP] Sending %d bytes HTTP GET...\r\n", req_len);
            if(HAL_UART_Transmit(&huart1, (uint8_t *)http_req, req_len, 5000) != HAL_OK) {
                printf("[HTTP] TX failed\r\n");
                goto exit_clip;
            }

            /* 等待 SEND OK */
            {
                char snap[AT_RX_BUF_SIZE];
                uint32_t t0 = osKernelGetTickCount();
                int ok = 0;
                AT_ClearResponse();
                while(osSemaphoreAcquire(at_resp_sem, 0) == osOK);
                while((osKernelGetTickCount() - t0) < 10000) {
                    if(osSemaphoreAcquire(at_resp_sem, 1000) == osOK) {
                        AT_CopyResponse(snap, sizeof(snap));
                        if(strstr(snap, "SEND OK")) { ok = 1; break; }
                        if(strstr(snap, "ERROR"))   break;
                    }
                }
                if(!ok) {
                    printf("[HTTP] SEND OK timeout\r\n");
                    goto exit_clip;
                }
            }
        }
    }

    /* 给 ESP8266 时间输出 +IPD 数据 (SEND OK 之后的 HTTP 响应可能在路上) */
    osDelay(300);

    printf("[HTTP] Waiting headers...\r\n");
    /* ---- 6. 接收 HTTP 响应头, 累积分片直到 \\r\\n\\r\\n, 提取 Content-Length ---- */
    {
        static char hdr_buf[4096];  /* static 省栈空间, mutex 保护线程安全 */
        int  hdr_len = 0;
        int  headers_done = 0;
        uint32_t recv_deadline = osKernelGetTickCount() + 15000;

        g_http_buf_len = 0;
        g_http_total = 0;
        g_http_received = 0;

        while(!headers_done && (osKernelGetTickCount() < recv_deadline))
        {
            uint16_t snap_len;

            /* 先检查 buffer 是否已有数据 (SEND OK 阶段可能已收到 +IPD) */
            uint32_t primask = AT_EnterCritical();
            snap_len = at_response_len;
            AT_ExitCritical(primask);

            if(snap_len == 0)
            {
                if(osSemaphoreAcquire(at_resp_sem, 3000) != osOK) break;
                primask = AT_EnterCritical();
                snap_len = at_response_len;
                if(snap_len == 0) { AT_ExitCritical(primask); continue; }
                /* 直接拷贝到 hdr_buf, 省掉栈上 4KB snapshot */
                {
                    int copy = snap_len;
                    if(hdr_len + copy > (int)sizeof(hdr_buf) - 1)
                        copy = (int)sizeof(hdr_buf) - 1 - hdr_len;
                    if(copy > 0) {
                        memcpy(hdr_buf + hdr_len, at_response, copy);
                        hdr_len += copy;
                    }
                }
                at_response_len = 0;
                at_response[0] = '\0';
                AT_ExitCritical(primask);
            }
            else
            {
                primask = AT_EnterCritical();
                snap_len = at_response_len;  /* 重读最新长度, 避免竞态丢数据 */
                if(snap_len > 0) {
                    int copy = snap_len;
                    if(hdr_len + copy > (int)sizeof(hdr_buf) - 1)
                        copy = (int)sizeof(hdr_buf) - 1 - hdr_len;
                    if(copy > 0) {
                        memcpy(hdr_buf + hdr_len, at_response, copy);
                        hdr_len += copy;
                    }
                }
                at_response_len = 0;
                at_response[0] = '\0';
                AT_ExitCritical(primask);
            }

            hdr_buf[hdr_len] = '\0';

            /* 检查 CLOSED/ERROR (在拷贝后的 hdr_buf 上检查, 线程安全) */
            if(strstr(hdr_buf, "CLOSED") || strstr(hdr_buf, "ERROR"))
                goto exit_clip;

            /* 只在确认收到 HTTP 响应后才搜索 \\r\\n\\r\\n, 避免误匹配 Recv/SEND OK 等 */
            char *http_marker = strstr(hdr_buf, "HTTP/");
            char *header_end = http_marker ? strstr(http_marker, "\r\n\r\n") : NULL;
            if(header_end)
            {
                headers_done = 1;
                /* 在累积的完整头部中搜索 */
                char *cl = strstr(hdr_buf, "Content-Length:");
                if(cl) {
                    g_http_total = (uint32_t)atoi(cl + 15);
                }
                /* 调试: 打印状态码和 Content-Length */
                {
                    char *status_end = strstr(http_marker, "\r\n");
                    int status_len = status_end ? (int)(status_end - http_marker) : 20;
                    printf("[HTTP] Response: %.*s\r\n", status_len, http_marker);
                }
                /* 保存 \\r\\n\\r\\n 之后的 body 数据 */
                uint8_t *body_start = (uint8_t *)header_end + 4;
                uint32_t body_avail = (uint32_t)((uint8_t *)hdr_buf + hdr_len - body_start);
                if(body_avail > 0) {
                    uint32_t save = body_avail;
                    if(save > sizeof(g_http_buf)) save = sizeof(g_http_buf);
                    memcpy(g_http_buf, body_start, save);
                    g_http_buf_len = save;
                    g_http_received = save;
                    if(body_avail > sizeof(g_http_buf))
                        printf("[HTTP] Warning: body data truncated %lu->%u\r\n",
                               body_avail, (unsigned)sizeof(g_http_buf));
                }
            }
        }
        if(!headers_done) {
            hdr_buf[hdr_len < (int)sizeof(hdr_buf)-1 ? hdr_len : (int)sizeof(hdr_buf)-1] = '\0';
            printf("[HTTP] Headers timeout (have %d bytes):\r\n%s\r\n", hdr_len, hdr_buf);
            goto exit_clip;
        }
    }

    /* 消耗 header 解析期间可能产生的残留信号量, 并将 DMA 已收到的数据加入缓存 */
    {
        uint32_t _primask = AT_EnterCritical();
        uint16_t _extra = at_response_len;
        if(_extra > 0 && g_http_buf_len == 0) {
            uint16_t _save = (_extra < sizeof(g_http_buf)) ? _extra : sizeof(g_http_buf);
            memcpy(g_http_buf, at_response, _save);
            g_http_buf_len = _save;
            g_http_received += _save;
            at_response_len = 0;
            at_response[0] = '\0';
        }
        AT_ExitCritical(_primask);
        while(osSemaphoreAcquire(at_resp_sem, 0) == osOK); /* drain stale semaphore */
    }

    printf("[HTTP] Headers OK, Content-Length=%lu\r\n", g_http_total);
    g_http_open = 1;
    *total_size = g_http_total;
    return HAL_OK;

exit_clip:
    printf("[HTTP] Closing connection\r\n");
    AT_SendLocked("AT+CIPCLOSE=1", "OK", 3000);
    AT_ClearResponse();
    while(osSemaphoreAcquire(at_resp_sem, 0) == osOK);
exit:
    osMutexRelease(at_mutex);
    g_http_open = 0;
    return HAL_ERROR;
}

HAL_StatusTypeDef AT_HttpRead(uint8_t *buf, uint32_t *len)
{
    uint32_t copy;

    if(!g_http_open || !buf || !len || *len == 0) return HAL_ERROR;

    /* 优先返回缓存的数据 (头部解析时剩余的 body) */
    if(g_http_buf_len > 0)
    {
        copy = (*len < g_http_buf_len) ? *len : g_http_buf_len;
        memcpy(buf, g_http_buf, copy);
        if(g_http_buf_len > copy)
            memmove(g_http_buf, g_http_buf + copy, g_http_buf_len - copy);
        g_http_buf_len -= copy;
        *len = copy;
        return HAL_OK;
    }

    /* 轮询模式: 直接检查 at_response, 不等信号量 (binary sem 高速时丢通知) */
    {
        uint32_t deadline = osKernelGetTickCount() + 30000;
        uint16_t snap_len;
        uint32_t primask;

        while((osKernelGetTickCount() < deadline))
        {
            /* 检查 at_response 有无数据 */
            primask = AT_EnterCritical();
            snap_len = at_response_len;
            AT_ExitCritical(primask);

            if(snap_len == 0) {
                /* 无数据: 短暂延时后继续轮询 */
                osDelay(50);
                continue;
            }

            /* === 有数据: 检查 CLOSED/ERROR === */
            {
                char tmp[256];
                uint16_t check = (snap_len < 255) ? snap_len : 255;
                memcpy(tmp, at_response, check);
                tmp[check] = '\0';
                if(strstr(tmp, "CLOSED") || strstr(tmp, "CLOSE"))
                {
                    primask = AT_EnterCritical();
                    at_response_len = 0; at_response[0] = '\0';
                    AT_ExitCritical(primask);
                    g_http_open = 0;
                    if(g_http_total > 0 && g_http_received >= g_http_total)
                        { *len = 0; return HAL_OK; }
                    *len = 0;
                    return HAL_ERROR;
                }
                if(strstr(tmp, "ERROR"))
                {
                    primask = AT_EnterCritical();
                    at_response_len = 0; at_response[0] = '\0';
                    AT_ExitCritical(primask);
                    *len = 0;
                    return HAL_ERROR;
                }
            }

            /* === 尝试 +IPD 解析 === */
            {
                char *ipd = strstr((char*)at_response, "+IPD,");
                if(ipd)
                {
                    char *comma2 = strchr(ipd + 5, ',');
                    char *colon  = comma2 ? strchr(comma2 + 1, ':') : strchr(ipd + 5, ':');
                    if(colon)
                    {
                        int ipd_data_len = comma2 ? atoi(comma2 + 1) : 0;
                        if(ipd_data_len > 0)
                        {
                            char *data_start = colon + 1;
                            uint32_t avail = (uint32_t)((uint8_t*)at_response + snap_len - (uint8_t*)data_start);
                            copy = (uint32_t)ipd_data_len < *len ? (uint32_t)ipd_data_len : *len;
                            if(copy > avail) copy = avail;
                            if(copy > 0)
                            {
                                memcpy(buf, data_start, copy);
                                g_http_received += copy;
                                uint32_t leftover = (avail > (uint32_t)ipd_data_len) ? avail - (uint32_t)ipd_data_len : 0;
                                if(leftover > 0) {
                                    g_http_buf_len = leftover;
                                    if(g_http_buf_len > sizeof(g_http_buf)) g_http_buf_len = sizeof(g_http_buf);
                                    memcpy(g_http_buf, data_start + ipd_data_len, g_http_buf_len);
                                    if(strstr((char*)g_http_buf, "CLOSED")) { g_http_open = 0; g_http_buf_len = 0; }
                                }
                                primask = AT_EnterCritical();
                                at_response_len = 0; at_response[0] = '\0';
                                AT_ExitCritical(primask);
                                *len = copy;
                                return HAL_OK;
                            }
                        }
                    }
                    /* IPD 解析失败: 清空, 继续等待 */
                    primask = AT_EnterCritical();
                    at_response_len = 0; at_response[0] = '\0';
                    AT_ExitCritical(primask);
                    continue;
                }
            }

            /* === 无 +IPD: 裸固件数据 === */
            /* 裸固件数据 (无 +IPD 前缀) */
            copy = (*len < snap_len) ? *len : snap_len;
            if(copy > 0)
            {
                memcpy(buf, at_response, copy);
                g_http_received += copy;
                if(snap_len > copy) {
                    g_http_buf_len = snap_len - copy;
                    if(g_http_buf_len > sizeof(g_http_buf)) g_http_buf_len = sizeof(g_http_buf);
                    memcpy(g_http_buf, (uint8_t*)at_response + copy, g_http_buf_len);
                }
                primask = AT_EnterCritical();
                at_response_len = 0; at_response[0] = '\0';
                AT_ExitCritical(primask);
                *len = copy;
                return HAL_OK;
            }
        } /* end while */

        /* 超时: 30秒无数据 */
        printf("[HTTP] Poll timeout, received=%lu/%lu\r\n", g_http_received, g_http_total);
        *len = 0;
        return HAL_ERROR;
    }
}

void AT_HttpClose(void)
{
    if(g_http_open)
    {
        AT_SendLocked("AT+CIPCLOSE=1", "OK", 3000);
        AT_ClearResponse();
        while(osSemaphoreAcquire(at_resp_sem, 0) == osOK);
        g_http_open = 0;
        g_http_buf_len = 0;
    }
    osMutexRelease(at_mutex);
}





