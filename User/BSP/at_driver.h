#ifndef AT_DRIVER_H
#define AT_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <string.h>
#include "usart.h"

#define AT_RX_BUF_SIZE      2048

void AT_init(void);
HAL_StatusTypeDef AT_Send(const char *cmd, const char *expect, uint32_t timeout_ms);
HAL_StatusTypeDef AT_ConnectWiFi(const char *ssid, const char *pwd);
HAL_StatusTypeDef AT_ConnectWiFi_Retry(const char *ssid, const char *pwd, uint8_t max_retries);
HAL_StatusTypeDef AT_GetLastResponse(char *buffer, uint16_t buffer_size);
HAL_StatusTypeDef AT_SendQuery(const char *cmd, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif // AT_DRIVER_H