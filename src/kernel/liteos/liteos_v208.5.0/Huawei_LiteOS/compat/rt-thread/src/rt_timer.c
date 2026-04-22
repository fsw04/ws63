/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2025. All rights reserved.
 * Description : LiteOS adapt rtthread timer.
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

#include "rtdef.h"
#include "rtthread.h"
#include "los_swtmr.h"
#include "los_swtmr_pri.h"
#include "los_memory.h"
#include "los_task.h"

#define MAX_TIMERS LOSCFG_BASE_CORE_SWTMR_LIMIT

typedef struct {
    rt_timer_t timer;
    UINT16 swtmrId;
} rt_timer_to_swtmrId;
typedef rt_timer_to_swtmrId *rt_timer_to_swtmrId_t;

static rt_timer_to_swtmrId_t timer_map[MAX_TIMERS] = {0};
static void (*g_rt_timer_enter_hook)(struct rt_timer *timer);
static void (*g_rt_timer_exit_hook)(struct rt_timer *timer);

static UINT8 find_timer_adapter(rt_timer_t timer)
{
    for (UINT8 i = 0; i < MAX_TIMERS; i++) {
        if (timer_map[i]->timer != NULL && timer_map[i]->timer == (VOID *)timer)
            return i;
    }
    return MAX_TIMERS;
}

void rt_timer_enter_sethook(void (*hook)(struct rt_timer *timer))
{
    g_rt_timer_enter_hook = hook;
}

void rt_timer_exit_sethook(void (*hook)(struct rt_timer *timer))
{
    g_rt_timer_exit_hook = hook;
}

static void rt_timer_timeout_adapter(void *parameter)
{
    if (parameter == NULL) {
        return;
    }
    rt_timer_t timer = (rt_timer_t)(UINTPTR)parameter;
    if (g_rt_timer_enter_hook) {
        g_rt_timer_enter_hook(timer);
    }
    timer->timeout_func(timer->parameter);
    if (g_rt_timer_exit_hook) {
        g_rt_timer_exit_hook(timer);
    }
}

void create_timer_adapter(
    rt_timer_t timer, void (*timeout)(void *parameter), void *parameter, rt_tick_t time, rt_uint8_t flag)
{
    UINT16 timer_id;
    if (flag == RT_TIMER_FLAG_PERIODIC) {
        flag = LOS_SWTMR_MODE_PERIOD;
    }
    UINT32 ret = LOS_SwtmrCreate(time, flag, (void *)rt_timer_timeout_adapter, &timer_id, (UINTPTR)timer);
    if (ret != LOS_OK) {
        return;
    }

    for (UINT8 i = 0; i < MAX_TIMERS; i++) {
        if (timer_map[i] == NULL) {
            rt_timer_to_swtmrId_t temp =
                (rt_timer_to_swtmrId_t)LOS_MemAlloc(OS_SYS_MEM_ADDR, sizeof(rt_timer_to_swtmrId));
            temp->timer = timer;
            temp->swtmrId = timer_id;
            timer_map[i] = temp;
            break;
        }
    }
}

void rt_timer_init(rt_timer_t timer, const char *name, void (*timeout)(void *parameter), void *parameter,
    rt_tick_t time, rt_uint8_t flag)
{
    /* parameter check */
    RT_ASSERT(timer != RT_NULL);
    RT_ASSERT(timeout != RT_NULL);
    timer->timeout_func = timeout;
    timer->parameter = parameter;
    timer->init_tick = time;
    create_timer_adapter(timer, timeout, parameter, time, flag);
    timer->parent.type = RT_Object_Class_Timer | RT_Object_Class_Static;
}

rt_err_t rt_timer_detach(rt_timer_t timer)
{
    if (!(timer->parent.type & RT_Object_Class_Static)) {
        return -RT_ERROR;
    }
    UINT8 index = find_timer_adapter(timer);
    if (index == MAX_TIMERS) {
        return -RT_ERROR;
    }
    UINT16 swtmrId = timer_map[index]->swtmrId;
    UINT32 ret = LOS_SwtmrDelete(swtmrId);
    if (ret != LOS_OK) {
        return ret;
    }

    rt_timer_to_swtmrId_t temp = (rt_timer_to_swtmrId_t)timer_map[index];
    ret = LOS_MemFree(OS_SYS_MEM_ADDR, temp);
    if (ret != LOS_OK) {
        return ret;
    }
    timer_map[index] = RT_NULL;
    timer->parent.type = 0;
    return RT_EOK;
}

