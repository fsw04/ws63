/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2025. All rights reserved.
 * Description : LiteOS adapt rtthread ulog.
 * Author : Huawei LiteOS Team
 * Create : 2025-11-26
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 * conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 * of conditions and the following disclaimer in the documentation and/or other materials
 * provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific prior written
 * permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <ctype.h>
#include <rtthread.h>

#include "rtdef.h"
#include "ulog_def.h"
#include "los_printf.h"
#include "los_printf_pri.h"

#define ADD_SPACE 8

int ulog_init(void)
{
    return 0;
}

void ulog_deinit(void)
{
}

void ulog_flush(void)
{
}

void ulog_output(rt_uint32_t level, const char *tag, rt_bool_t newline, const char *format, ...)
{
    dprintf("[%d] ", rt_tick_get());
    switch (level) {
        case LOG_LVL_ASSERT:
            dprintf("[A/%s] ", tag);
            break;
        case LOG_LVL_ERROR:
            dprintf("[E/%s] ", tag);
            break;
        case LOG_LVL_WARNING:
            dprintf("[W/%s] ", tag);
            break;
        case LOG_LVL_INFO:
            dprintf("[I/%s] ", tag);
            break;
        case LOG_LVL_DBG:
            dprintf("[D/%s] ", tag);
            break;
        default:
            dprintf("[?/%s] ", tag);
            break;
    }

    va_list ap;
    va_start(ap, format);
    ConsoleVprintf(format, ap);
    va_end(ap);

    if (newline) {
        dprintf("\n");
    }
}

void ulog_hexdump(const char *tag, rt_size_t width, const rt_uint8_t *buf, rt_size_t size, ...)
{
    const unsigned char *data = (const unsigned char *)buf;
    rt_size_t i, j;
    if (width == 0) {
        dprintf("The width cannot be set to 0!\n");
        return;
    }

    for (i = 0; i < size; i += width) {
        dprintf("[D/HEX] %s: %04X-%04X: ", tag, i, i + width);
        for (j = 0; j < width; j++) {
            if (i + j < size) {
                dprintf("%02X ", data[i + j]);
            } else {
                dprintf("   ");
            }
            if ((j + 1) % ADD_SPACE == 0) {
                dprintf(" ");
            }
        }
        dprintf(" ");
        for (j = 0; j < width; j++) {
            if (i + j < size) {
                unsigned char c = data[i + j];
                dprintf("%c", isprint(c) ? c : '.');
            } else {
                dprintf(" ");
            }
        }
        dprintf("\n");
    }
}

void ulog_raw(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    ConsoleVprintf(format, ap);
    va_end(ap);
}
