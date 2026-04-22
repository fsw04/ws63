/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2025-2025. All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

/**
 *    @file
 *          Provides hisilicon wifi utils
 */
#include "WifiUtils.h"
#include "securec.h"
#include "errcode.h"
#include "cmsis_os2.h"
#include "lwip/netifapi.h"
#include "osal_addr.h"

enum {
    WIFI_STA_SAMPLE_INIT = 0,       /* 0:初始态 */
    WIFI_STA_SAMPLE_SCANING,        /* 1:扫描中 */
    WIFI_STA_SAMPLE_SCAN_DONE,      /* 2:扫描完成 */
    WIFI_STA_SAMPLE_FOUND_TARGET,   /* 3:匹配到目标AP */
    WIFI_STA_SAMPLE_CONNECTING,     /* 4:连接中 */
    WIFI_STA_SAMPLE_CONNECT_DONE,   /* 5:关联成功 */
    WIFI_STA_SAMPLE_GET_IP,         /* 6:获取IP */
} wifi_state_enum;

constexpr int WIFI_GET_IP_MAX_COUNT = 300;
constexpr int WIFI_SCAN_AP_LIMIT = 64;
constexpr int WIFI_NOT_AVALLIABLE = 0;

static void wifi_scan_state_changed(int32_t state, int32_t size);
static void wifi_connection_changed(int32_t state, const wifi_linked_info_stru *info, int32_t reason_code);

wifi_event_stru wifi_event_cb = {
    .wifi_event_connection_changed      = wifi_connection_changed,
    .wifi_event_scan_state_changed      = wifi_scan_state_changed,
};

static uint8_t g_wifi_state = WIFI_STA_SAMPLE_INIT;
/*****************************************************************************
  STA 扫描事件回调函数
*****************************************************************************/
static void wifi_scan_state_changed(int32_t state, int32_t size)
{
    ChipLogProgress(DeviceLayer, "HisiWiFiDriver::%s state:%d\r\n", __func__, state);
    g_wifi_state = WIFI_STA_SAMPLE_SCAN_DONE;
    return;
}

/*****************************************************************************
  STA 关联事件回调函数
*****************************************************************************/
static void wifi_connection_changed(int32_t state, const wifi_linked_info_stru *info, int32_t reason_code)
{
    if (state == WIFI_NOT_AVALLIABLE) {
        ChipLogProgress(DeviceLayer, "HisiWiFiDriver::%s state:%d\r\n", __func__, state);
        g_wifi_state = WIFI_STA_SAMPLE_SCAN_DONE;
    } else {
        ChipLogProgress(DeviceLayer, "HisiWiFiDriver::%s state:%d\r\n", __func__, state);
        g_wifi_state = WIFI_STA_SAMPLE_CONNECT_DONE;
    }
    return;
}

CHIP_ERROR WifiUtilsInit()
{
    errcode_t ret = wifi_register_event_cb(&wifi_event_cb);
    if (ret != 0) {
        ChipLogProgress(DeviceLayer, "HisiWiFiDriver::%s wifi_register_event_cb failed\r\n", __func__);
    }
        /* 等待wifi初始化完成 */
    while (wifi_is_wifi_inited() == 0) {
        (void)osDelay(10); /* 1: 等待100ms后判断状态 */
    }

    ChipLogProgress(DeviceLayer, "WifiUtilsInit OK\r\n");

    return CHIP_NO_ERROR;
}

/*****************************************************************************
  STA DHCP状态查询
*****************************************************************************/
bool wifi_check_dhcp_status(struct netif *netif_p, uint32_t *wait_count)
{
    if ((ip_addr_isany(&(netif_p->ip_addr)) == 0) && (*wait_count <= WIFI_GET_IP_MAX_COUNT)) {
        /* DHCP成功 */
        ChipLogProgress(NetworkProvisioning, "%s::STA DHCP success.\r\n", __func__);
        return 0;
    }

    if (*wait_count > WIFI_GET_IP_MAX_COUNT) {
        ChipLogProgress(NetworkProvisioning, "%s::STA DHCP timeout, try again !.\r\n", __func__);
        *wait_count = 0;
        g_wifi_state = WIFI_STA_SAMPLE_INIT;
    }
    return -1;
}

