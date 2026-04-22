#include "sensor_task.h"
#include "mqtt_task.h"
#include "soc_osal.h"
#include "stdio.h"
#include "string.h"
#include "gpio.h"
#include "pinctrl.h"
#include "osal_debug.h"

// 引入刚刚在 sle_speed_client.c 中新增的发送接口
extern int sle_client_send_data(const char *payload, uint16_t len);

static void *sensor_main_task(const char *arg)
{
    unused(arg);

    osal_msleep(10000); // 刚开机等 10 秒，让底层星闪雷达扫频连接网关

    //while (1) {
        char payload[1024];

        snprintf(payload, sizeof(payload), 
            "{"
            "\"deviceId\": \"watch_01\","
            "\"type\": \"vitals\","
            "\"sessionId\": \"sess_1001\","
            "\"ts\": 1713500000,"
            "\"sentAt\": 1713500000,"
            "\"ver\": \"1.0\","
            "\"data\": {"
                "\"name\": \"李桂芳\","
                "\"phone\": \"13900000002\","
                "\"idCard\": \"110101196104082422\","
                "\"height\": \"153.5 cm\","
                "\"weight\": \"43.6 kg\","
                "\"bmi\": \"18.5 kg/m2\","
                "\"bloodPressure\": \"115/71 mmHg\","
                "\"fastingBloodGlucose\": \"5.23 mmol/L\""
            "}"
            "}"
        );

        // 【核心】不再发往 MQTT，而是发往星闪网关！
        int ret = sle_client_send_data(payload, strlen(payload));
        if (ret == 0) {
            osal_printk("[Sensor] 成功通过星闪发送数据给网关！\r\n");
        } else {
            osal_printk("[Sensor] 发送失败，星闪可能未连接。\r\n");
        }
        
        osal_msleep(5000);
    //}
    return NULL;
}

void sensor_task_start(void)
{
    osal_task *task_handle = NULL;
    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)sensor_main_task, 0, "Sensor_Task", 0x1000);
    if (task_handle != NULL) osal_kthread_set_priority(task_handle, 26);
    osal_kthread_unlock();
}