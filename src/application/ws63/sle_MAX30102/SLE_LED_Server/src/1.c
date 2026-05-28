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
#include "sle_common.h"
#include "sle_errcode.h"
#include "sle_ssap_server.h"
#include "sle_connection_manager.h"
#include "sle_device_discovery.h"
#include "../inc/SLE_LED_Server_adv.h"
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
// #include "MQTTClient.h"
#include "los_memory.h"
#include "los_task.h"
#include "soc_osal.h"
#include "app_init.h"
#include "common_def.h"
#include "../inc/wifi_connect.h"
#include "watchdog.h"
#include "../inc/cjson_demo.h"
#include "../inc/pwm_demo.h"
// #include "mqtt_demo.h"


#include "pinctrl.h"
#include "gpio.h"
#include "hal_gpio.h"




extern int heart;		//定义心率
// extern float SpO2;		//定义血氧饱和度
extern int32_t spo2;  //定义血氧饱和度 
#define OCTET_BIT_LEN 8
#define UUID_LEN_2 2







#define encode2byte_little(_ptr, data)                     \
    do {                                                   \
        *(uint8_t *)((_ptr) + 1) = (uint8_t)((data) >> 8); \
        *(uint8_t *)(_ptr) = (uint8_t)(data);              \
    } while (0)

/* sle server app uuid for sample */
static char g_sle_uuid_app_uuid[UUID_LEN_2] = {0x0, 0x0};
/* server property value for sample */
static char g_sle_property_value[OCTET_BIT_LEN] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
/* sle connect id */
static uint16_t g_conn_id = 0;
/* sle server id */
static uint8_t g_server_id = 0;
/* sle service handle */
static uint16_t g_service_handle = 0;
/* sle ntf property handle */
static uint16_t g_property_handle = 0;

static errcode_t example_sle_server_send_notify_by_handle(const uint8_t *data, uint8_t len);

/**
 * @if Eng
 * @brief  LED Control type.
 * @else
 * @brief  LED控制类型。
 * @endif
 */
typedef enum {
    EXAMPLE_CONTORL_LED_EXIT = 0x00,              /*!< @if Eng Exit LED Control Demo
                                                    @else   退出LED控制演示  */
    EXAMPLE_CONTORL_LED_MAINBOARD_LED_ON = 0x01,  /*!< @if Eng Turn on the LED on MainBoard
                                                    @else   打开主板上的LED @endif */
    EXAMPLE_CONTORL_LED_MAINBOARD_LED_OFF = 0x02, /*!< @if Eng Turn off the LED on MainBoard
                                                    @else   关闭主板上的LED @endif */
    EXAMPLE_CONTORL_LED_LEDBOARD_RLED_ON = 0x03,  /*!< @if Eng Turn on the RED LED on LED Board
                                                    @else   打开灯板上的红色LED @endif */
    EXAMPLE_CONTORL_LED_LEDBOARD_RLED_OFF = 0x04, /*!< @if Eng Turn off the RED LED on LED Board
                                                    @else   关闭灯板上的红色LED @endif */
    EXAMPLE_CONTORL_LED_LEDBOARD_YLED_ON = 0x05,  /*!< @if Eng Turn on the YELLOW LED on LED Board
                                                    @else   打开灯板上的黄色LED @endif */
    EXAMPLE_CONTORL_LED_LEDBOARD_YLED_OFF = 0x06, /*!< @if Eng Turn off the YELLOW LED on LED Board
                                                    @else   关闭灯板上的黄色LED @endif */
    EXAMPLE_CONTORL_LED_LEDBOARD_GLED_ON = 0x07,  /*!< @if Eng Turn on the GREEN LED on LED Board
                                                    @else   打开灯板上的绿色LED @endif */
    EXAMPLE_CONTORL_LED_LEDBOARD_GLED_OFF = 0x08, /*!< @if Eng Turn off the GREEN LED on LED Board
                                                    @else   关闭灯板上的绿色LED @endif */
} example_control_led_type_t;

