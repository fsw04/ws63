/**
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2025-2025. All rights reserved. \n
 *
 * Description: Application core main function for standard \n
 * Author:  \n
 * History: \n
 * 2025-08-15, Create file. \n
 */

#include "lwip/netifapi.h"
#include "wifi_hotspot.h"
#include "wifi_hotspot_config.h"
#include "td_base.h"
#include "td_type.h"
#include "stdlib.h"
#include "uart.h"
#include "cmsis_os2.h"
#include "app_init.h"
#include "tiot_service_interface.h"
#include "chip_io.h"
#include "osal_task.h"
#include "osal_msgqueue.h"
#include "securec.h"
#include "tcxo.h"
#include "soc_osal.h"
#include "gpio.h"
#include "pinctrl.h"

#define EXPECT_AP_SSID                      "my_softAP"
#define EXPECT_AP_PWD                       "123456789"

#define WIFI_IFNAME_MAX_SIZE                16
#define WIFI_MAX_SSID_LEN                   33
#define WIFI_SCAN_AP_NUM_LIMIT              64
#define WIFI_MAC_LEN                        6
#define WIFI_NOT_AVALLIABLE                 0
#define WIFI_GET_IP_MAX_COUNT               300

#define WIFI_RECONNECT_ENABLE               1
#define WIFI_RECONNECT_SECOND               65535       // 代表无限次循环重连
#define WIFI_RECONNECT_PERIOD               1
#define WIFI_RECONNECT_TRY_TIMES            65535

#define WIFI_TASK_PRIO                      (osPriority_t)(13)
#define WIFI_TASK_STACK_SIZE                0x1000

#define SLP_RADAR_SAMPLE_LOG                "[SLP_RADAR][WIRELESS_SAMPLE]"

static int radar_udp_socket_server_weakref(void) __attribute__ ((weakref("radar_udp_socket_server")));
static int radar_socket_server_weakref(void) __attribute__ ((weakref("radar_socket_server")));
static td_void wifi_scan_state_changed(td_s32 state, td_s32 size);
static td_void wifi_connection_changed(td_s32 state, const wifi_linked_info_stru *info, td_s32 reason_code);

wifi_event_stru g_wifi_event_cb = {
    .wifi_event_connection_changed = wifi_connection_changed,
    .wifi_event_scan_state_changed = wifi_scan_state_changed,
};

typedef enum {
    WIFI_STA_SAMPLE_INIT = 0,       /* 0:初始态 */
    WIFI_STA_SAMPLE_SCANING,        /* 1:扫描中 */
    WIFI_STA_SAMPLE_SCAN_DONE,      /* 2:扫描完成 */
    WIFI_STA_SAMPLE_FOUND_TARGET,   /* 3:匹配到目标AP */
    WIFI_STA_SAMPLE_CONNECTING,     /* 4:连接中 */
    WIFI_STA_SAMPLE_CONNECT_DONE,   /* 5:关联成功 */
    WIFI_STA_SAMPLE_GET_IP,         /* 6:获取IP */
} wifi_state_enum;

static td_u8 g_wifi_state = WIFI_STA_SAMPLE_INIT;

/*****************************************************************************
  STA 扫描事件回调函数
*****************************************************************************/
static td_void wifi_scan_state_changed(td_s32 state, td_s32 size)
{
    UNUSED(state);
    UNUSED(size);
    g_wifi_state = WIFI_STA_SAMPLE_SCAN_DONE;
}

/*****************************************************************************
  STA 关联事件回调函数
*****************************************************************************/
static td_void wifi_connection_changed(td_s32 state, const wifi_linked_info_stru *info, td_s32 reason_code)
{
    UNUSED(info);
    UNUSED(reason_code);

    if (state == WIFI_NOT_AVALLIABLE) {
        osal_printk("%s::Connect fail!. try agin !\r\n", SLP_RADAR_SAMPLE_LOG);
        g_wifi_state = WIFI_STA_SAMPLE_INIT;
    } else {
        g_wifi_state = WIFI_STA_SAMPLE_CONNECT_DONE;
    }
}

