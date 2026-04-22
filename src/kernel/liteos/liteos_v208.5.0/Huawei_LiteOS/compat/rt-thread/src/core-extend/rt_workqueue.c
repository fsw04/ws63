/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2025. All rights reserved.
 * Description : LiteOS adapt rtthread workqueue.
 * Author : Huawei LiteOS Team
 * Create : 2025-12-02
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
#include <linux/workqueue.h>
#include "workqueue.h"

#ifdef LOSCFG_COMPAT_LINUX_WORKQUEUE

struct rt_list_node g_rtwq_list = {&g_rtwq_list, &g_rtwq_list};
struct wq_node {
    struct rt_work *rtw;
    struct work_struct *losw;
    struct rt_list_node node;
};

static inline void set_losq_to_rtq(struct rt_workqueue *rtq, struct workqueue_struct *losq)
{
    rtq->work_thread = (struct rt_thread *)losq;
}

static inline struct workqueue_struct *get_losq_from_rtq(struct rt_workqueue *rtq)
{
    return (struct workqueue_struct *)(rtq->work_thread);
}

static inline void set_losw_to_rtw(struct rt_work *rtw, struct work_struct *losw)
{
    rtw->timeout_tick = (rt_tick_t)losw;
}

static inline struct work_struct *get_losw_from_rtw(struct rt_work *rtw)
{
    return (struct work_struct *)(rtw->timeout_tick);
}

/* 创建rtq、losq, 并单向关联rtq->losq */
struct rt_workqueue *rt_workqueue_create(const char *name, rt_uint16_t stack_size, rt_uint8_t priority)
{
    RT_UNUSED(stack_size);
    RT_UNUSED(priority);
    if (name == NULL) {
        return NULL;
    }
    struct workqueue_struct *losq = create_singlethread_workqueue((char *)name);
    if (losq == NULL) {
        return NULL;
    }

    struct rt_workqueue *rtq = (struct rt_workqueue *)LOS_MemAlloc(m_aucSysMem0, sizeof(struct rt_workqueue));
    if (rtq == NULL) {
        destroy_workqueue(losq);
        return NULL;
    }

    set_losq_to_rtq(rtq, losq);
    rt_list_init(&(rtq->work_list));
    return rtq;
}


static struct wq_node *rt_find_work_node(const struct work_struct *work)
{
    struct rt_list_node *cur = NULL;

    if (work == NULL) {
        return NULL;
    }

    if (rt_list_isempty(&g_rtwq_list)) {
        return NULL;
    }
    LOS_TaskLock();
    rt_list_for_each(cur, &g_rtwq_list) {
        struct wq_node *wn = rt_list_entry(cur, struct wq_node, node);
        if (wn->losw == work) {
            LOS_TaskUnlock();
            return wn;
        }
    }
    LOS_TaskUnlock();
    return NULL;
}

void work_func_adapter(struct work_struct *losw)
{
    struct wq_node *wq_node = rt_find_work_node(losw);
    if (wq_node == NULL) {
        return;
    }
    struct rt_work *rtw = wq_node->rtw;
    if (rtw == NULL || rtw->work_func == NULL) {
        return;
    }
    void *work_data = rtw->work_data;
    rtw->work_func(rtw, work_data);
}

/* 初始化rt_work结构体, 创建losw, 双向关联rtw<->losw */
void rt_work_init(struct rt_work *work, void (*work_func)(struct rt_work *work, void *work_data), void *work_data)
{
    struct work_struct *losw = NULL;
    struct wq_node *w_node = NULL;
    if (work == NULL || work_func == NULL) {
        return;
    }
    losw = LOS_MemAlloc(m_aucSysMem0, sizeof(struct work_struct));
    if (losw == NULL) {
        return;
    }

    w_node = LOS_MemAlloc((void *)m_aucSysMem0, sizeof(struct wq_node));
    if (w_node == NULL) {
        LOS_MemFree((void *)m_aucSysMem0, (void *)losw);
        return;
    }
    INIT_WORK(losw, work_func_adapter);
    rt_list_init(&(work->list));
    w_node->rtw = work;
    w_node->losw = losw;
    LOS_TaskLock();
    rt_list_insert_after(&g_rtwq_list, &(w_node->node));
    LOS_TaskUnlock();
    set_losw_to_rtw(work, losw);
    work->work_data = work_data;
    work->work_func = work_func;
}

/* 回收rtq, 回收losq以及losq->losw, rtw资源释放? */
rt_err_t rt_workqueue_destroy(struct rt_workqueue *queue)
{
    struct rt_work *work = NULL;
    if (queue == NULL) {
        return RT_ERROR;
    }
    struct workqueue_struct *losq = get_losq_from_rtq(queue);
    if (losq == NULL) {
        return RT_ERROR;
    }
    LOS_TaskLock();
    rt_list_for_each_entry(work, &queue->work_list, list) {
        struct wq_node *wn = rt_find_work_node(get_losw_from_rtw(work));
        if (wn != NULL) {
            rt_list_remove(&wn->node);
            LOS_MemFree(m_aucSysMem0, wn->losw);
            LOS_MemFree(m_aucSysMem0, wn);
        }
    }
    destroy_workqueue(losq);
    set_losq_to_rtq(queue, NULL);
    LOS_MemFree(m_aucSysMem0, queue);
    LOS_TaskUnlock();
    return RT_EOK;
}

/* 把rtw添加到rtq, 把losw添加到losq 建立losq链表->losw节点 */
rt_err_t rt_workqueue_dowork(struct rt_workqueue *queue, struct rt_work *work)
{
    if (work == NULL || queue == NULL) {
        return RT_ERROR;
    }
    struct workqueue_struct *losq = get_losq_from_rtq(queue);
    struct work_struct *losw = get_losw_from_rtw(work);
    if (losq == NULL || losw == NULL) {
        return RT_ERROR;
    }
    LOS_TaskLock();
    rt_list_insert_after(&(work->list), &queue->work_list);
    queue_work(losq, losw);
    LOS_TaskUnlock();
    return RT_EOK;
}

/* 把rtwork从rtq移除, 把losw从losq移除 */
rt_err_t rt_workqueue_cancel_work(struct rt_workqueue *queue, struct rt_work *work)
{
    if (queue == NULL || work == NULL) {
        return RT_ERROR;
    }

    struct work_struct *losw = get_losw_from_rtw(work);
    if (losw == NULL) {
        return RT_ERROR;
    }
    LOS_TaskLock();
    int found = 0;
    struct rt_list_node *cur;
    rt_list_for_each(cur, &queue->work_list) {
        if (cur == &work->list) {
            found = 1;
            break;
        }
    }
    LOS_TaskUnlock();
    if (!found) {
        return RT_ERROR;
    }

    rt_list_remove(&(work->list));

    struct wq_node *wn = rt_find_work_node(losw);
    if (wn != NULL) {
        rt_list_remove(&wn->node);
        LOS_MemFree(m_aucSysMem0, wn->losw);
        LOS_MemFree(m_aucSysMem0, wn);
        set_losw_to_rtw(work, NULL);
    }
    LOS_ListDelInit(&losw->entry);
    return RT_EOK;
}

/* 把rtwork从rtq移除, 把losw从losq移除 */
rt_err_t rt_workqueue_cancel_work_sync(struct rt_workqueue *queue, struct rt_work *work)
{
    return rt_workqueue_cancel_work(queue, work);
}
#endif