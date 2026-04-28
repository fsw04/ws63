#include "sensor_task.h"
#include "mqtt_task.h"
#include "soc_osal.h"
#include "stdio.h"
#include "string.h"
#include "gpio.h"
#include "pinctrl.h"
#include "osal_debug.h"

#include "sle_device_discovery.h" // 包含获取本地 MAC 地址的接口

// 引入刚刚在 sle_speed_client.c 中新增的发送接口
extern int sle_client_send_data(const char *payload, uint16_t len);

// 辅助函数：获取本机 SLE MAC 地址并转为字符串
static void get_local_mac_str(char *mac_str, uint32_t len) {
    sle_addr_t local_addr = {0};
    // 获取本地 SLE 地址
    errcode_t ret = sle_get_local_addr(&local_addr);
    if (ret == ERRCODE_SUCC) {
        // 格式化为 12 位十六进制字符串，例如 5200075C6713
        snprintf(mac_str, len, "%02X%02X%02X%02X%02X%02X",
                 local_addr.addr[0], local_addr.addr[1], local_addr.addr[2],
                 local_addr.addr[3], local_addr.addr[4], local_addr.addr[5]);
    } else {
        // 获取失败时的 fallback
        snprintf(mac_str, len, "UNKNOWN_MAC");
    }
}

static void *sensor_main_task(const char *arg)
{
    unused(arg);

    osal_msleep(10000); // 刚开机等 10 秒，让底层星闪雷达扫频连接网关

    char device_mac[20] = {0};
    get_local_mac_str(device_mac, sizeof(device_mac));

    char json_buffer[1024] = {0};
    
    // 将 MAC 地址拼接到 deviceId 中，例如：watch_A1B2C3D4E5F6
    snprintf(json_buffer, sizeof(json_buffer), 
        "{"
        "\"deviceId\":\"watch_%s\","
        "\"type\":\"vitals\","
        "\"data\":{"
            "\"name\":\"李桂芳\","
            "\"phone\":\"13900000002\","
            "\"idCard\":\"110101196104082422\","
            "\"height\":\"160 cm\","
            "\"weight\":\"55 kg\","
            "\"bmi\":\"21.5\","
            "\"bloodPressure\":\"120/80 mmHg\","
            "\"fastingBloodGlucose\":\"5.2 mmol/L\""
        "}"
        "}", device_mac);

    // 调用你之前写好的星闪发送函数
    sle_client_send_data((const char *)json_buffer, strlen(json_buffer));
    osal_printk("[Sensor Task] 发送数据到网关, DeviceID: watch_%s, 长度: %d\n", device_mac, strlen(json_buffer));

    // osal_msleep(10000); // 刚开机等 10 秒，让底层星闪雷达扫频连接网关

    // //while (1) {
    //     char payload[1024];

    //     snprintf(payload, sizeof(payload), 
    //         "{"
    //         "\"deviceId\": \"watch_01\","
    //         "\"type\": \"vitals\","
    //         "\"sessionId\": \"sess_1001\","
    //         "\"ts\": 1713500000,"
    //         "\"sentAt\": 1713500000,"
    //         "\"ver\": \"1.0\","
    //         "\"data\": {"
    //             "\"name\": \"李桂芳\","
    //             "\"phone\": \"13900000002\","
    //             "\"idCard\": \"110101196104082422\","
    //             "\"height\": \"153.5 cm\","
    //             "\"weight\": \"43.6 kg\","
    //             "\"bmi\": \"18.5 kg/m2\","
    //             "\"bloodPressure\": \"115/71 mmHg\","
    //             "\"fastingBloodGlucose\": \"5.23 mmol/L\""
    //         "}"
    //         "}"
    //     );

    //     // 【核心】不再发往 MQTT，而是发往星闪网关！
    //     int ret = sle_client_send_data(payload, strlen(payload));
    //     if (ret == 0) {
    //         osal_printk("[Sensor] 成功通过星闪发送数据给网关！\r\n");
    //     } else {
    //         osal_printk("[Sensor] 发送失败，星闪可能未连接。\r\n");
    //     }
        
    //     osal_msleep(5000);
    // //}
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