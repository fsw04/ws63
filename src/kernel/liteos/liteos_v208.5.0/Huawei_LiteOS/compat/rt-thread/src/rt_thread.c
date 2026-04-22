/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2025. All rights reserved.
 * Description : LiteOS adapt rtthread thread.
 * Author : Huawei LiteOS Team
 * Create : 2025-7-25
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

#include "los_task.h"
#include "los_task_base.h"
#include "los_task_pri.h"
#include "rtdef.h"
#include "rtthread.h"

#ifdef RT_USING_HOOK
#ifndef RT_IDLE_HOOK_LIST_SIZE
#define RT_IDLE_HOOK_LIST_SIZE 4
#endif /* RT_IDLE_HOOK_LIST_SIZE */

static void (*rt_scheduler_hook)(struct rt_thread *from, struct rt_thread *to);
static void (*rt_thread_suspend_hook)(rt_thread_t thread);
static void (*rt_thread_resume_hook)(rt_thread_t thread);
static void (*rt_thread_inited_hook)(rt_thread_t thread);

/**
 * This function will set a hook function, which will be invoked when thread
 * switch happens.
 *
 * @param hook the hook function
 */
void rt_scheduler_sethook(void (*hook)(struct rt_thread *from,
                                       struct rt_thread *to))
{
    rt_scheduler_hook = hook;
}

static void (*idle_hook_list[RT_IDLE_HOOK_LIST_SIZE])(void);

static void rt_hook_idle_handle(void)
{
    void (*idle_hook)(void);
    for (UINT8 i = 0; i < RT_IDLE_HOOK_LIST_SIZE; i++) {
        idle_hook = idle_hook_list[i];
        if (idle_hook != RT_NULL) {
            idle_hook();
        }
    }
}

/**
 * @ingroup SystemInit
 *
 * This function will initialize idle thread, then start it.
 *
 * @note this function must be invoked when system init.
 */
void rt_thread_idle_init(void)
{
    LOS_IdleHandlerHookReg(rt_hook_idle_handle);
}


/**
 * @ingroup Hook
 * This function sets a hook function to idle thread loop. When the system
 * performs idle loop, this hook function should be invoked.
 *
 * @param hook the specified hook function
 *
 * @return RT_EOK: set OK
 *         -RT_EFULL: hook list is full
 *
 * @note the hook function must be simple and never be blocked or suspend.
 */
rt_err_t rt_thread_idle_sethook(void (*hook)(void))
{
    rt_size_t i;
    rt_err_t ret = -RT_EFULL;
    rt_base_t level;

    level = rt_hw_interrupt_disable();

    for (i = 0; i < RT_IDLE_HOOK_LIST_SIZE; i++) {
        if (idle_hook_list[i] == RT_NULL) {
            idle_hook_list[i] = hook;
            ret = RT_EOK;
            break;
        }
    }
    
    rt_hw_interrupt_enable(level);
    return ret;
}

/**
 * delete the idle hook on hook list
 *
 * @param hook the specified hook function
 *
 * @return RT_EOK: delete OK
 *         -RT_ENOSYS: hook was not found
 */
rt_err_t rt_thread_idle_delhook(void (*hook)(void))
{
    rt_size_t i;
    rt_base_t level;
    rt_err_t ret = -RT_ENOSYS;

    /* disable interrupt */
    level = rt_hw_interrupt_disable();

    for (i = 0; i < RT_IDLE_HOOK_LIST_SIZE; i++) {
        if (idle_hook_list[i] == hook) {
            idle_hook_list[i] = RT_NULL;
            ret = RT_EOK;
            break;
        }
    }
    /* enable interrupt */
    rt_hw_interrupt_enable(level);
    return ret;
}

/**
 * @ingroup Hook
 * This function sets a hook function when the system suspend a thread.
 *
 * @param hook the specified hook function
 *
 * @note the hook function must be simple and never be blocked or suspend.
 */
void rt_thread_suspend_sethook(void (*hook)(rt_thread_t thread))
{
    rt_thread_suspend_hook = hook;
}

