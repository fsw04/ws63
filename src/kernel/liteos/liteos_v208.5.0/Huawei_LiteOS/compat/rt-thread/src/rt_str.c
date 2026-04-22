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

#include "rtthread.h"
#include "rttypes.h"
#include "securec.h"
#include "string.h"

void *rt_memset(void *s, int c, rt_ubase_t count)
{
    if (s == NULL) {
        return RT_NULL;
    }

    errno_t ret = memset_s(s, count, c, count);
    if (ret != EOK) {
        return RT_NULL;
    } else {
        return s;
    }
}

void *rt_memcpy(void *dst, const void *src, rt_ubase_t count)
{
    if (dst == NULL || src == NULL) {
        return RT_NULL;
    }

    errno_t ret = memcpy_s(dst, count, src, count);
    if (ret != EOK) {
        return RT_NULL;
    } else {
        return dst;
    }
}

void *rt_memmove(void *dest, const void *src, rt_size_t n)
{
    if (dest == NULL || src == NULL) {
        return RT_NULL;
    }

    errno_t ret = memmove_s(dest, n, src, n);
    if (ret != EOK) {
        return RT_NULL;
    } else {
        return dest;
    }
}

rt_int32_t rt_memcmp(const void *cs, const void *ct, rt_size_t count)
{
    if (cs == NULL || ct == NULL) {
        return -RT_ERROR;
    }

    return memcmp(cs, ct, count);
}

char *rt_strstr(const char *s1, const char *s2)
{
    if (s1 == NULL || s2 == NULL) {
        return RT_NULL;
    }

    char *ret = strstr(s1, s2);
    if (ret == NULL) {
        return RT_NULL;
    }
    return ret;
}

rt_int32_t rt_strcasecmp(const char *a, const char *b)
{
    if (a == NULL || b == NULL) {
        return -RT_ERROR;
    }

    return strcasecmp(a, b);
}

char *rt_strncpy(char *dst, const char *src, rt_size_t n)
{
    if (dst == NULL || src == NULL) {
        return RT_NULL;
    }

    errno_t ret = strncpy_s(dst, n, src, n);
    if (ret != EOK) {
        return RT_NULL;
    } else {
        return dst;
    }
}

rt_int32_t rt_strncmp(const char *cs, const char *ct, rt_size_t count)
{
    if (cs == NULL || ct == NULL) {
        return -RT_ERROR;
    }

    return strncmp(cs, ct, count);
}

rt_int32_t rt_strcmp(const char *cs, const char *ct)
{
    if (cs == NULL || ct == NULL) {
        return -RT_ERROR;
    }

    return strcmp(cs, ct);
}

rt_size_t rt_strnlen(const char *s, rt_ubase_t maxlen)
{
    if (s == NULL) {
        return -RT_ERROR;
    }

    return strnlen(s, maxlen);
}

rt_size_t rt_strlen(const char *s)
{
    if (s == NULL) {
        return -RT_ERROR;
    }

    return strlen(s);
}

char *rt_strdup(const char *s)
{
    if (s == NULL) {
        return RT_NULL;
    }

    rt_size_t len = rt_strlen(s) + 1;
    char *tmp = (char *)rt_malloc(len);

    if (!tmp) {
        return RT_NULL;
    }

    rt_memcpy(tmp, s, len);

    return tmp;
}