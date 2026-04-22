#include "sensor_task.h"
#include "mqtt_task.h"
#include "soc_osal.h"
#include "stdio.h"
#include "string.h"
#include "gpio.h"
#include "pinctrl.h"
#include "osal_debug.h"
#include "MQTTClient.h"

extern MQTTClient client;

static void *sensor_main_task(const char *arg)
{
    unused(arg);

    osal_msleep(3000); // 等待 MQTT 连接成功

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

        // 【改 1】结构体名改为 MQTTClient_message
        MQTTClient_message message = MQTTClient_message_initializer;

        // 【改 2】QoS 直接用 0（不是 QOS0）
        message.qos = 0;
        message.retained = 0;
        message.payload = payload;
        message.payloadlen = (int)strlen(payload);

        // 【改 3】增加第 4 个参数 deliveryToken
        MQTTClient_deliveryToken dt;
        int rc = MQTTClient_publishMessage(client, "watch/watch_01/up", &message, &dt);

        if (rc == 0) {
            osal_printk("[MQTT] 成功发布李桂芳体检数据！\r\n");
        } else {
            osal_printk("[MQTT] 发布失败，错误码: %d\r\n", rc);
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