/*
# Copyright (C) 2024 HiHope Open Source Organization .
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
 */
#include "securec.h"
#include "errcode.h"
#include "osal_addr.h"
#include "../inc/SLE_LED_Server.h"

#include "cmsis_os2.h"
#include "debug_print.h"
#include "soc_osal.h"
#include "app_init.h"
#include "common_def.h"

#include "../inc/aht20_test.h"
#include "../inc/blood.h"

#include "MQTTClient.h"
#include "MQTTClientPersistence.h"
#include "osal_debug.h"
#include "los_memory.h"
#include "los_task.h"
#include "../inc/wifi_connect.h"
#include "watchdog.h"
#include "../inc/cjson_demo.h"
#include "../inc/pwm_demo.h"

#include "pinctrl.h"
#include "gpio.h"
#include "hal_gpio.h"

extern int heart;
extern int32_t spo2;

#define ADDRESS "tcp://2084118ebf.st1.iotda-device.cn-north-4.myhuaweicloud.com"
#define CLIENTID "67f67d422902516e866fd005_147852369_0_0_2025050812"
#define TOPIC "MQTT Examples"
#define PAYLOAD "Hello World!"
#define QOS 1

#define CONFIG_WIFI_SSID "P40 Pro+"
#define CONFIG_WIFI_PWD "Zyh114785"
#define MQTT_STA_TASK_PRIO 24
#define MQTT_STA_TASK_STACK_SIZE 0x1000
#define TIMEOUT 10000L
#define MSG_MAX_LEN 28
#define MSG_QUEUE_SIZE 32

static unsigned long g_msg_queue;
volatile MQTTClient_deliveryToken deliveredtoken;
char *g_username = "67f67d422902516e866fd005_147852369";
char *g_password = "32df7b42b6a8a8e3c7cd6240844c2acb725020d1d6175fb755783bf02b5949b5";
MQTTClient client;
extern int MQTTClient_init(void);

MQTT_msg *mqtt_msg;
bloodmsg aht_msg;

/* ========== MQTT ========== */
void delivered(void *context, MQTTClient_deliveryToken dt)
{
    unused(context);
    printf("Message with token value %d delivery confirmed\r\n", dt);
    deliveredtoken = dt;
}

int msgArrved(void *context, char *topic_name, int topic_len, MQTTClient_message *message)
{
    unused(context);
    unused(topic_len);
    MQTT_msg *receive_msg = osal_kmalloc(sizeof(MQTT_msg), 0);
    printf("mqtt_message_arrive() success, the topic is %s, the payload is %s \n", topic_name, message->payload);
    receive_msg->msg_type = EN_MSG_PARS;
    receive_msg->receive_payload = message->payload;
    uint32_t ret = osal_msg_queue_write_copy(g_msg_queue, receive_msg, sizeof(MQTT_msg), OSAL_WAIT_FOREVER);
    if (ret != 0) {
        printf("ret = %#x\r\n", ret);
        osal_kfree(receive_msg);
        return -1;
    }
    return 1;
}

void connlost(void *context, char *cause)
{
    unused(context);
    printf("mqtt_connection_lost() error, cause: %s\n", cause);
}

int mqtt_subscribe(const char *topic)
{
    printf("subscribe start\r\n");
    MQTTClient_subscribe(client, topic, QOS);
    return 0;
}

int mqtt_publish(const char *topic, MQTT_msg *report_msg)
{
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;
    int rc = 0;
    char *msg = make_json("xinxue", report_msg->xin, report_msg->xue);
    pubmsg.payload = msg;
    pubmsg.payloadlen = (int)strlen(msg);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;
    rc = MQTTClient_publishMessage(client, topic, &pubmsg, &token);
    if (rc != MQTTCLIENT_SUCCESS) {
        printf("mqtt_publish failed\r\n");
    }
    printf("mqtt_publish(), the payload is %s, the topic is %s\r\n", msg, topic);
    osal_kfree(msg);
    msg = NULL;
    return rc;
}

int mqtt_connect(void)
{
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;
    printf("start mqtt sync subscribe...\r\n");
    MQTTClient_init();
    MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    conn_opts.keepAliveInterval = 120;
    conn_opts.cleansession = 1;
    if (g_username != NULL) {
        conn_opts.username = g_username;
        conn_opts.password = g_password;
    }
    MQTTClient_setCallbacks(client, NULL, connlost, msgArrved, delivered);
    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to connect, return code %d\n", rc);
        return -1;
    }
    printf("connect success\r\n");
    return rc;
}

