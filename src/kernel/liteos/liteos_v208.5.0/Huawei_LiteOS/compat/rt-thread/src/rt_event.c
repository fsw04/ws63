/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2025. All rights reserved.
 * Description : LiteOS adapt rtthread tick.
 * Author : Huawei LiteOS Team
 * Create : 2025-8-11
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
#include "los_event.h"
#include "los_task.h"
#include "rtdef.h"
#include "rttypes.h"
#include "securec.h"
#include "los_list.h"
#include "los_errno.h"

static UINT32 get_mode(rt_uint8_t rt_option)
{
    UINT32 liteos_mode = 0;

    if (rt_option & RT_EVENT_FLAG_AND) {
        liteos_mode |= LOS_WAITMODE_AND;
    } else if (rt_option & RT_EVENT_FLAG_OR) {
        liteos_mode |= LOS_WAITMODE_OR;
    } else {
        return LOS_ERRNO_EVENT_FLAGS_INVALID;
    }

    if (rt_option & RT_EVENT_FLAG_CLEAR) {
        liteos_mode |= LOS_WAITMODE_CLR;
    }
    return liteos_mode;
}

rt_err_t rt_event_init(rt_event_t rt_event, const char *name, rt_uint8_t flag)
{
    if (rt_event == RT_NULL) {
        return -RT_ERROR;
    }

    if (flag != RT_IPC_FLAG_FIFO || flag == RT_IPC_FLAG_PRIO) {
        return -RT_ERROR;
    }

    UINT32 ret;
    EVENT_CB_S *los_event = (EVENT_CB_S *)LOS_MemAlloc(m_aucSysMem0, sizeof(EVENT_CB_S));
    
    ret = LOS_EventInit(los_event);
    if (ret != LOS_OK) {
        LOS_MemFree(m_aucSysMem0, los_event);
        return RT_NULL;
    }

    rt_event->parent.parent.flag = flag; // LOS is no used flag now, just assignment
    rt_event->set = (rt_uint32_t)los_event; // later, can find the associated los_event through rt_event_t->set
    rt_event->parent.parent.type = RT_Object_Class_Event | RT_Object_Class_Static;
    return RT_EOK;
}

rt_err_t rt_event_detach(rt_event_t event)
{
    if (event == RT_NULL) {
        return -RT_ERROR;
    }

    if (!(event->parent.parent.type & RT_Object_Class_Static)) {
        return -RT_ERROR;
    }

    LOS_EventDestroy((PEVENT_CB_S)event->set); // destroy los_event
    LOS_MemFree(m_aucSysMem0, (void *)event->set);
    event->set = RT_NULL;
    return RT_EOK;
}

rt_event_t rt_event_create(const char *name, rt_uint8_t flag)
{
    if (flag != RT_IPC_FLAG_FIFO || flag == RT_IPC_FLAG_PRIO) {
        return RT_NULL;
    }
    rt_event_t event = (rt_event_t)LOS_MemAlloc(m_aucSysMem0, sizeof(rt_event_t));
    if (event == NULL) {
        return RT_NULL;
    }

    UINT32 ret = rt_event_init(event, name, flag);
    if (ret != RT_EOK) {
        LOS_MemFree(m_aucSysMem0, event);
        return RT_NULL;
    }

    event->parent.parent.type = RT_Object_Class_Event;
    return event;
}

rt_err_t rt_event_delete(rt_event_t event)
{
    if (event == RT_NULL) {
        return -RT_ERROR;
    }

    if (event->parent.parent.type & RT_Object_Class_Static) {
        return -RT_ERROR;
    }

    rt_event_detach(event);
    LOS_MemFree(m_aucSysMem0, event); // used in combination with rt_event_create, the event needs to be released
    return RT_EOK;
}

rt_err_t rt_event_send(rt_event_t event, rt_uint32_t set)
{
    if (event == RT_NULL) {
        return -RT_ERROR;
    }

    UINT32 ret;
    ret = LOS_EventWrite((PEVENT_CB_S)event->set, set);
    if (ret != LOS_OK) {
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t rt_event_recv(rt_event_t event, rt_uint32_t set, rt_uint8_t option,
                       rt_int32_t timeout, rt_uint32_t *recved)
{
    if (event == RT_NULL) {
        return -RT_ERROR;
    }

    UINT32 mode = get_mode(option);
    if (mode == LOS_ERRNO_EVENT_FLAGS_INVALID || recved == NULL) {
        return -RT_ERROR;
    }
    UINT32 ret = LOS_EventRead((PEVENT_CB_S)event->set, set, mode, timeout);
    if (ret & LOS_ERRNO_EVENT_SETBIT_INVALID) {
        if (ret == LOS_ERRNO_EVENT_READ_TIMEOUT) {
            return -RT_ETIMEOUT;
        }
        return -RT_ERROR;
    }
    if (ret == 0) {
        return -RT_ERROR;
    }

    *recved = ret;
    return RT_EOK;
}