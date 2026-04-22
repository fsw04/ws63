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
 *          Provides implementations for the CHIP logging functions
 *          on the Hisi platform.
 */
/* this file behaves like a config.h, comes first */
#include <platform/logging/LogV.h>

#include <lib/core/CHIPConfig.h>
#include <lib/support/logging/Constants.h>

#include <cstdio>
#include "securec.h"
extern "C" void osal_printk(const char * fmt, ...);

#ifdef LOG_LOCAL_LEVEL
#undef LOG_LOCAL_LEVEL
#endif

namespace chip {
namespace Logging {
namespace Platform {
constexpr int CHIP_LOG_TAG_LEN = 11;
void LogV(const char * module, uint8_t category, const char * msg, va_list v)
{
    char tag[CHIP_LOG_TAG_LEN];
    int ret = 0;

    ret = snprintf_s(tag, sizeof(tag), sizeof(tag) - 1, "chip[%s]", module);
    if (ret < 0) {
        return;
    }
    tag[sizeof(tag) - 1] = 0;

    char formattedMsg[CHIP_CONFIG_LOG_MESSAGE_MAX_SIZE];
    ret = vsnprintf_s(formattedMsg, sizeof(formattedMsg), sizeof(formattedMsg) - 1, msg, v);
    if (ret < 0) {
        return;
    }

    switch (category) {
        case kLogCategory_Error:
            osal_printk("%s %s\r\n", tag, formattedMsg);
            break;
        case kLogCategory_Progress:
        default:
            osal_printk("%s %s\r\n", tag, formattedMsg);
            break;
        case kLogCategory_Detail:
        case kLogCategory_Automation:
            osal_printk("%s %s\r\n", tag, formattedMsg);
            break;
    }
}

} // namespace Platform
} // namespace Logging
} // namespace chip
