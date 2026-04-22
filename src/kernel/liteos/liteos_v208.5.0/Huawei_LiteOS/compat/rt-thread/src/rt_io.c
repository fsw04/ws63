/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2025. All rights reserved.
 * Description : LiteOS adapt rtthread mq.
 * Author : Huawei LiteOS Team
 * Create : 2025-08-07
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

#include <stdio.h>

#include <stdarg.h>
#include "rtthread.h"
#include "rttypes.h"
#include "securec.h"

int rt_snprintf(char *buf, rt_size_t size, const char *fmt, ...)
{
    va_list args;
    int ret;
    
    if (buf == NULL || fmt == NULL || size == 0) {
        return -RT_ERROR;
    }
    
    va_start(args, fmt);
    ret = vsnprintf_s(buf, (size_t)size, (size_t)size, fmt, args);
    va_end(args);

    return ret;
}

int rt_vsprintf(char *buf, const char *format, va_list args)
{
    if (buf == NULL || format == NULL) {
        return -RT_ERROR;
    }

    return vsnprintf_s(buf, RT_CONSOLEBUF_SIZE, RT_CONSOLEBUF_SIZE, format, args);
}

int rt_sprintf(char *str, const char *format, ...)
{
    va_list args;
    int ret;
    
    if (str == NULL || format == NULL) {
        return -RT_ERROR;
    }
    
    va_start(args, format);
    ret = vsnprintf_s(str, RT_CONSOLEBUF_SIZE, RT_CONSOLEBUF_SIZE, format, args);
    va_end(args);
    
    return ret;
}

#ifdef RT_KLIBC_USING_LIBC_VSNPRINTF
int rt_vsnprintf(char *buf, rt_size_t size, const char *fmt, va_list args)
{
    int ret = vsnprintf_s(buf, (size_t)size, (size_t)size, fmt, args);
    return ret;
}
#endif /* RT_KLIBC_USING_LIBC_VSNPRINTF */
RTM_EXPORT(rt_vsnprintf);