#define LED_CONTROL_TASK_STACK_SIZE 0x1000
#define LED_CONTROL_TASK_PRIO (osPriority_t)(17)



#define ADDRESS "tcp://2084118ebf.st1.iotda-device.cn-north-4.myhuaweicloud.com"
#define CLIENTID "67f67d422902516e866fd005_147852369_0_0_2025050812"
#define TOPIC "MQTT Examples"
#define PAYLOAD "Hello World!"
#define QOS 1

#define CONFIG_WIFI_SSID "P40 Pro+"       // 要连接的WiFi 热点账号
#define CONFIG_WIFI_PWD "Zyh114785" // 要连接的WiFi 热点密码
#define MQTT_STA_TASK_PRIO 24
#define MQTT_STA_TASK_STACK_SIZE 0x1000
#define TIMEOUT 10000L
#define MSG_MAX_LEN 28
#define MSG_QUEUE_SIZE 32

static unsigned long g_msg_queue;
volatile MQTTClient_deliveryToken deliveredtoken;
char *g_username = "67f67d422902516e866fd005_147852369"; // deviceId or nodeId
char *g_password = "32df7b42b6a8a8e3c7cd6240844c2acb725020d1d6175fb755783bf02b5949b5";
MQTTClient client;
extern int MQTTClient_init(void);

    MQTT_msg *mqtt_msg;
    bloodmsg aht_msg;


static int example_led_control_task(const char *arg)
{
    unused(arg);
    // errcode_t ret = 0;
    // example_control_led_type_t last_led_operation = EXAMPLE_CONTORL_LED_LEDBOARD_GLED_OFF;

    PRINT("[SLE Server] start led control task\r\n");


  while (1)
  {
    (void)osal_msleep(500);
    // blood_Loop();
    // aht_msg.temperature = heart;
    // aht_msg.humidity = spo2;
    // mqtt_msg->msg_type = EN_MSG_REPORT;

    //     if ((mqtt_msg != NULL) && (aht_msg.temperature != 0) && (aht_msg.humidity != 0)) {
    //         sprintf(mqtt_msg->xin, "%.2f", aht_msg.temperature);
    //         sprintf(mqtt_msg->xue, "%.2f", aht_msg.humidity);
    //         printf("xinlv = %s, xueyang= %s\r\n", mqtt_msg->xin, mqtt_msg->xue);
    //         uint32_t ret = osal_msg_queue_write_copy(g_msg_queue, mqtt_msg, sizeof(MQTT_msg), OSAL_WAIT_FOREVER);
    //         if (ret != 0) {
    //             printf("ret = %#x\r\n", ret);
    //             osal_kfree(mqtt_msg);
    //             break;
    //         }
    //     }


     int x;
     int y;
     int q;

     x = heart ;
     y = spo2 ;

     q = 1 ;
    //  q = 2 ;

        uint8_t write_req_data[] = {x,y,q};
        example_sle_server_send_notify_by_handle(write_req_data, sizeof(write_req_data));

        // sever_xianshi(x,y);

        osal_msleep(1000); // 1000ms读取数据

  }
  


    return 0;
}

static void example_led_control_entry(void)
{
    osal_task *task_handle = NULL;
    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)example_led_control_task, 0, "LedControlTask",
                                      LED_CONTROL_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, LED_CONTROL_TASK_PRIO);
        osal_kfree(task_handle);
    }
    osal_kthread_unlock();
}

