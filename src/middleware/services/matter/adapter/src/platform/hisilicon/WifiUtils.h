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

/**
 *    @file
 *          Provides hisilicon wifi utils
 */

#ifndef __WIFI_UTILS_INCLUDE
#define __WIFI_UTILS_INCLUDE

#include <lib/support/CodeUtils.h>
#include <cstdio>
#include "wifi_device.h"

#ifdef __cplusplus
extern "C" {
#endif

CHIP_ERROR WifiUtilsInit();
CHIP_ERROR WifiUtilsConnectNetwork(wifi_sta_config_stru *expected_bss);
CHIP_ERROR WifiUtilsStaScan(wifi_scan_params_stru *scan_params);

#ifdef __cplusplus
}
#endif
#endif
