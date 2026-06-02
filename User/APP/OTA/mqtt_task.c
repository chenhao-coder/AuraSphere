#include "at_driver.h"
#include "mqtt_task.h"
#include <string.h>

static const char *ssid = "CU_bDHQ";
static const char *pwd = "kba26t4u";

#define ALIYUN_HOST     PRODUCT_KEY ".iot-as-mqtt.cn-shanghai.aliyuncs.com"
#define ALIYUN_PORT  1883

static void EscapeEspAtMqttField(const char *src, char *dst, size_t dst_size)
{
    size_t j = 0;

    if (dst_size == 0)
        return;

    for (size_t i = 0; src[i] != '\0' && j + 1 < dst_size; i++)
    {
        if (src[i] == ',' && j + 3 < dst_size)
        {
            dst[j++] = '\\';
            dst[j++] = '\\';
            dst[j++] = ',';
        }
        else
        {
            dst[j++] = src[i];
        }
    }

    dst[j] = '\0';
}

osMessageQueueId_t ota_notify_queue;        /* 传递 OTA JSON 给 OTA 任务 */
const osMessageQueueAttr_t ota_queue_attributes = {
  .name = "ota_notify_queue"
};

void MQTT_Task(void *arg)
{
    AT_init();
    osDelay(1000);
    ota_notify_queue = osMessageQueueNew(2, 512, &ota_queue_attributes);

    /* Step1: 连接 Wi-Fi */
    while(AT_ConnectWiFi(ssid, pwd) != HAL_OK)
    {
        printf("[MQTT] WiFi connecting...\r\n");
        osDelay(3000);
    }
    printf("[MQTT] WiFi OK\r\n");
    osDelay(1000);

    /* Step2: 配置 MQTT 参数 */
    char client_id[256], username[128], password[65];
    Aliyun_CalcMqttParams(client_id, username, password);
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

    /* Step3: 连接 Broker */
    snprintf(cmd, sizeof(cmd), "AT+MQTTCONN=0,\"%s\",%d,1", ALIYUN_HOST, ALIYUN_PORT);
    while (AT_Send(cmd, "+MQTTCONNECTED", 10000) != HAL_OK)
    {
        printf("[MQTT] Connecting...\r\n");
        osDelay(3000);
    }
    printf("[MQTT] Connected!\r\n");

    /* Step4: 订阅 OTA Topic */
    snprintf(cmd, sizeof(cmd),
        "AT+MQTTSUB=0,\"%s\",1", OTA_TOPIC_UP);
    AT_Send(cmd, "OK", 5000);

    /* Step5: 循环监听消息 */
    char response[AT_RX_BUF_SIZE];
    for(;;)
    {
        /* 检查 at_response 是否含 MQTTSUBRECV */
        if(AT_GetLastResponse(response, sizeof(response)) == HAL_OK)
        {
            if(strstr(response, "+MQTTSUBRECV"))
            {
                char *json = strchr(response, '{');
                if(json)
                {
                    char buf[512];
                    strncpy(buf, json, 511);
                    osMessageQueuePut(ota_notify_queue, buf, 0, 0);
                }
            }
        }
        osDelay(200);
    }
}

