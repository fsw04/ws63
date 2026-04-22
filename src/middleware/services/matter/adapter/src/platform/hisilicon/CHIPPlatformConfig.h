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
 *          Platform-specific configuration overrides for CHIP on
 *          Hisi platforms.
 */

#pragma once

// ==================== General Platform Adaptations ====================

#define ASN1_CONFIG_NO_ERROR 0
#define ASN1_CONFIG_ERROR_MIN 5000000
#define ASN1_CONFIG_ERROR_MAX 5000999

#define ChipDie() abort()


// ==================== General Configuration Overrides ====================
#ifndef CHIP_CONFIG_MAX_EXCHANGE_CONTEXTS
#define CHIP_CONFIG_MAX_EXCHANGE_CONTEXTS 10
#endif // CHIP_CONFIG_MAX_EXCHANGE_CONTEXTS

#ifndef CHIP_CONFIG_WRMP_TIMER_DEFAULT_PERIOD
#define CHIP_CONFIG_WRMP_TIMER_DEFAULT_PERIOD 50
#endif // CHIP_CONFIG_WRMP_TIMER_DEFAULT_PERIOD

#define CHIP_CONFIG_MAX_GROUP_DATA_PEERS 5
#define CHIP_IM_MAX_NUM_COMMAND_HANDLER 1
#define CHIP_IM_MAX_NUM_WRITE_HANDLER 1
#define CHIP_IM_MAX_NUM_WRITE_CLIENT 1
#define CHIP_IM_MAX_REPORTS_IN_FLIGHT 1
#define CHIP_CONFIG_LAMBDA_EVENT_SIZE (16)
#define CHIP_CONFIG_DEVICE_MAX_ACTIVE_DEVICES 2
#define CHIP_CONFIG_MAX_FABRICS 5
#define CHIP_LOG_FILTERING 0
#define CHIP_CONFIG_MAX_UNSOLICITED_MESSAGE_HANDLERS 8
