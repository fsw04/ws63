#ifndef WIFI_TASK_H
#define WIFI_TASK_H

#include "errcode.h"

// 启动 Wi-Fi 连接并阻塞等待获取 IP
errcode_t wifi_connect_start(const char *ssid, const char *pwd);

#endif // WIFI_TASK_H