static void example_print_led_state(ssaps_req_write_cb_t *write_cb_para)
{
    if (write_cb_para->length == strlen("LED_ON") && write_cb_para->value[0] == 'L' && write_cb_para->value[1] == 'E' &&
        write_cb_para->value[2] == 'D' && write_cb_para->value[3] == '_' && write_cb_para->value[4] == 'O' &&
        write_cb_para->value[5] == 'N') {
        PRINT("[SLE Server] client main board led is on.\r\n");
    }

    if (write_cb_para->length == strlen("LED_OFF") && write_cb_para->value[0] == 'L' &&
        write_cb_para->value[1] == 'E' && write_cb_para->value[2] == 'D' && write_cb_para->value[3] == '_' &&
        write_cb_para->value[4] == 'O' && write_cb_para->value[5] == 'F' && write_cb_para->value[6] == 'F') {
        PRINT("[SLE Server] client main board led is off.\r\n");
    }
}

static uint8_t sle_uuid_base[] = {0x37, 0xBE, 0xA8, 0x80, 0xFC, 0x70, 0x11, 0xEA,
                                  0xB7, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static void example_sle_uuid_set_base(sle_uuid_t *out)
{
    (void)memcpy_s(out->uuid, SLE_UUID_LEN, sle_uuid_base, SLE_UUID_LEN);
    out->len = UUID_LEN_2;
}

static void example_sle_uuid_setu2(uint16_t u2, sle_uuid_t *out)
{
    example_sle_uuid_set_base(out);
    out->len = UUID_LEN_2;
    encode2byte_little(&out->uuid[14], u2);
}

static void example_ssaps_read_request_cbk(uint8_t server_id,
                                           uint16_t conn_id,
                                           ssaps_req_read_cb_t *read_cb_para,
                                           errcode_t status)
{
    PRINT("[SLE Server] ssaps read request cbk server_id:0x%x, conn_id:0x%x, handle:0x%x, type:0x%x, status:0x%x\r\n",
          server_id, conn_id, read_cb_para->handle, read_cb_para->type, status);
}

static void example_ssaps_write_request_cbk(uint8_t server_id,
                                            uint16_t conn_id,
                                            ssaps_req_write_cb_t *write_cb_para,
                                            errcode_t status)
{
    PRINT("[SLE Server] ssaps write request cbk server_id:0x%x, conn_id:0x%x, handle:0x%x, status:0x%x\r\n", server_id,
          conn_id, write_cb_para->handle, status);

    for (uint16_t idx = 0; idx < write_cb_para->length; idx++) {
        PRINT("[SLE Server] write request cbk[0x%x] 0x%02x\r\n", idx, write_cb_para->value[idx]);
    }

    if (status == ERRCODE_SUCC) {
        example_print_led_state(write_cb_para);
    }
}

static void example_ssaps_mtu_changed_cbk(uint8_t server_id,
                                          uint16_t conn_id,
                                          ssap_exchange_info_t *mtu_size,
                                          errcode_t status)
{
    PRINT("[SLE Server] ssaps mtu changed cbk server_id:0x%x, conn_id:0x%x, mtu_size:0x%x, status:0x%x\r\n", server_id,
          conn_id, mtu_size->mtu_size, status);
}

static void example_ssaps_start_service_cbk(uint8_t server_id, uint16_t handle, errcode_t status)
{
    PRINT("[SLE Server] start service cbk server_id:0x%x, handle:0x%x, status:0x%x\r\n", server_id, handle, status);
}

static errcode_t example_sle_ssaps_register_cbks(void)
{
    ssaps_callbacks_t ssaps_cbk = {0};
    ssaps_cbk.start_service_cb = example_ssaps_start_service_cbk;
    ssaps_cbk.mtu_changed_cb = example_ssaps_mtu_changed_cbk;
    ssaps_cbk.read_request_cb = example_ssaps_read_request_cbk;
    ssaps_cbk.write_request_cb = example_ssaps_write_request_cbk;
    return ssaps_register_callbacks(&ssaps_cbk);
}

static errcode_t example_sle_server_service_add(void)
{
    errcode_t ret = ERRCODE_FAIL;
    sle_uuid_t service_uuid = {0};
    example_sle_uuid_setu2(SLE_UUID_SERVER_SERVICE, &service_uuid);
    ret = ssaps_add_service_sync(g_server_id, &service_uuid, true, &g_service_handle);
    if (ret != ERRCODE_SUCC) {
        PRINT("[SLE Server] sle uuid add service fail, ret:0x%x\r\n", ret);
        return ERRCODE_FAIL;
    }

    PRINT("[SLE Server] sle uuid add service service_handle: %u\r\n", g_service_handle);

    return ERRCODE_SUCC;
}

static errcode_t example_sle_server_property_add(void)
{
    errcode_t ret = ERRCODE_FAIL;
    ssaps_property_info_t property = {0};
    ssaps_desc_info_t descriptor = {0};
    uint8_t ntf_value[] = {0x01, 0x0};

    property.permissions = SSAP_PERMISSION_READ | SSAP_PERMISSION_WRITE;
    example_sle_uuid_setu2(SLE_UUID_SERVER_PROPERTY, &property.uuid);
    property.value = osal_vmalloc(sizeof(g_sle_property_value));
    if (property.value == NULL) {
        PRINT("[SLE Server] sle property mem fail\r\n");
        return ERRCODE_MALLOC;
    }

    if (memcpy_s(property.value, sizeof(g_sle_property_value), g_sle_property_value, sizeof(g_sle_property_value)) !=
        EOK) {
        osal_vfree(property.value);
        PRINT("[SLE Server] sle property mem cpy fail\r\n");
        return ERRCODE_MEMCPY;
    }
    ret = ssaps_add_property_sync(g_server_id, g_service_handle, &property, &g_property_handle);
    if (ret != ERRCODE_SUCC) {
        PRINT("[SLE Server] sle uuid add property fail, ret:0x%x\r\n", ret);
        osal_vfree(property.value);
        return ERRCODE_FAIL;
    }

    PRINT("[SLE Server] sle uuid add property property_handle: %u\r\n", g_property_handle);

    // descriptor.permissions = SSAP_PERMISSION_READ | SSAP_PERMISSION_WRITE;
    // descriptor.value = osal_vmalloc(sizeof(ntf_value));

    descriptor.permissions = SSAP_PERMISSION_READ | SSAP_PERMISSION_WRITE;
    descriptor.operate_indication = SSAP_OPERATE_INDICATION_BIT_READ | SSAP_OPERATE_INDICATION_BIT_WRITE;
    descriptor.type = SSAP_DESCRIPTOR_USER_DESCRIPTION;
    descriptor.value = ntf_value;
    descriptor.value_len = sizeof(ntf_value); 

    if (descriptor.value == NULL) {
        PRINT("[SLE Server] sle descriptor mem fail\r\n");
        osal_vfree(property.value);
        return ERRCODE_MALLOC;
    }
    if (memcpy_s(descriptor.value, sizeof(ntf_value), ntf_value, sizeof(ntf_value)) != EOK) {
        PRINT("[SLE Server] sle descriptor mem cpy fail\r\n");
        osal_vfree(property.value);
        osal_vfree(descriptor.value);
        return ERRCODE_MEMCPY;
    }
    ret = ssaps_add_descriptor_sync(g_server_id, g_service_handle, g_property_handle, &descriptor);
    if (ret != ERRCODE_SUCC) {
        PRINT("[SLE Server] sle uuid add descriptor fail, ret:0x%x\r\n", ret);
        osal_vfree(property.value);
        osal_vfree(descriptor.value);
        return ERRCODE_FAIL;
    }
    osal_vfree(property.value);
    osal_vfree(descriptor.value);
    return ERRCODE_SUCC;
}

static errcode_t example_sle_server_add(void)
{
    errcode_t ret = ERRCODE_FAIL;
    sle_uuid_t app_uuid = {0};

    PRINT("[SLE Server] sle uuid add service in\r\n");
    app_uuid.len = sizeof(g_sle_uuid_app_uuid);
    if (memcpy_s(app_uuid.uuid, app_uuid.len, g_sle_uuid_app_uuid, sizeof(g_sle_uuid_app_uuid)) != EOK) {
        return ERRCODE_MEMCPY;
    }
    ssaps_register_server(&app_uuid, &g_server_id);

    if (example_sle_server_service_add() != ERRCODE_SUCC) {
        ssaps_unregister_server(g_server_id);
        return ERRCODE_FAIL;
    }

    if (example_sle_server_property_add() != ERRCODE_SUCC) {
        ssaps_unregister_server(g_server_id);
        return ERRCODE_FAIL;
    }
    PRINT("[SLE Server] sle uuid add service, server_id:0x%x, service_handle:0x%x, property_handle:0x%x\r\n",
          g_server_id, g_service_handle, g_property_handle);
    ret = ssaps_start_service(g_server_id, g_service_handle);
    if (ret != ERRCODE_SUCC) {
        PRINT("[SLE Server] sle uuid add service fail, ret:0x%x\r\n", ret);
        return ERRCODE_FAIL;
    }
    PRINT("[SLE Server] sle uuid add service out\r\n");
    return ERRCODE_SUCC;
}

/* server通过handle向client发送数据：notify */
static errcode_t example_sle_server_send_notify_by_handle(const uint8_t *data, uint8_t len)
{
    ssaps_ntf_ind_t param = {0};

    param.handle = g_property_handle;
    param.type = 0;

    param.value = osal_vmalloc(len);
    param.value_len = len;
    if (param.value == NULL) {
        PRINT("[SLE Server] send notify mem fail\r\n");
        return ERRCODE_MALLOC;
    }

    if (memcpy_s(param.value, param.value_len, data, len) != EOK) {
        PRINT("[SLE Server] send notify memcpy fail\r\n");
        osal_vfree(param.value);
        return ERRCODE_MEMCPY;
    }

    if (ssaps_notify_indicate(g_server_id, g_conn_id, &param) != ERRCODE_SUCC) {
        PRINT("[SLE Server] ssaps notify indicate fail\r\n");
        osal_vfree(param.value);
        return ERRCODE_FAIL;
    }
    osal_vfree(param.value);
    return ERRCODE_SUCC;
}

static void example_sle_connect_state_changed_cbk(uint16_t conn_id,
                                                  const sle_addr_t *addr,
                                                  sle_acb_state_t conn_state,
                                                  sle_pair_state_t pair_state,
                                                  sle_disc_reason_t disc_reason)
{
    PRINT(
        "[SLE Server] connect state changed conn_id:0x%02x, conn_state:0x%x, pair_state:0x%x, \
        disc_reason:0x%x\r\n",
        conn_id, conn_state, pair_state, disc_reason);
    PRINT("[SLE Server] connect state changed addr:%02x:**:**:**:%02x:%02x\r\n", addr->addr[0], addr->addr[4],
          addr->addr[5]);
    g_conn_id = conn_id;
}

static void example_sle_pair_complete_cbk(uint16_t conn_id, const sle_addr_t *addr, errcode_t status)
{
    PRINT("[SLE Server] pair complete conn_id:0x%02x, status:0x%x\r\n", conn_id, status);
    PRINT("[SLE Server] pair complete addr:%02x:**:**:**:%02x:%02x\r\n", addr->addr[0], addr->addr[4], addr->addr[5]);

    if (status == ERRCODE_SUCC) {
        example_led_control_entry();
    }
}

static errcode_t example_sle_conn_register_cbks(void)
{
    sle_connection_callbacks_t conn_cbks = {0};
    conn_cbks.connect_state_changed_cb = example_sle_connect_state_changed_cbk;
    conn_cbks.pair_complete_cb = example_sle_pair_complete_cbk;
    return sle_connection_register_callbacks(&conn_cbks);
}

static int example_sle_led_server_task(const char *arg)
{
    unused(arg);



    (void)osal_msleep(5000); /* 延时5s，等待SLE初始化完毕 */

    PRINT("[SLE Server] try enable.\r\n");
    /* 使能SLE */
    if (enable_sle() != ERRCODE_SUCC) {
        PRINT("[SLE Server] sle enbale fail !\r\n");
        return -1;
    }

    /* 注册连接管理回调函数 */
    if (example_sle_conn_register_cbks() != ERRCODE_SUCC) {
        PRINT("[SLE Server] sle conn register cbks fail !\r\n");
        return -1;
    }

    /* 注册 SSAP server 回调函数 */
    if (example_sle_ssaps_register_cbks() != ERRCODE_SUCC) {
        PRINT("[SLE Server] sle ssaps register cbks fail !\r\n");
        return -1;
    }

    /* 注册Server, 添加Service和property, 启动Service */
    if (example_sle_server_add() != ERRCODE_SUCC) {
        PRINT("[SLE Server] sle server add fail !\r\n");
        return -1;
    }

    /* 设置设备公开，并公开设备 */
    if (example_sle_server_adv_init() != ERRCODE_SUCC) {
        PRINT("[SLE Server] sle server adv fail !\r\n");
        return -1;
    }

    PRINT("[SLE Server] init ok\r\n");

    return 0;
}



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
    conn_opts.keepAliveInterval = 120; // 保持间隔为120秒，每120秒发送一次消息
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
    int biao = 0 ;
    uint32_t resize = 32;
    char *beep_status = NULL;


    wifi_connect(CONFIG_WIFI_SSID, CONFIG_WIFI_PWD);
    ret = mqtt_connect();
    if (ret != 0) {
        printf("connect failed, result %d\n", ret);
    }
    osal_msleep(1000); // 1000等待连接成功
    char *cmd_topic = combine_strings(3, "$oc/devices/", g_username, "/sys/commands/#");
    ret = mqtt_subscribe(cmd_topic);
    if (ret < 0) {
        printf("subscribe topic error, result %d\n", ret);
    }
    char *report_topic = combine_strings(3, "$oc/devices/", g_username, "/sys/properties/report");
    while (1) {



        biao = 1 ;
        ret = osal_msg_queue_read_copy(g_msg_queue, report_msg, &resize, OSAL_WAIT_FOREVER);
        if (ret != 0) {
            printf("queue_read ret = %#x\r\n", ret);
            osal_kfree(report_msg);
            biao = 0 ;
            // break;
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
        osal_msleep(1000); // 1000等待连接成功
    }



    return ret;
}

#define BSP_LED 7  
int MAX30102_task(void)
{
    int x2;
    int y2;
    mqtt_msg = osal_kmalloc(sizeof(MQTT_msg), 0);
    // 检查内存分配
    if (mqtt_msg == NULL) {
        printf("Memory allocation failed\r\n");
    }
    Aht20TestTask();
    uapi_pin_set_mode(BSP_LED, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(BSP_LED, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(BSP_LED, GPIO_LEVEL_LOW);
    while(1)
    {
      blood_Loop();
      x2 = heart ;
      y2 = spo2 ;
      sever_xianshi(x2,y2);

      if(x2>0)
      {
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
    task_handle = osal_kthread_create((osal_kthread_handler)example_sle_led_server_task, 0, "SLELedServerTask",
                                      SLE_LED_SER_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, SLE_LED_SER_TASK_PRIO);
        osal_kfree(task_handle);
    }


    task_handle = osal_kthread_create((osal_kthread_handler)MAX30102_task, 0, "MAX30102_task", SLE_LED_SER_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, SLE_LED_SER_TASK_PRIO);
        osal_kfree(task_handle);
    }

    osal_kthread_unlock();
}

/* Run the example_sle_led_server_entry. */
 app_run(example_sle_led_server_entry);