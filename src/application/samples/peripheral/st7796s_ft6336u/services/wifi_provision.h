#ifndef WIFI_PROVISION_H
#define WIFI_PROVISION_H

#include <stdbool.h>
#include "errcode.h"

#define WIFI_PROV_AP_SSID "WS63-Setup"
#define WIFI_PROV_AP_PASSWORD "12345678"
#define WIFI_PROV_AP_IP "192.168.63.1"
#define WIFI_PROV_HTTP_PORT 80
#define WIFI_PROV_HTTP_PATH "/provision"

errcode_t wifi_provision_start(void);
void wifi_provision_stop(void);
bool wifi_provision_is_running(void);
const char *wifi_provision_get_ap_ssid(void);
const char *wifi_provision_get_ap_password(void);
const char *wifi_provision_get_ap_ip(void);

#endif
