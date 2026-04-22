/**
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2023-2023. All rights reserved.
 */

#include "common_def.h"
#include "soc_osal.h"
#include "app_init.h"

// 引入我们刚才解耦的三个模块头文件
#include "wifi_task.h"
#include "mqtt_task.h"
#include "sensor_task.h"

#define DEFAULT_TASK_STACK_SIZE         0x1000
#define DEFAULT_TASK_PRIORITY           24
#define DELAYS_MS                       1000

// 请替换为你实际的 Wi-Fi 热点信息
#define WIFI_SSID "FSW"
#define WIFI_PWD  "a2821840334"

static void *app_main_task(const char *arg)
{
    unused(arg);
    osal_printk("================================\r\n");
    osal_printk("  Watch Application Start!      \r\n");
    osal_printk("================================\r\n");

    // 1. 阻塞连接 Wi-Fi 并获取 IP 地址
    if (wifi_connect_start(WIFI_SSID, WIFI_PWD) == ERRCODE_SUCC) {
        osal_printk("[APP] Network Ready!\r\n");

        // 2. 网络连通后，启动 MQTT 通信任务
        mqtt_task_start();

        // 3. 启动传感器定时采集任务
        sensor_task_start();
    } else {
        osal_printk("[APP] Network Connect Failed, stop MQTT services.\r\n");
    }

    // 主任务可以进入空闲休眠，不占用 CPU
    for(;;){
        osal_msleep(DELAYS_MS * 10);
    }
    return NULL;
}

static void watch_app_entry(void)
{
    osal_task *task_handle = NULL;
    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)app_main_task, 0, "AppMain_Task", DEFAULT_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, DEFAULT_TASK_PRIORITY);
    }
    osal_kthread_unlock();
}

/* 注册入口函数 */
app_run(watch_app_entry);