/**
 * @ingroup Hook
 * This function sets a hook function when the system resume a thread.
 *
 * @param hook the specified hook function
 *
 * @note the hook function must be simple and never be blocked or suspend.
 */
void rt_thread_resume_sethook(void (*hook)(rt_thread_t thread))
{
    rt_thread_resume_hook = hook;
}

/**
 * @ingroup Hook
 * This function sets a hook function when a thread is initialized.
 *
 * @param hook the specified hook function
 */
void rt_thread_inited_sethook(void (*hook)(rt_thread_t thread))
{
    rt_thread_inited_hook = hook;
}

#endif

VOID *g_threadRt2Los[MAX_TASKS];
struct rt_thread g_tidIdle;

static void create_thread_map(rt_thread_t thread, UINT32 taskId)
{
    LosTaskCB *task = OS_TCB_FROM_TID(taskId);
    g_threadRt2Los[taskId] = (VOID *)thread;
    for (int i = 0; i < RT_NAME_MAX; i++) {
        thread->parent.name[i] = task->taskName[i];
    }
    thread->stack_addr = (void *)task->topOfStack;
    thread->stack_size = task->stackSize;
    thread->sched_thread_ctx.sched_thread_priv.current_priority = task->priority;
    thread->sched_thread_ctx.sched_thread_priv.init_tick = g_threadTick[taskId];
}

static rt_thread_t get_thread_adapter(UINT32 taskId)
{
    /** During system initialization, some tasks are
     *  created that do not have a tid but may be utilized,
     *  such as "app_task". Therefore, it is necessary to
     *  dynamically create "tid" for these tasks.
    */
    if (g_threadRt2Los[taskId] == NULL) {
        rt_thread_t tid = LOS_MemAlloc(OS_SYS_MEM_ADDR, sizeof(struct rt_thread));
        create_thread_map(tid, taskId);
    }
    return (rt_thread_t)g_threadRt2Los[taskId];
}

UINT8 find_thread_adapter_index(rt_thread_t thread)
{
    if (thread == NULL) {
        return MAX_TASKS;
    }
    for (UINT8 i = 0; i < MAX_TASKS; i++) {
        if (thread == g_threadRt2Los[i]) {
            return i;
        }
    }
    return MAX_TASKS;
}

static UINT32 los_to_rt_errno(UINT32 los_err)
{
    if (los_err == LOS_OK) {
        return RT_EOK;
    }

    return -RT_ERROR;
}

void rt_defunct_execute(void)
{
    LOS_TaskResRecycle();
}

rt_thread_t rt_thread_idle_gethandler(void)
{
    UINT32 idleTaskId = OsGetIdleTaskId();
    if (g_threadRt2Los[idleTaskId] == NULL) {
        create_thread_map(&g_tidIdle, idleTaskId);
    }
    return (rt_thread_t)g_threadRt2Los[idleTaskId];
}

void rt_schedule(void)
{
    LOS_TaskYield();
}

rt_base_t rt_enter_critical(void)
{
    LOS_TaskLock();
    return (rt_base_t)rt_critical_level();
}

void rt_exit_critical(void)
{
    LOS_TaskUnlock();
}

rt_uint16_t rt_critical_level(void)
{
    Percpu *percpu = NULL;
    percpu = OsPercpuGet();
    LOS_ASSERT(percpu != NULL);
    return (UINT16)percpu->taskLockCnt;
}

#ifdef LOSCFG_TASK_STACK_STATIC_ALLOCATION
rt_err_t rt_thread_init(struct rt_thread *thread,
                        const char       *name,
                        void (*entry)(void *parameter),
                        void             *parameter,
                        void             *stack_start,
                        rt_uint32_t       stack_size,
                        rt_uint8_t        priority,
                        rt_uint32_t       tick)
{
    UINT32 taskId, ret;
    TSK_INIT_PARAM_S initParam = {0};

    if (thread == RT_NULL) {
        return -RT_ERROR;
    }

    if (stack_start == RT_NULL) {
        return -RT_ERROR;
    }

    if (tick == RT_NULL) {
        return -RT_ERROR;
    }

    thread->parent.type = RT_Object_Class_Thread | RT_Object_Class_Static;

    initParam.pcName = (char *)name;
    initParam.pfnTaskEntry = (TSK_ENTRY_FUNC)entry;
    initParam.pArgs = parameter;
    initParam.uwStackSize = stack_size;
    initParam.usTaskPrio = priority;
    ret = LOS_TaskCreateOnlyStatic(&taskId, &initParam, stack_start);
    if (ret == LOS_OK) {
        g_threadTick[taskId] = tick;
        create_thread_map(thread, taskId);
        RT_OBJECT_HOOK_CALL(rt_thread_inited_hook, (thread));
    }
    return los_to_rt_errno(ret);
}
#endif

