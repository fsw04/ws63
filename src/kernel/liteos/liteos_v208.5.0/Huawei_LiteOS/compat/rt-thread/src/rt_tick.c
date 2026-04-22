/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2025. All rights reserved.
 * Description : LiteOS adapt rtthread tick.
 * Author : Huawei LiteOS Team
 * Create : 2025-7-08
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

#include "los_tick.h"
#include "los_tick_pri.h"
#include "rtdef.h"
#include "los_task_pri.h"

static void (*rt_tick_hook)(void);

/**
 * This function will set a hook function, which will be invoked when tick increase
 *
 *
 * @param hook the hook function
 */
void rt_tick_sethook(void (*hook)(void))
{
    rt_tick_hook = hook;
}

/**
 * @brief    This function will set current tick.
 *
 * @param    tick is the value that you will set.
 */
void rt_tick_set(rt_tick_t tick)
{
    (void)tick;
}

/**
 * @brief    This function will return current tick from operating system startup.
 *
 * @return   Return current tick.
 */
rt_tick_t rt_tick_get(void)
{
    return LOS_TickCountGet();
}

/**
 * @brief    This function will notify kernel there is one tick passed.
 *           Normally, this function is invoked by clock ISR.
 */
void rt_tick_increase(void)
{
    RT_OBJECT_HOOK_CALL(rt_tick_hook, ());
    OsTickHandler();
}

/**
 * @brief    This function will calculate the tick from millisecond.
 *
 * @param    ms is the specified millisecond.
 *              - Negative Number wait forever
 *              - Zero not wait
 *              - Max 0x7fffffff
 *
 * @return   Return the calculated tick.
 */
rt_tick_t rt_tick_from_millisecond(rt_int32_t ms)
{
    rt_tick_t tick;
    if (ms < 0) {
        tick = (rt_tick_t)RT_WAITING_FOREVER;
    } else {
        tick = LOS_MS2Tick(ms);
    }

    return tick;
}
