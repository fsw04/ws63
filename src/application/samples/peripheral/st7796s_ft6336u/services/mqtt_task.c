#include "mqtt_task.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "MQTTClient.h"
#include "securec.h"
#include "soc_osal.h"
#include "vitals_report.h"
#include "../model/watch_model.h"

#define MQTT_SERVER_URL "tcp://192.168.43.110:1883"
#define MQTT_BROKER_TEXT "192.168.43.110:1883"
#define MQTT_CLIENT_ID "ws63_watch_device_01"
#define MQTT_USERNAME "admin"
#define MQTT_PASSWORD "admin"
#define MQTT_TOPIC_SUB "watch/commands"
#define MQTT_TOPIC_PUB "watch/sensors/report"
#define MQTT_QUEUE_LEN 16
#define MQTT_QUEUE_MSG_SIZE 1024
#define MQTT_TASK_STACK_SIZE 0x2000
#define MQTT_TASK_PRIORITY 25
#define MQTT_TASK_STOP_MSG "__STOP__"
#define MQTT_PUBLISH_TIMEOUT_MS 5000

extern int MQTTClient_init(void);
extern void MQTTClient_cleanup(void);

unsigned long g_mqtt_msg_queue = 0;
static MQTTClient g_mqtt_client;
static uint8_t g_mqtt_connected = 0;
static uint8_t g_mqtt_task_running = 0;
static uint8_t g_mqtt_stop_requested = 0;

static int mqtt_message_arrived(void *context, char *topic_name, int topic_len, MQTTClient_message *message)
{
    (void)context;
    (void)topic_len;

    osal_printk("[MQTT] command topic=%s payload=%.*s\r\n",
                topic_name, message->payloadlen, (char *)message->payload);
    watch_model_add_log(WATCH_LOG_MQTT, "MQTT", "command arrived");
    return 1;
}

bool mqtt_task_is_connected(void)
{
    return g_mqtt_connected != 0;
}

errcode_t mqtt_task_enqueue_report(const char *payload)
{
    uint32_t len;

    if ((payload == NULL) || (payload[0] == '\0') || (g_mqtt_msg_queue == 0) || (g_mqtt_connected == 0)) {
        osal_printk("[MQTT] enqueue skipped payload=%p queue=0x%x connected=%u\r\n",
                    payload, (unsigned int)g_mqtt_msg_queue, (unsigned int)g_mqtt_connected);
        return ERRCODE_FAIL;
    }

    len = (uint32_t)strlen(payload) + 1;
    if (len > MQTT_QUEUE_MSG_SIZE) {
        osal_printk("[MQTT] enqueue too large len=%u max=%u\r\n",
                    (unsigned int)len, (unsigned int)MQTT_QUEUE_MSG_SIZE);
        watch_model_add_log(WATCH_LOG_WARNING, "MQTT", "report too large");
        return ERRCODE_FAIL;
    }
    if (osal_msg_queue_write_copy(g_mqtt_msg_queue, (void *)payload, len, 0) != 0) {
        osal_printk("[MQTT] enqueue failed len=%u\r\n", (unsigned int)len);
        watch_model_add_log(WATCH_LOG_WARNING, "MQTT", "queue full");
        return ERRCODE_FAIL;
    }
    osal_printk("[MQTT] enqueue report len=%u\r\n", (unsigned int)(len - 1));
    return ERRCODE_SUCC;
}

errcode_t mqtt_task_publish(const char *payload)
{
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;
    int rc;

    if ((payload == NULL) || (g_mqtt_connected == 0)) {
        return ERRCODE_FAIL;
    }

    pubmsg.payload = (void *)payload;
    pubmsg.payloadlen = (int)strlen(payload);
    pubmsg.qos = 1;
    pubmsg.retained = 0;
    rc = MQTTClient_publishMessage(g_mqtt_client, MQTT_TOPIC_PUB, &pubmsg, &token);
    osal_printk("[MQTT] publish topic=%s len=%u rc=%d token=%d\r\n",
                MQTT_TOPIC_PUB, (unsigned int)pubmsg.payloadlen, rc, token);
    if (rc != MQTTCLIENT_SUCCESS) {
        watch_model_add_log(WATCH_LOG_WARNING, "MQTT", "publish failed");
        return ERRCODE_FAIL;
    }

    rc = MQTTClient_waitForCompletion(g_mqtt_client, token, MQTT_PUBLISH_TIMEOUT_MS);
    osal_printk("[MQTT] publish wait token=%d rc=%d\r\n", token, rc);
    if (rc != MQTTCLIENT_SUCCESS) {
        watch_model_add_log(WATCH_LOG_WARNING, "MQTT", "publish ack failed");
        return ERRCODE_FAIL;
    }

    watch_model_set_mqtt(WATCH_LINK_CONNECTED, MQTT_BROKER_TEXT, MQTT_TOPIC_PUB, "just now");
    watch_model_add_log(WATCH_LOG_MQTT, "MQTT", "published report");
    return ERRCODE_SUCC;
}

void mqtt_task_stop(void)
{
    if (g_mqtt_task_running == 0) {
        g_mqtt_stop_requested = 0;
        return;
    }

    g_mqtt_stop_requested = 1;
    if ((g_mqtt_msg_queue != 0) && (g_mqtt_connected != 0)) {
        (void)osal_msg_queue_write_copy(g_mqtt_msg_queue, (void *)MQTT_TASK_STOP_MSG,
                                        (unsigned int)sizeof(MQTT_TASK_STOP_MSG), 0);
    }
}