/*****************************************************************************
  STA 匹配目标AP
*****************************************************************************/
int32_t wifi_get_match_network(wifi_sta_config_stru *expected_bss)
{
    int32_t  ret;
    uint32_t  num = 64; /* 64:扫描到的Wi-Fi网络数量 */
    char expected_ssid[WIFI_MAX_SSID_LEN] = {0};

    bool find_ap = false;
    uint8_t   bss_index;
    /* 获取扫描结果 */
    uint32_t scan_len = sizeof(wifi_scan_info_stru) * WIFI_SCAN_AP_LIMIT;
    wifi_scan_info_stru *result = (wifi_scan_info_stru*)osal_kmalloc(scan_len, OSAL_GFP_ATOMIC);
    if (result == NULL) {
        return -1;
    }
    memset_s(result, scan_len, 0, scan_len);
    ret = wifi_sta_get_scan_info(result, &num);
    if (ret != 0) {
        osal_kfree(result);
        return -1;
    }
    memcpy_s(expected_ssid, WIFI_MAX_SSID_LEN, expected_bss->ssid, WIFI_MAX_SSID_LEN);
    /* 筛选扫描到的Wi-Fi网络，选择待连接的网络 */
    for (bss_index = 0; bss_index < num; bss_index ++) {
        if (strlen(expected_ssid) == strlen(result[bss_index].ssid)) {
            if (memcmp(expected_ssid, result[bss_index].ssid, strlen(expected_ssid)) == 0) {
                find_ap = true;
                break;
            }
        }
    }
    /* 未找到待连接AP,可以继续尝试扫描或者退出 */
    if (find_ap == false) {
        osal_kfree(result);
        return -1;
    }

    if (memcpy_s(expected_bss->bssid, WIFI_MAC_LEN, result[bss_index].bssid, WIFI_MAC_LEN) != 0) {
        osal_kfree(result);
        return -1;
    }
    expected_bss->security_type = result[bss_index].security_type;
    ChipLogProgress(NetworkProvisioning, "wifi_get_match_network  network ssid:%.*s", WIFI_MAX_SSID_LEN, result[bss_index].ssid);

    expected_bss->ip_type = DHCP; /* 1：IP类型为动态DHCP获取 */
    osal_kfree(result);
    ChipLogProgress(NetworkProvisioning, "wifi_get_match_network OK\r\n");
    return 0;
}

CHIP_ERROR  WifiUtilsConnectNetwork(wifi_sta_config_stru *expected_bss)
{
    char ifname[WIFI_IFNAME_MAX_SIZE + 1] = "wlan0";
    struct netif *netif_p = nullptr;
    uint32_t wait_count = 0;
    wifi_sta_config_stru sta_config = {0};
    constexpr int delayTime = 1;
    constexpr int retryTimes = 300;

    memcpy_s(&sta_config, sizeof(wifi_sta_config_stru), expected_bss, sizeof(wifi_sta_config_stru));
    while (1) {
        (void)osDelay(delayTime);
        int32_t res = wifi_sta_scan();
        if (res == 0) {
            if (g_wifi_state == WIFI_STA_SAMPLE_SCAN_DONE) {
                break;
            }
        }
        wait_count++;
        if (wait_count > retryTimes) {
            ChipLogProgress(NetworkProvisioning, "uapi_wifi_sta_scan timeout!");
            g_wifi_state = WIFI_STA_SAMPLE_INIT;
            break;
        }
    }

    wait_count = 0;
    do {
        (void)osDelay(delayTime);
        if (g_wifi_state == WIFI_STA_SAMPLE_SCAN_DONE) {
            /* 获取待连接的网络 */
            if (wifi_get_match_network(&sta_config) != 0) {
                ChipLogProgress(NetworkProvisioning, "Do not find AP, try again.");
                g_wifi_state = WIFI_STA_SAMPLE_INIT;
                continue;
            }
            g_wifi_state = WIFI_STA_SAMPLE_FOUND_TARGET;
        } else if (g_wifi_state == WIFI_STA_SAMPLE_FOUND_TARGET) {
            ChipLogProgress(NetworkProvisioning, "%s::Connect start.\r\n", __func__);
            g_wifi_state = WIFI_STA_SAMPLE_CONNECTING;
            /* 启动连接 */
            sta_config.ip_type = DHCP;
            if (wifi_sta_connect(&sta_config) != 0) {
                ChipLogProgress(NetworkProvisioning, "wifi_sta_connect failed, try again.");
                g_wifi_state = WIFI_STA_SAMPLE_SCAN_DONE;
                continue;
            }
        } else if (g_wifi_state == WIFI_STA_SAMPLE_CONNECT_DONE) {
            ChipLogProgress(NetworkProvisioning, "%s::DHCP start.\r\n", __func__);
            g_wifi_state = WIFI_STA_SAMPLE_GET_IP;
            netif_p = netifapi_netif_find(ifname);
            err_t res = netifapi_dhcp_start(netif_p);
            if (netif_p == NULL || res != 0) {
                ChipLogProgress(NetworkProvisioning, "%s::find netif or start DHCP fail, try again !\r\n", __func__);
                g_wifi_state = WIFI_STA_SAMPLE_FOUND_TARGET;
                continue;
            }
        } else if (g_wifi_state == WIFI_STA_SAMPLE_GET_IP) {
            if (wifi_check_dhcp_status(netif_p, &wait_count) == 0) {
                break;
            }
            wait_count++;
        }
    } while (1);

    return CHIP_NO_ERROR;
}

CHIP_ERROR WifiUtilsStaScan(wifi_scan_params_stru *scan_params)
{
    ChipLogProgress(NetworkProvisioning, "HisiWiFiDriver::%s line:%d\r\n", __func__, __LINE__);
    uint32_t cnt = 0;
    constexpr int delayTime = 1;
    constexpr int retryTimes = 300;
    do {
        osDelay(delayTime);
        if (wifi_sta_scan_advance(scan_params) == 0) {
            break;
        }
        if (cnt > retryTimes) {
            break;
        }
        cnt++;
    } while (1);

    ChipLogProgress(NetworkProvisioning, "HisiWiFiDriver::%s line:%d\r\n", __func__, __LINE__);
    return CHIP_NO_ERROR;
}