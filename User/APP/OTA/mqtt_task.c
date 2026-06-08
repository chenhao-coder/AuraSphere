#include "at_driver.h"
#include "mqtt_task.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static const char *ssid = "CU_bDHQ";
static const char *pwd = "kba26t4u";

#define FIRMWARE_VERSION "1.0.1"

#define ALIYUN_HOST     PRODUCT_KEY ".iot-as-mqtt.cn-shanghai.aliyuncs.com"
#define ALIYUN_PORT  1883

/* ESP8266 AT 命令字段转义: " -> \\",  , -> \\,,  \\ -> \\\\ */
void EscapeEspAtMqttField(const char *src, char *dst, size_t dst_size)
{
    size_t j = 0;

    if (dst_size == 0)
        return;

    for (size_t i = 0; src[i] != '\0' && j + 1 < dst_size; i++)
    {
        if ((src[i] == '\\' || src[i] == '"' || src[i] == ',') && j + 3 < dst_size)
        {
            dst[j++] = '\\';
            dst[j++] = src[i];
        }
        else if (j + 1 < dst_size)
        {
            dst[j++] = src[i];
        }
    }

    dst[j] = '\0';
}

/* 还原 ESP8266 AT 转义 (原地操作, 输出 <= 输入) */
void EscapeEspAtMqttFieldUnescape(char *str)
{
    char *src = str, *dst = str;
    while (*src)
    {
        if (src[0] == '\\' && (src[1] == ',' || src[1] == '"' || src[1] == '\\'))
        {
            *dst++ = src[1];
            src += 2;
        }
        else
        {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

static int MonthToIndex(const char *month)
{
    static const char *months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };

    for(int i = 0; i < 12; i++)
    {
        if(strncmp(month, months[i], 3) == 0)
        {
            return i;
        }
    }

    return -1;
}

static int IsLeapYear(int year)
{
    return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
}

static uint16_t DaysBeforeMonth(int year, int month)
{
    static const uint16_t days_before_month[] = {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };

    uint16_t days = days_before_month[month];
    if((month > 1) && IsLeapYear(year))
    {
        days++;
    }

    return days;
}

static uint64_t UtcToUnixSeconds(int year, int month, int day, int hour, int minute, int second)
{
    uint64_t days = 0;

    for(int y = 1970; y < year; y++)
    {
        days += IsLeapYear(y) ? 366U : 365U;
    }

    days += DaysBeforeMonth(year, month);
    days += (uint32_t)(day - 1);

    return days * 86400ULL + (uint64_t)hour * 3600ULL + (uint64_t)minute * 60ULL + (uint64_t)second;
}

static HAL_StatusTypeDef ParseEspSntpTime(const char *response, uint64_t *unix_ms)
{
    char month_name[4];
    int day, hour, minute, second, year;
    int month;
    const char *time_text;

    if((response == NULL) || (unix_ms == NULL))
    {
        return HAL_ERROR;
    }

    time_text = strstr(response, "+CIPSNTPTIME:");
    if(time_text == NULL)
    {
        return HAL_ERROR;
    }

    time_text += strlen("+CIPSNTPTIME:");
    if(sscanf(time_text, "%*3s %3s %d %d:%d:%d %d",
              month_name, &day, &hour, &minute, &second, &year) != 6)
    {
        return HAL_ERROR;
    }

    month = MonthToIndex(month_name);
    if((month < 0) || (year < 2024) || (day < 1) || (day > 31) ||
       (hour < 0) || (hour > 23) || (minute < 0) || (minute > 59) ||
       (second < 0) || (second > 59))
    {
        return HAL_ERROR;
    }

    *unix_ms = UtcToUnixSeconds(year, month, day, hour, minute, second) * 1000ULL;
    return HAL_OK;
}

static void Uint64ToStr(uint64_t val, char *buf, size_t buf_size)
{
    char temp[21];
    int i = 0;

    if (val == 0)
    {
        if (buf_size > 1)
        {
            buf[0] = '0';
            buf[1] = '\0';
        }
        return;
    }

    while (val > 0 && i < (int)sizeof(temp) - 1)
    {
        temp[i++] = '0' + (char)(val % 10ULL);
        val /= 10ULL;
    }

    size_t j = 0;
    while (i > 0 && j + 1 < buf_size)
    {
        buf[j++] = temp[--i];
    }
    buf[j] = '\0';
}

static HAL_StatusTypeDef MQTT_GetTimestamp(char *timestamp, size_t timestamp_size)
{
    char response[AT_RX_BUF_SIZE];
    uint64_t unix_ms;

    if((timestamp == NULL) || (timestamp_size == 0))
    {
        return HAL_ERROR;
    }

    if(AT_Send("AT+CIPSNTPCFG=1,0,\"ntp.aliyun.com\",\"cn.ntp.org.cn\"", "OK", 3000) != HAL_OK)
    {
        return HAL_ERROR;
    }

    for(uint8_t retry = 0; retry < 5; retry++)
    {
        osDelay(1000);

        if(AT_Send("AT+CIPSNTPTIME?", "+CIPSNTPTIME:", 3000) != HAL_OK)
        {
            continue;
        }

        if(AT_GetLastResponse(response, sizeof(response)) != HAL_OK)
        {
            continue;
        }

        if(ParseEspSntpTime(response, &unix_ms) == HAL_OK)
        {
            Uint64ToStr(unix_ms, timestamp, timestamp_size);
            return HAL_OK;
        }
    }

    return HAL_TIMEOUT;
}

osMessageQueueId_t ota_notify_queue;       
const osMessageQueueAttr_t ota_queue_attributes = {
  .name = "ota_notify_queue"
};

void MQTT_Task(void *arg)
{
    AT_init();
    osDelay(1000);
    ota_notify_queue = osMessageQueueNew(2, 512, &ota_queue_attributes);

    while(AT_ConnectWiFi(ssid, pwd) != HAL_OK)
    {
        printf("[MQTT] WiFi connecting...\r\n");
        osDelay(3000);
    }
    printf("[MQTT] WiFi OK\r\n");
    osDelay(1000);

    char timestamp[16];
    while(MQTT_GetTimestamp(timestamp, sizeof(timestamp)) != HAL_OK)
    {
        printf("[MQTT] SNTP sync failed, retrying...\r\n");
        osDelay(3000);
    }
    printf("[MQTT] SNTP timestamp: %s\r\n", timestamp);

    char client_id[256], username[128], password[65];
    Aliyun_CalcMqttParams(client_id, username, password, timestamp);
    char at_client_id[320];
    EscapeEspAtMqttField(client_id, at_client_id, sizeof(at_client_id));

    char cmd[512];
    snprintf(cmd, sizeof(cmd),
            "AT+MQTTUSERCFG=0,1,\"%s\",\"%s\",\"%s\",0,0,\"\"",
            at_client_id, username, password);
    while (AT_Send(cmd, "OK", 5000) != HAL_OK)
    {
        printf("[MQTT] MQTT config failed, retrying...\r\n");
        osDelay(1000);
    }

    snprintf(cmd, sizeof(cmd), "AT+MQTTCONN=0,\"%s\",%d,1", ALIYUN_HOST, ALIYUN_PORT);
    while (AT_Send(cmd, "+MQTTCONNECTED", 10000) != HAL_OK)
    {
        printf("[MQTT] Connecting...\r\n");
        osDelay(3000);
    }
    printf("[MQTT] Connected!\r\n");

    /* 等待 MQTT keepalive 就绪后再订阅 */
    osDelay(500);

    /* Step4: 订阅 OTA 升级 Topic */
    snprintf(cmd, sizeof(cmd),
        "AT+MQTTSUB=0,\"%s\",1", OTA_TOPIC_UP);
    if(AT_Send(cmd, "OK", 5000) == HAL_OK)
        printf("[MQTT] Subscribed to OTA topic OK\r\n");
    else
        printf("[MQTT] Subscribe FAILED!\r\n");
    osDelay(200);

    /* Step5: 上报固件版本 (阿里云 IoT 要求先上报版本才会下发 OTA) */
    {
        char payload[128], escaped_payload[256];
        snprintf(payload, sizeof(payload),
            "{\"id\":\"%lu\",\"params\":{\"version\":\"%s\",\"module\":\"MCU\"}}",
            (uint32_t)osKernelGetTickCount(), FIRMWARE_VERSION);
        EscapeEspAtMqttField(payload, escaped_payload, sizeof(escaped_payload));
        snprintf(cmd, sizeof(cmd),
            "AT+MQTTPUB=0,\"%s\",\"%s\",1,0",
            OTA_TOPIC_IN, escaped_payload);
        if(AT_Send(cmd, "OK", 5000) == HAL_OK)
            printf("[MQTT] Firmware version reported: %s\r\n", FIRMWARE_VERSION);
        else
            printf("[MQTT] Version report FAILED!\r\n");
    }

    /* Step6: 循环处理消息 */
    char response[AT_RX_BUF_SIZE];
    for(;;)
    {
        if(AT_GetLastResponse(response, sizeof(response)) == HAL_OK)
        {
            char *subrecv = strstr(response, "+MQTTSUBRECV");
            if(subrecv)
            {
                /* 从 +MQTTSUBRECV 之后找 '{', 避免匹配到回显中的 payload */
                char *json = strchr(subrecv, '{');
                if(json)
                {
                    char buf[512];
                    strncpy(buf, json, 511);
                    buf[511] = '\0';
                    printf("[MQTT] OTA notify: %s\r\n", buf);
                    osMessageQueuePut(ota_notify_queue, buf, 0, 0);
                    AT_ClearRxBuffer(); /* 防止同一消息重复入列 */
                }
            }
        }
        osDelay(200);
    }
}

