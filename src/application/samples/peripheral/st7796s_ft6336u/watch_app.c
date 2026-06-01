#include "watch_app.h"

#include "sle_watch_server.h"
#include "soc_osal.h"
#include "watch_model.h"
#include "wifi_task.h"

#define WATCH_APP_TASK_STACK_SIZE 0x2000
#define WATCH_APP_TASK_PRIORITY 24
#define WATCH_APP_WIFI_SSID "FSW"
#define WATCH_APP_WIFI_PWD "a2821840334"

static void *watch_app_task(const char *arg)
{
    (void)arg;

    watch_model_add_log(WATCH_LOG_SLE, "SLE", "starting server");
    sle_watch_server_start();

    watch_model_add_log(WATCH_LOG_WIFI, "WiFi", "start controller");
    wifi_task_start(WATCH_APP_WIFI_SSID, WATCH_APP_WIFI_PWD);

    while (1) {
        osal_msleep(10000);
    }
    return NULL;
}

void watch_app_start(void)
{
    osal_task *task_handle = NULL;

    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)watch_app_task, 0, "WatchAppTask",
                                      WATCH_APP_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, WATCH_APP_TASK_PRIORITY);
        osal_kfree(task_handle);
    }
    osal_kthread_unlock();
}
