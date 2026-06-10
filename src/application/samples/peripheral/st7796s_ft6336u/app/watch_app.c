#include "watch_app.h"

#include "../services/sle_watch_server.h"
#include "soc_osal.h"
#include "../services/vitals_report.h"
#include "../model/watch_model.h"
#include "../tasks/watch_nfc_task.h"
#include "../services/wifi_task.h"

#define WATCH_APP_TASK_STACK_SIZE 0x2000
#define WATCH_APP_TASK_PRIORITY 24

static void *watch_app_task(const char *arg)
{
    (void)arg;

    watch_model_add_log(WATCH_LOG_SLE, "SLE", "starting server");
    sle_watch_server_start();

    watch_model_add_log(WATCH_LOG_WIFI, "WiFi", "start controller");
    wifi_task_start(NULL, NULL);

    watch_model_add_log(WATCH_LOG_DEVICE, "NFC", "start reader");
    watch_nfc_task_start();

    vitals_report_start();

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