rt_thread_t rt_thread_self(void)
{
    LosTaskCB *runTask;
    UINT32 taskId;
    runTask = OsCurrTaskGet();
    taskId = runTask->taskId;
    return get_thread_adapter(taskId);
}

rt_err_t rt_thread_startup(rt_thread_t thread)
{
    if (thread == RT_NULL) {
        return -RT_ERROR;
    }

    UINT8 taskId = find_thread_adapter_index(thread);
    if (taskId == MAX_TASKS) {
        return -RT_ERROR;
    }
    return los_to_rt_errno(LOS_TaskResume(taskId));
}

rt_err_t rt_thread_detach(rt_thread_t thread)
{
    if (thread == RT_NULL) {
        return -RT_ERROR;
    }

    if (!(thread->parent.type & RT_Object_Class_Static)) {
        return -RT_ERROR;
    }

    UINT8 taskId = find_thread_adapter_index(thread);
    if (taskId == MAX_TASKS) {
        return -RT_ERROR;
    }
    g_threadRt2Los[taskId] = NULL;
    return los_to_rt_errno(LOS_TaskDelete(taskId));
}

rt_thread_t rt_thread_create(   const char *name,
                                void (*entry)(void *parameter),
                                void       *parameter,
                                rt_uint32_t stack_size,
                                rt_uint8_t  priority,
                                rt_uint32_t tick)
{
    if (tick == RT_NULL) {
        return RT_NULL;
    }

    UINT32 taskId, ret;
    TSK_INIT_PARAM_S initParam = {0};
    
    initParam.pcName = (char *)name;
    initParam.pfnTaskEntry = (TSK_ENTRY_FUNC)entry;
    initParam.pArgs = parameter;
    initParam.uwStackSize = stack_size;
    initParam.usTaskPrio = priority;
    
    rt_thread_t thread = LOS_MemAlloc(OS_SYS_MEM_ADDR, sizeof(struct rt_thread));
    if (thread == NULL) {
        return NULL;
    }

    ret = LOS_TaskCreateOnly(&taskId, &initParam);
    if (ret == LOS_OK) {
        g_threadTick[taskId] = tick;
        create_thread_map(thread, taskId);
        thread = get_thread_adapter(taskId);
        thread->parent.type = RT_Object_Class_Thread;
    } else {
        LOS_MemFree(OS_SYS_MEM_ADDR, thread);
        thread = NULL;
    }
    return thread;
}

rt_err_t rt_thread_delete(rt_thread_t thread)
{
    if (thread == RT_NULL) {
        return -RT_ERROR;
    }

    if (thread->parent.type & RT_Object_Class_Static) {
        return -RT_ERROR;
    }

    UINT8 taskId = find_thread_adapter_index(thread);
    if (taskId == MAX_TASKS) {
        return -RT_ERROR;
    }
    g_threadRt2Los[taskId] = NULL;
    LOS_MemFree(OS_SYS_MEM_ADDR, thread);
    return los_to_rt_errno(LOS_TaskDelete(taskId));
}

rt_err_t rt_thread_yield(void)
{
    return los_to_rt_errno(LOS_TaskYield());
}

rt_err_t rt_thread_sleep(rt_tick_t tick)
{
    return los_to_rt_errno(LOS_TaskDelay(tick));
}


rt_err_t rt_thread_delay(rt_tick_t tick)
{
    return los_to_rt_errno(LOS_TaskDelay(tick));
}