rt_timer_t rt_timer_create(
    const char *name, void (*timeout)(void *parameter), void *parameter, rt_tick_t time, rt_uint8_t flag)
{
    RT_ASSERT(timeout != RT_NULL);
    struct rt_timer *timer = (struct rt_timer *)LOS_MemAlloc(OS_SYS_MEM_ADDR, sizeof(struct rt_timer));
    if (timer == NULL) {
        return NULL;
    }

    timer->timeout_func = timeout;
    timer->parameter = parameter;
    timer->init_tick = time;
    create_timer_adapter(timer, timeout, parameter, time, flag);
    timer->parent.type = RT_Object_Class_Timer;
    return timer;
}

rt_err_t rt_timer_delete(rt_timer_t timer)
{
    if ((timer->parent.type & RT_Object_Class_Static)) {
        return -RT_ERROR;
    }

    UINT8 index = find_timer_adapter(timer);
    if (index == MAX_TIMERS) {
        return -RT_ERROR;
    }

    UINT16 swtmrId = timer_map[index]->swtmrId;
    UINT32 ret = LOS_SwtmrDelete(swtmrId);
    if (ret != LOS_OK) {
        return ret;
    }

    ret = LOS_MemFree(OS_SYS_MEM_ADDR, timer);
    if (ret != LOS_OK) {
        return ret;
    }
    rt_timer_to_swtmrId_t temp = (rt_timer_to_swtmrId_t)timer_map[index];
    ret = LOS_MemFree(OS_SYS_MEM_ADDR, temp);
    if (ret != LOS_OK) {
        return ret;
    }
    timer_map[index] = RT_NULL;
    return RT_EOK;
}

rt_err_t rt_timer_start(rt_timer_t timer)
{
    UINT8 index = find_timer_adapter(timer);
    if (index == MAX_TIMERS) {
        return -RT_ERROR;
    }

    UINT16 swtmrId = timer_map[index]->swtmrId;
    UINT32 ret = LOS_SwtmrStart(swtmrId);
    if (ret != LOS_OK) {
        return ret;
    }
    return RT_EOK;
}

rt_err_t rt_timer_stop(rt_timer_t timer)
{
    UINT8 index = find_timer_adapter(timer);
    if (index == MAX_TIMERS) {
        return -RT_ERROR;
    }

    UINT16 swtmrId = timer_map[index]->swtmrId;
    UINT32 ret = LOS_SwtmrStop(swtmrId);
    if (ret != LOS_OK) {
        return ret;
    }
    return RT_EOK;
}

rt_err_t rt_timer_control(rt_timer_t timer, int cmd, void *arg)
{
    UINT8 index = find_timer_adapter(timer);
    if (index == MAX_TIMERS) {
        return -RT_ERROR;
    }
    UINT16 swtmrId = timer_map[index]->swtmrId;
    LosSwtmrCB *swtmr;
    swtmr = OsSwtmrIdVerify(swtmrId);
    if (swtmr == NULL) {
        return -RT_ERROR;
    }
    switch (cmd) {
        case RT_TIMER_CTRL_GET_TIME: {
            *(rt_tick_t *)arg = swtmr->interval;
            return RT_EOK;
        }
        case RT_TIMER_CTRL_SET_TIME: {
            OsSwtmrStartTimer(swtmrId, *(rt_tick_t *)arg, swtmr->mode);
            return RT_EOK;
        }
        case RT_TIMER_CTRL_SET_ONESHOT: {
            OsSwtmrStartTimer(swtmrId, swtmr->interval, LOS_SWTMR_MODE_ONCE);
            return RT_EOK;
        }
        case RT_TIMER_CTRL_SET_PERIODIC: {
            OsSwtmrStartTimer(swtmrId, swtmr->interval, LOS_SWTMR_MODE_PERIOD);
            return RT_EOK;
        }
        case RT_TIMER_CTRL_GET_STATE: {
            *(rt_uint32_t *)arg = swtmr->mode;
            return RT_EOK;
        }
        case RT_TIMER_CTRL_GET_REMAIN_TIME: {
            UINT32 remaining_ticks;
            UINT32 ret = LOS_SwtmrTimeGet(swtmrId, &remaining_ticks);
            if (ret == LOS_OK) {
                *(rt_tick_t *)arg = rt_tick_get() + (rt_tick_t)remaining_ticks;
                return RT_EOK;
            }
            return -RT_ERROR;
        }
        default:
            return -RT_ERROR;
    }
}
