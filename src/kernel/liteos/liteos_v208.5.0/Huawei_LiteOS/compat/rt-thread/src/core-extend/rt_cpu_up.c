/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2025. All rights reserved.
 * Description : LiteOS adapt rtthread cpu_up.
 * Author : Huawei LiteOS Team
 * Create : 2025-11-21
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

#include <rtthread.h>
#include "rtdef.h"
#include "rttypes.h"
#include "los_spinlock.h"

static struct rt_cpu _cpu;

void rt_spin_lock_init(struct rt_spinlock *lock)
{
    RT_UNUSED(lock);
}

void rt_spin_lock(struct rt_spinlock *lock)
{
    RT_UNUSED(lock);
    LOS_SpinLock(NULL);
}

void rt_spin_unlock(struct rt_spinlock *lock)
{
    RT_UNUSED(lock);
    LOS_SpinUnlock(NULL);
}

rt_base_t rt_spin_lock_irqsave(struct rt_spinlock *lock)
{
    RT_UNUSED(lock);
    UINT32 intSave;
    LOS_SpinLockSave(NULL, &intSave);
    return intSave;
}

void rt_spin_unlock_irqrestore(struct rt_spinlock *lock, rt_base_t level)
{
    RT_UNUSED(lock);
    LOS_SpinUnlockRestore(NULL, level);
}

rt_base_t rt_cpus_lock(void)
{
    return -RT_ERROR;
}

void rt_cpus_unlock(rt_base_t level)
{
    RT_UNUSED(level);
}

void rt_cpus_lock_status_restore(struct rt_thread *thread)
{
    RT_UNUSED(thread);
}

struct rt_cpu *rt_cpu_self(void)
{
    return &_cpu;
}

struct rt_cpu *rt_cpu_index(int index)
{
    return index == 0 ? &_cpu : RT_NULL;
}
