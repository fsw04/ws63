/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2025-2025. All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */
#include <lib/support/logging/CHIPLogging.h>
#include "driver/pinctrl.h"
#include "driver/gpio.h"
#include "LightingManager.h"

LightingManager LightingManager::sLight;

#define MATTER_LIGHT_PIN GPIO_02

static void LightingGpioInit()
{
    uapi_pin_set_mode(MATTER_LIGHT_PIN, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(MATTER_LIGHT_PIN, GPIO_DIRECTION_OUTPUT);
}

void LightingManager::LightSet(LightingManager::Action_t aAction)
{
    int32_t ret = 0;
    LightingGpioInit();
    if(aAction == LightingManager::ON_ACTION) {
        ret = uapi_gpio_set_val(MATTER_LIGHT_PIN, GPIO_LEVEL_HIGH);
        ChipLogProgress(NotSpecified, "LightingManager Action ON, ret:0x%x", ret);
    }
    if(aAction == LightingManager::OFF_ACTION) {
        ret = uapi_gpio_set_val(MATTER_LIGHT_PIN, GPIO_LEVEL_LOW);
        ChipLogProgress(NotSpecified, "LightingManager Action OFF ret:0x%x", ret);
    }
}