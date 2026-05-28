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

#include "los_memory.h"
#include "los_task.h"
#include "watchdog.h"
#include "../inc/pwm_demo.h"

#include "pinctrl.h"
#include "gpio.h"
#include "hal_gpio.h"

extern int heart;
extern int32_t spo2;

#define BSP_LED 7

int MAX30102_task(void)
{
    (void)osal_msleep(5000);

    int x2;
    int y2;

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

        osal_msleep(500);
    }
    return 0;
}

#define SLE_LED_SER_TASK_PRIO 24
#define SLE_LED_SER_STACK_SIZE 0x2000

static void example_sle_led_server_entry(void)
{
    uapi_watchdog_disable();

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