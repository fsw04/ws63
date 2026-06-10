#ifndef WIFI_TASK_H
#define WIFI_TASK_H

#include <stdbool.h>
#include "errcode.h"
#include "wifi_device_config.h"

typedef struct {
    char ssid[WIFI_MAX_SSID_LEN];
    char pwd[WIFI_MAX_KEY_LEN];
} wifi_profile_t;

void wifi_task_start(const char *default_ssid, const char *default_pwd);
bool wifi_task_get_saved_profile(wifi_profile_t *profile);
errcode_t wifi_task_request_connect(const char *ssid, const char *pwd);
errcode_t wifi_task_request_disconnect(void);
errcode_t wifi_connect_start(const char *ssid, const char *pwd);

#endif
