/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2025. All rights reserved.
 * Description : LiteOS adapt rtthread atomic.
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

#include "rtatomic.h"

rt_atomic_t rt_hw_atomic_exchange(volatile rt_atomic_t *ptr, rt_atomic_t val)
{
    if (ptr == NULL) {
        return -1;
    }
    rt_base_t level;
    rt_atomic_t temp;
    level = rt_hw_interrupt_disable();
    temp = *ptr;
    *ptr = val;
    rt_hw_interrupt_enable(level);
    return temp;
}

rt_atomic_t rt_hw_atomic_add(volatile rt_atomic_t *ptr, rt_atomic_t val)
{
    if (ptr == NULL) {
        return -1;
    }
    rt_base_t level;
    rt_atomic_t temp;
    level = rt_hw_interrupt_disable();
    temp = *ptr;
    *ptr = temp + val;
    rt_hw_interrupt_enable(level);
    return temp;
}

rt_atomic_t rt_hw_atomic_sub(volatile rt_atomic_t *ptr, rt_atomic_t val)
{
    if (ptr == NULL) {
        return -1;
    }
    rt_base_t level;
    rt_atomic_t temp;
    level = rt_hw_interrupt_disable();
    temp = *ptr;
    *ptr = temp - val;
    rt_hw_interrupt_enable(level);
    return temp;
}

rt_atomic_t rt_hw_atomic_xor(volatile rt_atomic_t *ptr, rt_atomic_t val)
{
    if (ptr == NULL) {
        return -1;
    }
    rt_base_t level;
    rt_atomic_t temp;
    level = rt_hw_interrupt_disable();
    temp = *ptr;
    *ptr = temp ^ val;
    rt_hw_interrupt_enable(level);
    return temp;
}

rt_atomic_t rt_hw_atomic_and(volatile rt_atomic_t *ptr, rt_atomic_t val)
{
    if (ptr == NULL) {
        return -1;
    }
    rt_base_t level;
    rt_atomic_t temp;
    level = rt_hw_interrupt_disable();
    temp = *ptr;
    *ptr = temp & val;
    rt_hw_interrupt_enable(level);
    return temp;
}

rt_atomic_t rt_hw_atomic_or(volatile rt_atomic_t *ptr, rt_atomic_t val)
{
    if (ptr == NULL) {
        return -1;
    }
    rt_base_t level;
    rt_atomic_t temp;
    level = rt_hw_interrupt_disable();
    temp = *ptr;
    *ptr = temp | val;
    rt_hw_interrupt_enable(level);
    return temp;
}

rt_atomic_t rt_hw_atomic_load(volatile rt_atomic_t *ptr)
{
    if (ptr == NULL) {
        return -1;
    }
    return *(volatile rt_atomic_t *)ptr;
}

void rt_hw_atomic_store(volatile rt_atomic_t *ptr, rt_atomic_t val)
{
    if (ptr == NULL) {
        return;
    }
    *(volatile rt_atomic_t *)ptr = val;
}

rt_atomic_t rt_hw_atomic_flag_test_and_set(volatile rt_atomic_t *ptr)
{
    if (ptr == NULL) {
        return -1;
    }
    rt_base_t level;
    rt_atomic_t temp;
    level = rt_hw_interrupt_disable();
    if (*ptr == 0) {
        temp = 0;
        *ptr = 1;
    } else {
        temp = 1;
    }
    rt_hw_interrupt_enable(level);
    return temp;
}

rt_atomic_t rt_hw_atomic_compare_exchange_strong(volatile rt_atomic_t *ptr1, rt_atomic_t *ptr2, rt_atomic_t desired)
{
    if (ptr1 == NULL || ptr2 == NULL) {
        return -1;
    }
    rt_base_t level;
    rt_atomic_t temp;
    level = rt_hw_interrupt_disable();
    if ((*ptr1) == (*ptr2)) {
        *ptr1 = desired;
        temp = 1;
    } else {
        *ptr2 = *ptr1;
        temp = 0;
    }
    rt_hw_interrupt_enable(level);
    return temp;
}

void rt_hw_atomic_flag_clear(volatile rt_atomic_t *ptr)
{
    if (ptr == NULL) {
        return;
    }
    *(volatile rt_atomic_t *)ptr = 0;
}