static void *mqtt_main_task(const char *arg)
{
    (void)arg;
    int rc;
    char recv_buf[MQTT_QUEUE_MSG_SIZE];

    if (g_mqtt_msg_queue == 0) {
        (void)osal_msg_queue_create("mqtt_queue", MQTT_QUEUE_LEN, &g_mqtt_msg_queue, 0, MQTT_QUEUE_MSG_SIZE);
    }
    if (g_mqtt_stop_requested != 0) {
        g_mqtt_task_running = 0;
        return NULL;
    }

    watch_model_set_mqtt(WATCH_LINK_CONNECTING, MQTT_BROKER_TEXT, MQTT_TOPIC_PUB, "--");
    watch_model_add_log(WATCH_LOG_MQTT, "MQTT", "connecting broker");

    rc = MQTTClient_init();
    if (rc != MQTTCLIENT_SUCCESS) {
        watch_model_set_mqtt(WATCH_LINK_ERROR, MQTT_BROKER_TEXT, MQTT_TOPIC_PUB, "--");
        watch_model_add_log(WATCH_LOG_WARNING, "MQTT", "client init failed");
        g_mqtt_task_running = 0;
        return NULL;
    }
    if (g_mqtt_stop_requested != 0) {
        MQTTClient_cleanup();
        g_mqtt_task_running = 0;
        return NULL;
    }

    rc = MQTTClient_create(&g_mqtt_client, MQTT_SERVER_URL, MQTT_CLIENT_ID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    if (rc != MQTTCLIENT_SUCCESS) {
        watch_model_set_mqtt(WATCH_LINK_ERROR, MQTT_BROKER_TEXT, MQTT_TOPIC_PUB, "--");
        watch_model_add_log(WATCH_LOG_WARNING, "MQTT", "client create failed");
        (void)MQTTClient_cleanup();
        g_mqtt_task_running = 0;
        return NULL;
    }

    MQTTClient_setCallbacks(g_mqtt_client, NULL, NULL, mqtt_message_arrived, NULL);
    if (g_mqtt_stop_requested != 0) {
        MQTTClient_destroy(&g_mqtt_client);
        MQTTClient_cleanup();
        g_mqtt_task_running = 0;
        return NULL;
    }

    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    conn_opts.keepAliveInterval = 60;
    conn_opts.cleansession = 1;
    conn_opts.username = MQTT_USERNAME;
    conn_opts.password = MQTT_PASSWORD;

    rc = MQTTClient_connect(g_mqtt_client, &conn_opts);
    if (rc != MQTTCLIENT_SUCCESS) {
        watch_model_set_mqtt(WATCH_LINK_ERROR, MQTT_BROKER_TEXT, MQTT_TOPIC_PUB, "--");
        watch_model_add_log(WATCH_LOG_WARNING, "MQTT", "connect failed");
        MQTTClient_destroy(&g_mqtt_client);
        MQTTClient_cleanup();
        g_mqtt_task_running = 0;
        return NULL;
    }

    g_mqtt_connected = 1;
    g_mqtt_task_running = 1;
    watch_model_set_mqtt(WATCH_LINK_CONNECTED, MQTT_BROKER_TEXT, MQTT_TOPIC_PUB, "--");
    watch_model_add_log(WATCH_LOG_MQTT, "MQTT", "subscribed commands");
    (void)MQTTClient_subscribe(g_mqtt_client, MQTT_TOPIC_SUB, 1);
    vitals_report_flush_pending();

    while (1) {
        static uint32_t read_fail_count = 0;
        unsigned int read_size = sizeof(recv_buf);
        int ret = osal_msg_queue_read_copy(g_mqtt_msg_queue, recv_buf, &read_size, OSAL_WAIT_FOREVER);
        if (ret != 0) {
            read_fail_count++;
            if ((read_fail_count == 1U) || ((read_fail_count % 100U) == 0U)) {
                osal_printk("[MQTT] queue read failed ret=%d size=%u queue=0x%x\r\n",
                            ret, read_size, (unsigned int)g_mqtt_msg_queue);
            }
            osal_msleep(20);
            continue;
        }

        read_fail_count = 0;
        if (read_size >= sizeof(recv_buf)) {
            recv_buf[sizeof(recv_buf) - 1] = '\0';
        } else {
            recv_buf[read_size] = '\0';
        }
        if (strcmp(recv_buf, MQTT_TASK_STOP_MSG) == 0) {
            break;
        }
        osal_printk("[MQTT] queue recv len=%u payload:%s\r\n",
                    (unsigned int)strlen(recv_buf), recv_buf);
        if (mqtt_task_publish(recv_buf) != ERRCODE_SUCC) {
            (void)vitals_report_cache_pending(recv_buf);
        }
    }

    if (g_mqtt_connected != 0) {
        (void)MQTTClient_disconnect(g_mqtt_client, 10000);
    }
    MQTTClient_destroy(&g_mqtt_client);
    MQTTClient_cleanup();
    g_mqtt_connected = 0;
    g_mqtt_task_running = 0;
    watch_model_set_mqtt(WATCH_LINK_DISCONNECTED, MQTT_BROKER_TEXT, MQTT_TOPIC_PUB, "--");
    watch_model_add_log(WATCH_LOG_MQTT, "MQTT", "stopped");

    return NULL;
}

void mqtt_task_start(void)
{
    osal_task *task_handle = NULL;

    if (g_mqtt_task_running != 0) {
        return;
    }

    g_mqtt_stop_requested = 0;
    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)mqtt_main_task, 0, "WatchMqttTask", MQTT_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, MQTT_TASK_PRIORITY);
        osal_kfree(task_handle);
        g_mqtt_task_running = 1;
    }
    osal_kthread_unlock();
}
