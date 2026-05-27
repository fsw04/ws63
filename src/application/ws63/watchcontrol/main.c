#include "common_def.h"
#include "soc_osal.h"
#include "app_init.h"
#include "wifi_task.h"
#include "mqtt_task.h"
#include "sensor_task.h"
#include "ui_task.h"
#include "sle_speed_server.h"

#define DEFAULT_TASK_STACK_SIZE         0x1000
#define DEFAULT_TASK_PRIORITY           24
#define DELAYS_MS                       1000

#define WIFI_SSID "FSW"
#define WIFI_PWD  "a2821840334"

static void *app_main_task(const char *arg)
{
    unused(arg);
    osal_printk("================================\r\n");
    osal_printk("  Watch Application Start!      \r\n");
    osal_printk("================================\r\n");

    if (wifi_connect_start(WIFI_SSID, WIFI_PWD) == ERRCODE_SUCC) {
        osal_printk("[APP] Network Ready!\r\n");
        mqtt_task_start();
    } else {
        osal_printk("[APP] Network Connect Failed.\r\n");
    }

    sle_server_task_start();
    ui_task_start();

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

app_run(watch_app_entry);
