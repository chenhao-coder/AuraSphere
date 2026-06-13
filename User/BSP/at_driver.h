#ifndef AT_DRIVER_H
#define AT_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <string.h>
#include "usart.h"

#define AT_RX_BUF_SIZE      4096   /* 需容纳 +IPD 片段, ESP8266 单包可达 2920+ 字节 */

void AT_init(void);
HAL_StatusTypeDef AT_Send(const char *cmd, const char *expect, uint32_t timeout_ms);
HAL_StatusTypeDef AT_ConnectWiFi(const char *ssid, const char *pwd);
HAL_StatusTypeDef AT_ConnectWiFi_Retry(const char *ssid, const char *pwd, uint8_t max_retries);
HAL_StatusTypeDef AT_GetLastResponse(char *buffer, uint16_t buffer_size);
HAL_StatusTypeDef AT_SendQuery(const char *cmd, uint32_t timeout_ms);
HAL_StatusTypeDef AT_HttpGetChunk(const char *url, uint32_t offset,
                                   uint8_t *chunk, uint32_t *chunk_len);
void AT_ClearRxBuffer(void);

/* 持久化 SSL 连接下载: 单连接发送多个 Range 请求 */
HAL_StatusTypeDef AT_HttpPersistOpen(const char *url, uint32_t *total_size);
HAL_StatusTypeDef AT_HttpPersistGetChunk(uint32_t offset, uint8_t *chunk, uint32_t *chunk_len);
void AT_HttpPersistClose(void);

/* 流式 HTTP 下载: 单连接获取整个文件 */
HAL_StatusTypeDef AT_HttpOpen(const char *url, uint32_t *total_size);
HAL_StatusTypeDef AT_HttpRead(uint8_t *buf, uint32_t *len);
void AT_HttpClose(void);

#ifdef __cplusplus
}
#endif

#endif // AT_DRIVER_H