/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2025. All rights reserved.
 * Description : LiteOS adapt rtthread waitqueue.
 * Author : Huawei LiteOS Team
 * Create : 2025-12-01
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
#ifdef LOSCFG_COMPAT_LINUX_WAITQUEUE

#include <rtthread.h>
#include <linux/wait.h>
#include "waitqueue.h"

#define wait_event_adapt_rt(wait, condition, timeout) ({                                                   \
    (void)LOS_EventRead(&(wait).stEvent, 0x1U, LOS_WAITMODE_AND | LOS_WAITMODE_CLR, timeout);              \
})

void rt_wqueue_init(rt_wqueue_t *queue)
{
    if (queue == RT_NULL) {
        return;
    }
    wait_queue_head_t *wq =
        (wait_queue_head_t *)LOS_MemAlloc((void *)m_aucSysMem0, sizeof(wait_queue_head_t));
    if (wq == RT_NULL) {
        return;
    }
    init_waitqueue_head(wq);
    // 存储 wait_queue_head_t 的指针到 queue->flag
    queue->flag = (rt_uint32_t)wq;
}


int rt_wqueue_wait(rt_wqueue_t *queue, int condition, int timeout)
{
    if (queue == RT_NULL) {
        return RT_ERROR;
    }
    if ((condition) || ((UINT32)timeout == 0))
        return RT_EOK;

    wait_queue_head_t *wq = (wait_queue_head_t *)(queue->flag);
    if (wq == RT_NULL) {
        return RT_ERROR;
    }

    wait_event_adapt_rt(*wq, condition, (UINT32)timeout);
    return RT_EOK;
}

void rt_wqueue_wakeup(rt_wqueue_t *queue, void *key)
{
    RT_UNUSED(key);
    if (queue == RT_NULL) {
        return;
    }

    wait_queue_head_t *wq = (wait_queue_head_t *)(queue->flag);
    if (wq == RT_NULL) {
        return;
    }
    wake_up_interruptible(wq);
}

void rt_wqueue_add(rt_wqueue_t *queue, struct rt_wqueue_node *node)
{
}

void rt_wqueue_remove(struct rt_wqueue_node *node)
{
}

int rt_wqueue_wait_killable(rt_wqueue_t *queue, int condition, int timeout)
{
    return rt_wqueue_wait(queue, condition, timeout);
}

int rt_wqueue_wait_interruptible(rt_wqueue_t *queue, int condition, int timeout)
{
    return rt_wqueue_wait(queue, condition, timeout);
}

void rt_wqueue_wakeup_all(rt_wqueue_t *queue, void *key)
{
    return rt_wqueue_wakeup(queue, key);
}

#endif