rt_err_t rt_thread_mdelay(rt_int32_t ms)
{
    UINT32 tick = LOS_MS2Tick(ms);
    return los_to_rt_errno(LOS_TaskDelay(tick));
}

rt_err_t rt_thread_control(rt_thread_t thread, int cmd, void *arg)
{
    if (thread == RT_NULL) {
        return -RT_ERROR;
    }

    UINT8 taskId = find_thread_adapter_index(thread);
    if (taskId == MAX_TASKS) {
        return -RT_ERROR;
    }

    switch (cmd) {
        case RT_THREAD_CTRL_CHANGE_PRIORITY: {
            LOS_TaskPriSet(taskId, *(UINT16 *)arg);
            break;
        }

        case RT_THREAD_CTRL_STARTUP: {
            return rt_thread_startup(thread);
        }

        case RT_THREAD_CTRL_CLOSE: {
            rt_err_t rt_err = LOS_OK;

            if ((thread->parent.type & RT_Object_Class_Static) != 0) {
                rt_err = rt_thread_detach(thread);
            }
    #ifdef RT_USING_HEAP
            if ((thread->parent.type & RT_Object_Class_Static) == 0) {
                rt_err = rt_thread_delete(thread);
            }
    #endif /* RT_USING_HEAP */
            rt_schedule();
            return rt_err;
        }

    #if defined(RT_USING_SMP) && defined(LOSCFG_KERNEL_SMP)
        case RT_THREAD_CTRL_BIND_CPU: {
            rt_uint8_t cpu;
            LosTaskCB *taskCB = OS_TCB_FROM_TID(taskId);
            if ((thread->stat & RT_THREAD_STAT_MASK) != RT_THREAD_INIT || taskCB == NULL) {
                /* we only support bind cpu before started phase. */
                return -RT_ERROR;
            }

            cpu = (rt_uint8_t)(rt_size_t)arg;
            taskCB->cpuAffiMask = (1 << cpu);
            break;
        }
    #endif /* RT_USING_SMP */

        default:
            return -RT_ERROR;
    }

    return RT_EOK;
}

rt_err_t rt_thread_suspend(rt_thread_t thread)
{
    if (thread == RT_NULL) {
        return -RT_ERROR;
    }

    if (thread != rt_thread_self()) {
        return -RT_ERROR;
    }

    UINT8 taskId = find_thread_adapter_index(thread);
    if (taskId == MAX_TASKS) {
        return -RT_ERROR;
    }

    UINT32 ret = LOS_TaskSuspend(taskId);
    if (ret != LOS_OK) {
        return los_to_rt_errno(ret);
    }

    RT_OBJECT_HOOK_CALL(rt_thread_suspend_hook, (thread));

    return ret;
}

rt_err_t rt_thread_resume(rt_thread_t thread)
{
    if (thread == RT_NULL) {
        return -RT_ERROR;
    }

    UINT8 taskId = find_thread_adapter_index(thread);
    if (taskId == MAX_TASKS) {
        return -RT_ERROR;
    }
    RT_OBJECT_HOOK_CALL(rt_thread_resume_hook, (thread));
    return los_to_rt_errno(LOS_TaskResume(taskId));
}

rt_thread_t rt_thread_find(char *name)
{
    if (name == RT_NULL) {
        return RT_NULL;
    }

    for (UINT32 i = 0; i < MAX_TASKS; i++) {
        rt_thread_t thread = (rt_thread_t)g_threadRt2Los[i];
        if (thread != RT_NULL && strcmp(name, thread->parent.name) == 0) {
            return thread;
        }
    }
    return RT_NULL;
}

rt_err_t rt_thread_delay_until(rt_tick_t *tick, rt_tick_t inc_tick)
{
    if (tick == RT_NULL) {
        return RT_EOK;
    }

    rt_tick_t cur_tick = rt_tick_get();
    if (cur_tick - *tick < inc_tick) {
        rt_tick_t left_tick;
        *tick += inc_tick;
        left_tick = *tick - cur_tick;
        return rt_thread_sleep(left_tick);
    }

    return RT_EOK;
}
