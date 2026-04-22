#include "mqtt_task.h"
#include "MQTTClient.h"
#include "soc_osal.h"
#include "string.h"
#include "stdio.h"
#include "common_def.h"

// === 请替换为你自己的华为云或其它云平台的三元组信息 ===
// 华为云示例: "tcp://a162297e00.iot-mqtts.cn-north-4.myhuaweicloud.com:1883"
#define MQTT_SERVER_URL  "tcp://192.168.43.110:1883" 
#define MQTT_CLIENT_ID   "ws63_watch_device_01"                 
#define MQTT_USERNAME    "admin"               
#define MQTT_PASSWORD    "admin"                 

// Topic 可以自定义，不需要遵循华为云的格式了
#define MQTT_TOPIC_SUB   "watch/commands"
#define MQTT_TOPIC_PUB   "watch/sensors/report"

//#define MQTT_TOPIC_SUB   "$oc/devices/your_device_id/sys/commands/#"
//#define MQTT_TOPIC_PUB   "$oc/devices/your_device_id/sys/properties/report"

extern int MQTTClient_init(void);

unsigned long g_mqtt_msg_queue = 0;
MQTTClient client;

// 云端下发命令的回调函数
static int messageArrived(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
    unused(context);   
    unused(topicLen);

    osal_printk("[MQTT] Command Arrived! Topic: %s, Payload: %.*s\r\n", 
                topicName, message->payloadlen, (char*)message->payload);
    // TODO: 在这里解析 JSON，根据云端指令控制硬件（如蜂鸣器、屏幕）
    return 1;
}

static void *mqtt_main_task(const char *arg)
{
    unused(arg);
    int rc;
    
    // 1. 创建消息队列，用于接收传感器任务发来的字符串数据 (最多缓存16条，每条最大128字节)
    osal_msg_queue_create("mqtt_queue", 16, &g_mqtt_msg_queue, 0, 128);

    // 2. 初始化 MQTT 客户端
    MQTTClient_init();
    rc = MQTTClient_create(&client, MQTT_SERVER_URL, MQTT_CLIENT_ID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    if (rc != MQTTCLIENT_SUCCESS) {
        osal_printk("[MQTT] Client create fail: %d\r\n", rc);
        return NULL;
    }

    MQTTClient_setCallbacks(client, NULL, NULL, messageArrived, NULL);

    // 3. 配置连接参数
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    conn_opts.keepAliveInterval = 60;
    conn_opts.cleansession = 1;
    conn_opts.username = MQTT_USERNAME;
    conn_opts.password = MQTT_PASSWORD;

    // 4. 连接到云端
    osal_printk("[MQTT] Connecting to broker...\r\n");
    rc = MQTTClient_connect(client, &conn_opts);
    if (rc != MQTTCLIENT_SUCCESS) {
        osal_printk("[MQTT] Connect fail: %d\r\n", rc);
        return NULL;
    }
    osal_printk("[MQTT] Connected!\r\n");

    // 5. 订阅云端命令主题
    MQTTClient_subscribe(client, MQTT_TOPIC_SUB, 1);

    // 6. 核心循环：阻塞读取消息队列 -> 收到数据 -> 上报云端
    char recv_buf[128];
    while (1) {
        unsigned int read_size = sizeof(recv_buf);
        // 阻塞等待传感器数据
        int ret = osal_msg_queue_read_copy(g_mqtt_msg_queue, recv_buf, &read_size, OSAL_WAIT_FOREVER);
        
        if (ret == 0) { // 读取成功
            MQTTClient_message pubmsg = MQTTClient_message_initializer;
            pubmsg.payload = recv_buf;
            pubmsg.payloadlen = strlen(recv_buf);
            pubmsg.qos = 1;
            pubmsg.retained = 0;
            
            MQTTClient_deliveryToken token;
            MQTTClient_publishMessage(client, MQTT_TOPIC_PUB, &pubmsg, &token);
            osal_printk("[MQTT] Published: %s\r\n", recv_buf);
        }
    }
    return NULL;
}

void mqtt_task_start(void)
{
    osal_task *task_handle = NULL;
    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)mqtt_main_task, 0, "MQTT_Task", 0x2000);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, 25); // 优先级略高于传感器任务
    }
    osal_kthread_unlock();
}