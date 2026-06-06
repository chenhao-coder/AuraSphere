#include "aliyun_auth.h"
#include "md.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

// ²Î¿¼Á´½Ó£ºhttps://help.aliyun.com/zh/iot/user-guide/establish-mqtt-connections-over-tcp?spm=a2c4g.11186623.help-menu-30520.d_2_2_7_0_3.401074f5g1NRMT
void Aliyun_CalcMqttParams(char *client_id, char *username, char *password, const char *timestamp)
{
    const char *client_id_value = PRODUCT_KEY "." DEVICE_NAME;

    snprintf(client_id, 256, 
        "%s|securemode=2,signmethod=hmacsha256,timestamp=%s|",
        client_id_value, timestamp);
    
    snprintf(username, 128,
      "%s&%s", DEVICE_NAME, PRODUCT_KEY);

    char content[256];
    snprintf(content, sizeof(content),
      "clientId%sdeviceName%sproductKey%stimestamp%s",
      client_id_value, DEVICE_NAME, PRODUCT_KEY, timestamp);

    uint8_t hmac[32];
    mbedtls_md_hmac(
        mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
        (uint8_t*)DEVICE_SECRET, strlen(DEVICE_SECRET),
        (uint8_t*)content,       strlen(content),
        hmac);

    for (int i = 0; i < 32; i++)
        sprintf(password + i*2, "%02x", hmac[i]);
}