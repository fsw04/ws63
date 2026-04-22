/**
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2022. All rights reserved.
 *
 * Description: Watchdevice Main Entry (Sensor + SLE Client)
 */

#include "app_init.h"
#include "soc_osal.h"
#include "common_def.h"

// 引入我们自己写的传感器任务启动函数
extern void sensor_task_start(void);

// 这个函数是我们要注册给系统启动框架的入口函数
static void watchdevice_app_entry(void)
{
    osal_printk("\r\n================================\r\n");
    osal_printk("   WatchDevice (SLE Client) Start!      \r\n");
    osal_printk("================================\r\n\n");

    // 1. 启动传感器采集任务
    // 它会每隔 5 秒读取一次体检数据，并调用 sle_client_send_data() 通过星闪发给网关
    sensor_task_start();

    // 2. 注意：星闪客户端任务 (SLE_Client_Task) 不需要在这里启动。
    // 因为在 sle_speed_client.c 的最底部，官方已经写了 app_run(sle_speed_entry);
    // 所以系统在底层初始化完蓝牙/星闪协议栈后，会自动拉起星闪寻找和连接任务。
}

/* 注册入口函数，交给海思 LiteOS 底层框架在开机时自动调用 */
app_run(watchdevice_app_entry);