int mqtt_task(void)
{
    MQTT_msg *report_msg = osal_kmalloc(sizeof(MQTT_msg), 0);
    int ret = 0;
    int biao = 0;
    uint32_t resize = 32;
    char *beep_status = NULL;

    wifi_connect(CONFIG_WIFI_SSID, CONFIG_WIFI_PWD);
    ret = mqtt_connect();
    if (ret != 0) {
        printf("connect failed, result %d\n", ret);
    }
    osal_msleep(1000);
    char *cmd_topic = combine_strings(3, "$oc/devices/", g_username, "/sys/commands/#");
    ret = mqtt_subscribe(cmd_topic);
    if (ret < 0) {
        printf("subscribe topic error, result %d\n", ret);
    }
    char *report_topic = combine_strings(3, "$oc/devices/", g_username, "/sys/properties/report");
    while (1) {
        biao = 1;
        ret = osal_msg_queue_read_copy(g_msg_queue, report_msg, &resize, OSAL_WAIT_FOREVER);
        if (ret != 0) {
            printf("queue_read ret = %#x\r\n", ret);
            osal_kfree(report_msg);
            biao = 0;
        }
        if (report_msg != NULL && biao) {
            printf("report_msg->msg_type = %d, report_msg->temp = %s\r\n", report_msg->msg_type, report_msg->xin);
            switch (report_msg->msg_type) {
                case EN_MSG_PARS:
                    beep_status = parse_json(report_msg->receive_payload);
                    pwm_task(beep_status);
                    break;
                case EN_MSG_REPORT:
                    mqtt_publish(report_topic, report_msg);
                    break;
                default:
                    break;
            }
            osal_kfree(report_msg);
        }
        osal_msleep(1000);
    }
    return ret;
}

/* ========== MAX30102 + OLED ========== */
#define BSP_LED 7
int MAX30102_task(void)
{
    (void)osal_msleep(5000);

    int x2;
    int y2;
    mqtt_msg = osal_kmalloc(sizeof(MQTT_msg), 0);
    if (mqtt_msg == NULL) {
        printf("Memory allocation failed\r\n");
    }
    Aht20TestTask();
    uapi_pin_set_mode(BSP_LED, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(BSP_LED, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(BSP_LED, GPIO_LEVEL_LOW);
    while (1) {
        blood_Loop();
        x2 = heart;
        y2 = spo2;
        sever_xianshi(x2, y2);

        if (x2 > 0) {
            uapi_gpio_set_val(BSP_LED, GPIO_LEVEL_HIGH);
        }

        aht_msg.temperature = heart;
        aht_msg.humidity = spo2;
        mqtt_msg->msg_type = EN_MSG_REPORT;

        if ((mqtt_msg != NULL) && (aht_msg.temperature != 0) && (aht_msg.humidity != 0)) {
            sprintf(mqtt_msg->xin, "%.2f", aht_msg.temperature);
            sprintf(mqtt_msg->xue, "%.2f", aht_msg.humidity);
            printf("xinlv = %s, xueyang= %s\r\n", mqtt_msg->xin, mqtt_msg->xue);
            uint32_t ret = osal_msg_queue_write_copy(g_msg_queue, mqtt_msg, sizeof(MQTT_msg), OSAL_WAIT_FOREVER);
            if (ret != 0) {
                printf("ret = %#x\r\n", ret);
                osal_kfree(mqtt_msg);
                break;
            }
        }
        osal_msleep(500);
    }
    return 0;
}

/* ========== 入口 ========== */
#define SLE_LED_SER_TASK_PRIO 24
#define SLE_LED_SER_STACK_SIZE 0x2000

static void example_sle_led_server_entry(void)
{
    uint32_t ret;
    uapi_watchdog_disable();
    ret = osal_msg_queue_create("name", MSG_QUEUE_SIZE, &g_msg_queue, 0, MSG_MAX_LEN);
    if (ret != OSAL_SUCCESS) {
        printf("create queue failure!,error:%x\n", ret);
    }
    printf("create the queue success! queue_id = %d\n", g_msg_queue);

    osal_task *task_handle = NULL;
    osal_kthread_lock();

    task_handle = osal_kthread_create((osal_kthread_handler)MAX30102_task, 0, "MAX30102_task", SLE_LED_SER_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, SLE_LED_SER_TASK_PRIO);
        osal_kfree(task_handle);
    }

    osal_kthread_unlock();
}

app_run(example_sle_led_server_entry);