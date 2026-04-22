#include "sensor_task.h"
#include "mqtt_task.h"
#include "soc_osal.h"
#include "stdio.h"
#include "string.h"
#include "gpio.h"
#include "pinctrl.h"

// 假设测速传感器的 DO 引脚接在 WS63 的 GPIO_1
#define SENSOR_DO_PIN 1

// 初始化 GPIO
static void speed_sensor_init(void)
{
    // 配置引脚复用为 GPIO 功能
    uapi_pin_set_mode(SENSOR_DO_PIN, HAL_PIO_FUNC_GPIO);
    // 配置引脚为输入模式
    uapi_gpio_set_dir(SENSOR_DO_PIN, GPIO_DIRECTION_INPUT);
}

static void *sensor_main_task(const char *arg)
{
    unused(arg);

    char payload_buf[128];
    gpio_level_t current_level = GPIO_LEVEL_LOW;
    gpio_level_t last_level = GPIO_LEVEL_LOW;
    uint32_t pulse_count = 0; // 累计脉冲数（用于计算转速）
    
    speed_sensor_init();
    osal_msleep(3000); // 等待 MQTT 连接成功

    // 每 10ms 轮询一次传感器状态，累加脉冲数
    // 实际应用中也可以使用 GPIO 外部中断来实现更精确的脉冲计数
    uint32_t loop_count = 0;
    while (1) {
        current_level = uapi_gpio_get_val(SENSOR_DO_PIN);
        
        // 检测上升沿 (从无遮挡 0 变为有遮挡 1)
        if (current_level == GPIO_LEVEL_HIGH && last_level == GPIO_LEVEL_LOW) {
            pulse_count++;

            osal_printk("[SENSOR] Triggered! pulse_count = %d\r\n", pulse_count);
        }
        last_level = current_level;

        // 每隔 5 秒（500 * 10ms）上报一次累加脉冲数或转速
        loop_count++;
        if (loop_count >= 500) {
            // 组装 JSON 数据 (上报累计遮挡次数/脉冲数)
            snprintf(payload_buf, sizeof(payload_buf), 
                "{\"device\": \"watch_01\", \"pulse_count\": %d }", 
                pulse_count);

            osal_printk("[SENSOR] Current pulse count: %d\r\n", pulse_count);

            if (g_mqtt_msg_queue != 0) {
                osal_msg_queue_write_copy(g_mqtt_msg_queue, payload_buf, strlen(payload_buf) + 1, 0);
            }
            
            // 可选：每次上报后清零脉冲数，则代表上报的是这 5 秒内的"转速"
            // pulse_count = 0; 
            
            loop_count = 0;
        }
        
        osal_msleep(10); // 10ms 轮询间隔
    }
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