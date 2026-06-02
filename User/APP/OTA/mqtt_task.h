#ifndef MQTT_TASK_H
#define MQTT_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "cmsis_os.h"
#include "aliyun_auth.h"

#define OTA_TOPIC_UP  "/ota/device/upgrade/" PRODUCT_KEY "/" DEVICE_NAME
#define OTA_TOPIC_IN  "/ota/device/inform/"  PRODUCT_KEY "/" DEVICE_NAME

extern osMessageQueueId_t ota_notify_queue;  

void MQTT_Task(void *arg);


#ifdef __cplusplus
}
#endif

#endif // MQTT_TASK_H