/*****************************************************************************
  STA 匹配目标AP
*****************************************************************************/
static td_s32 example_get_match_network(wifi_sta_config_stru *expected_bss)
{
    td_s32  ret;
    td_u32  num = 64; /* 64:扫描到的Wi-Fi网络数量 */
    td_char expected_ssid[] = EXPECT_AP_SSID;
    td_char pd[] = EXPECT_AP_PWD;
    td_bool find_ap = TD_FALSE;
    td_u8   bss_index;
    /* 获取扫描结果 */
    td_u32 scan_len = sizeof(wifi_scan_info_stru) * WIFI_SCAN_AP_NUM_LIMIT;
    wifi_scan_info_stru *result = osal_kmalloc(scan_len, OSAL_GFP_ATOMIC);
    if (result == TD_NULL) {
        return -1;
    }
    memset_s(result, scan_len, 0, scan_len);
    ret = wifi_sta_get_scan_info(result, &num);
    if (ret != 0) {
        osal_kfree(result);
        return -1;
    }
    /* 筛选扫描到的Wi-Fi网络，选择待连接的网络 */
    for (bss_index = 0; bss_index < num; bss_index ++) {
        if (strlen(expected_ssid) == strlen(result[bss_index].ssid)) {
            if (memcmp(expected_ssid, result[bss_index].ssid, strlen(expected_ssid)) == 0) {
                find_ap = TD_TRUE;
                break;
            }
        }
    }
    /* 未找到待连接AP,可以继续尝试扫描或者退出 */
    if (find_ap == TD_FALSE) {
        osal_kfree(result);
        return -1;
    }
    /* 找到网络后复制网络信息和接入密码 */
    if (memcpy_s(expected_bss->ssid, WIFI_MAX_SSID_LEN, expected_ssid, strlen(expected_ssid)) != 0) {
        osal_kfree(result);
        return -1;
    }
    if (memcpy_s(expected_bss->bssid, WIFI_MAC_LEN, result[bss_index].bssid, WIFI_MAC_LEN) != 0) {
        osal_kfree(result);
        return -1;
    }
    expected_bss->security_type = result[bss_index].security_type;
    if (memcpy_s(expected_bss->pre_shared_key, WIFI_MAX_SSID_LEN, pd, strlen(pd)) != 0) {
        osal_kfree(result);
        return -1;
    }
    expected_bss->ip_type = 1; /* 1：IP类型为动态DHCP获取 */
    osal_kfree(result);
    return 0;
}

/*****************************************************************************
  STA DHCP状态查询
*****************************************************************************/
static td_s32 example_check_dhcp_status(struct netif *netif_p, td_u32 *wait_count)
{
    if ((ip_addr_isany(&(netif_p->ip_addr)) == 0) && (*wait_count <= WIFI_GET_IP_MAX_COUNT)) {
        /* DHCP成功 */
        return 0;
    }

    if (*wait_count > WIFI_GET_IP_MAX_COUNT) {
        osal_printk("%s::STA DHCP timeout, try again !.\r\n", SLP_RADAR_SAMPLE_LOG);
        *wait_count = 0;
        g_wifi_state = WIFI_STA_SAMPLE_INIT;
    }
    return -1;
}

/*****************************************************************************
  SOCKET 相关接口
*****************************************************************************/
static void radar_socket_init(void)
{
    if (radar_socket_server_weakref != NULL) {
        radar_socket_server_weakref();
    }
    if (radar_udp_socket_server_weakref != NULL) {
        radar_udp_socket_server_weakref();
    }
}

