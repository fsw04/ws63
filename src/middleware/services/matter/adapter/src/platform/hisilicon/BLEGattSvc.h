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
 *          Provides an implementation of the BLEManager singleton object
 */

#ifndef __BLE_GATT_SVC_INCLUDE
#define __BLE_GATT_SVC_INCLUDE
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif
void MatterBleStackOpen(void);

void MatterInitCallback(void);

void MatterBleStartAdv(bool fast);

void MatterBleStopAdv(void);

void MatterBleAddService(void);

void MatterCloseConnection(uint8_t conId);

void MatterTXCharSendIndication(uint8_t conId, uint16_t size, uint8_t * data);
#ifdef __cplusplus
}
#endif
#endif
