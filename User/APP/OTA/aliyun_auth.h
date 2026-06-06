#ifndef ALIYUN_AUTH_H
#define ALIYUN_AUTH_H

#ifdef __cplusplus
extern "C" {
#endif

#define PRODUCT_KEY     "a1ffX35paXM"
#define DEVICE_NAME     "my_stm32_device"
#define DEVICE_SECRET   "75e5d530de95fff689f582ecad1eef94"

void Aliyun_CalcMqttParams(char *client_id, char *username, char *password, const char *timestamp);

#ifdef __cplusplus
}
#endif

#endif // ALIYUN_AUTH_H