td_s32 example_sta_function(td_void)
{
    td_char ifname[WIFI_IFNAME_MAX_SIZE + 1] = "wlan0"; /* 创建的STA接口名 */
    wifi_sta_config_stru expected_bss = {0}; /* 连接请求信息 */
    struct netif *netif_p = TD_NULL;
    td_u32 wait_count = 0;

    /* 创建STA接口 */
    if (wifi_sta_enable() != 0) {
        return -1;
    }
    osal_printk("%s::STA enable succ.\r\n", SLP_RADAR_SAMPLE_LOG);

    do {
        (void)osDelay(1); /* 1: 等待10ms后判断状态 */
        if (g_wifi_state == WIFI_STA_SAMPLE_INIT) {
            g_wifi_state = WIFI_STA_SAMPLE_SCANING;
            /* 启动STA扫描 */
            if (wifi_sta_scan() != 0) {
                g_wifi_state = WIFI_STA_SAMPLE_INIT;
                continue;
            }
        } else if (g_wifi_state == WIFI_STA_SAMPLE_SCAN_DONE) {
            /* 获取待连接的网络 */
            if (example_get_match_network(&expected_bss) != 0) {
                osal_printk("%s::Do not find AP, try again !\r\n", SLP_RADAR_SAMPLE_LOG);
                g_wifi_state = WIFI_STA_SAMPLE_INIT;
                continue;
            }
            g_wifi_state = WIFI_STA_SAMPLE_FOUND_TARGET;
        } else if (g_wifi_state == WIFI_STA_SAMPLE_FOUND_TARGET) {
            g_wifi_state = WIFI_STA_SAMPLE_CONNECTING;
            /* 启动连接 */
            if (wifi_sta_connect(&expected_bss) != 0) {
                g_wifi_state = WIFI_STA_SAMPLE_INIT;
                continue;
            }
        } else if (g_wifi_state == WIFI_STA_SAMPLE_CONNECT_DONE) {
            g_wifi_state = WIFI_STA_SAMPLE_GET_IP;
            netif_p = netifapi_netif_find(ifname);
            if (netif_p == TD_NULL || netifapi_dhcp_start(netif_p) != 0) {
                osal_printk("%s::find netif or start DHCP fail, try again !\r\n", SLP_RADAR_SAMPLE_LOG);
                g_wifi_state = WIFI_STA_SAMPLE_INIT;
                continue;
            }
        } else if (g_wifi_state == WIFI_STA_SAMPLE_GET_IP) {
            if (example_check_dhcp_status(netif_p, &wait_count) == 0) {
                break;
            }
            wait_count++;
        }
    } while (1);

    wifi_sta_set_reconnect_policy(WIFI_RECONNECT_ENABLE, WIFI_RECONNECT_SECOND, WIFI_RECONNECT_PERIOD, \
                                  WIFI_RECONNECT_TRY_TIMES);

    radar_socket_init();

    return 0;
}

int sta_sample_init(void *param)
{
    param = param;

    /* 注册事件回调 */
    if (wifi_register_event_cb(&g_wifi_event_cb) != 0) {
        osal_printk("%s::g_wifi_event_cb register fail.\r\n", SLP_RADAR_SAMPLE_LOG);
        return -1;
    }

    /* 等待wifi初始化完成 */
    while (wifi_is_wifi_inited() == 0) {
        (void)osDelay(10); /* 1: 等待100ms后判断状态 */
    }

    if (example_sta_function() != 0) {
        osal_printk("%s::example_sta_function fail.\r\n", SLP_RADAR_SAMPLE_LOG);
        return -1;
    }
    return 0;
}

static void slp_radar_wireless_sample_entry(void)
{
    osThreadAttr_t attr;
    attr.name       = "slp_radar_wireless_sample";
    attr.attr_bits  = 0U;
    attr.cb_mem     = NULL;
    attr.cb_size    = 0U;
    attr.stack_mem  = NULL;
    attr.stack_size = WIFI_TASK_STACK_SIZE;
    attr.priority   = WIFI_TASK_PRIO;
    (void)osThreadNew((osThreadFunc_t)sta_sample_init, NULL, &attr);
}

/* Run the slp_radar_wireless_sample_task. */
app_run(slp_radar_wireless_sample_entry);