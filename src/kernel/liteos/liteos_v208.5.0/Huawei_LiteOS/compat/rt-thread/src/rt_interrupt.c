/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2025. All rights reserved.
 * Description : LiteOS adapt rtthread interrupt.
 * Author : Huawei LiteOS Team
 * Create : 2025-7-14
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
#include "los_typedef.h"
#include "los_printf.h"
#include "los_hwi.h"
#include "los_hwi_pri.h"
#include "rtdef.h"
#include "rthw.h"
#include "rtthread.h"

#define INVALID_VALUE   0xDEEDBEEF       // Invalid parameter test
#define RTT_DEFAULT_PRIORITY      6      // init default interrupt priority

#if defined(RT_USING_HOOK) && defined(RT_HOOK_USING_FUNC_PTR)

static void (*rt_interrupt_enter_hook)(void);
static void (*rt_interrupt_leave_hook)(void);

void rt_interrupt_enter_sethook(void (*hook)(void))
{
    rt_interrupt_enter_hook = hook;
}

void rt_interrupt_leave_sethook(void (*hook)(void))
{
    rt_interrupt_leave_hook = hook;
}
#endif /* RT_USING_HOOK */

#ifdef LOSCFG_DEBUG_HWI
STATIC void* g_rttpDevIdTable[LOSCFG_PLATFORM_HWI_LIMIT] = {
    [0 ... LOSCFG_PLATFORM_HWI_LIMIT - 1] = (void*)0xDEEDBEEF};           // record rtt interrupt pDevId
STATIC void* g_oldHandlerRecord[LOSCFG_PLATFORM_HWI_LIMIT] = {NULL};                 // record old handler
#endif

void rt_hw_interrupt_set_priority(int hwiNum, unsigned int priority);
void rt_hw_interrupt_clear_pending(int hwiNum);
void rt_hw_interrupt_set_priority_mask(unsigned int priority);
unsigned int rt_hw_interrupt_get_priority(int hwiNum);

void rt_interrupt_enter(void)
{
    INT64 level = rt_hw_interrupt_disable();
    RT_OBJECT_HOOK_CALL(rt_interrupt_enter_hook, ());
    rt_hw_interrupt_enable(level);
}

void rt_interrupt_leave(void)
{
    INT64 level = rt_hw_interrupt_disable();
    RT_OBJECT_HOOK_CALL(rt_interrupt_leave_hook, ());
    rt_hw_interrupt_enable(level);
}
__attribute__((weak)) rt_uint8_t rt_interrupt_get_nest(void)
{
    rt_uint8_t ret;
    rt_base_t level;

    level = rt_hw_interrupt_disable();
    ret = OsIrqNestingCntGet();
    rt_hw_interrupt_enable(level);
    return ret;
}

rt_base_t rt_hw_interrupt_disable(void)
{
    return LOS_IntLock();
}

void rt_hw_interrupt_enable(rt_base_t level)
{
    LOS_IntRestore(level);
}

void rt_hw_interrupt_init(void)
{
    OsHwiInit();
    for (int i = 0; i < LOSCFG_PLATFORM_HWI_LIMIT; i++) {
        rt_hw_interrupt_mask(i);
        rt_hw_interrupt_set_priority(i, RTT_DEFAULT_PRIORITY);
        rt_hw_interrupt_clear_pending(i);
    }
    rt_hw_interrupt_set_priority_mask(0);
}

void rt_hw_interrupt_mask(int hwiNum)
{
    LOS_HwiDisable(hwiNum);
}

void rt_hw_interrupt_umask(int hwiNum)
{
    LOS_HwiEnable(hwiNum);
}

#ifdef LOSCFG_DEBUG_HWI
rt_isr_handler_t rt_hw_interrupt_install(int hwiNum, rt_isr_handler_t handler,
                                         void *param, const char *name)
{
    if (hwiNum < 0) {
        return RT_NULL;
    }
    rt_isr_handler_t old_handler;
    HwiHandleInfo *hwiForm = NULL;
    HWI_IRQ_PARAM_S los_param;
    UINT32 hwiPrio, ret, level = 0;
    
    hwiForm = g_hwiOps->getHandleForm(hwiNum);
#ifdef LOSCFG_SHARED_IRQ
    if (hwiForm->shareMode & IRQF_SHARED) {
        old_handler = (rt_isr_handler_t) g_oldHandlerRecord[hwiNum];
    } else {
        old_handler = (rt_isr_handler_t) hwiForm->next->hook;
    }
#else
    old_handler = (rt_isr_handler_t) hwiForm->hook;
#endif
    los_param.swIrq  = hwiNum;
    los_param.pDevId = g_rttpDevIdTable[hwiNum];
    los_param.pName  = name;
    HwiStatus status;
    ret = g_hwiOps->getIrqStatus(hwiNum, &status);
    if (ret == LOS_OK) {
        /* LOS_HwiDelete may disable interrupt, so it is necessary to record
         * the current interrupt enable status to facilitate subsequent restoration.
         * However, if the current state is off, it will not turn on subsequently.
         */
        level = status.enable;
    }
    LOS_HwiDelete(hwiNum, &los_param);

    g_rttpDevIdTable[hwiNum] = param;
    g_oldHandlerRecord[hwiNum] = handler;
    los_param.pDevId = param;
#ifdef LOSCFG_DEBUG_HWI
    hwiPrio = rt_hw_interrupt_get_priority(hwiNum);
#else
    hwiPrio = RTT_DEFAULT_PRIORITY;
#endif
    ret = LOS_HwiCreate(hwiNum, hwiPrio, 0, (HWI_PROC_FUNC)handler, &los_param);
    
    if (level) {
        g_hwiOps->enableIrq(hwiNum);
    }
    
    if (ret != LOS_OK) {
        return RT_NULL;
    }

    return old_handler;
}
#endif

int rt_hw_interrupt_get_irq(void)
{
    return HalCurIrqGet();
}

void rt_hw_interrupt_ack(int hwiNum)
{
    LOS_HwiClear(hwiNum);
}

void rt_hw_interrupt_set_pending(int hwiNum)
{
    LOS_HwiTrigger(hwiNum);
}

void rt_hw_interrupt_clear_pending(int hwiNum)
{
    LOS_HwiClear(hwiNum);
}

void rt_hw_interrupt_set_priority(int hwiNum, unsigned int priority)
{
    LOS_HwiSetPriority(hwiNum, priority);
}

#ifdef LOSCFG_DEBUG_HWI
unsigned int rt_hw_interrupt_get_pending(int hwiNum)
{
    HwiStatus status;
    UINT32 ret = OsHwiStatusGet(hwiNum, NULL, &status);
    if (ret != LOS_OK) {
        return -RT_ERROR;
    }
    return status.pending;
}

unsigned int rt_hw_interrupt_get_priority(int hwiNum)
{
    HwiStatus status;
    UINT32 ret = OsHwiStatusGet(hwiNum, NULL, &status);
    if (ret != LOS_OK) {
        return -RT_ERROR;
    }
    return status.pri;
}
#endif

#ifdef RT_USING_SMP
void rt_hw_ipi_send(int ipi_vector, unsigned int cpu_mask)
{
    return;
}

void rt_hw_ipi_handler_install(int ipi_vector, rt_isr_handler_t ipi_isr_handler)
{
    return;
}

void rt_hw_interrupt_set_target_cpus(int hwiNum, unsigned int cpu_mask)
{
    return;
}

unsigned int rt_hw_interrupt_get_target_cpus(int hwiNum)
{
    return LOS_OK;
}
#endif

unsigned int rt_hw_interrupt_get_prior_group_bits(void)
{
    return LOS_OK;
}

int rt_hw_interrupt_set_prior_group_bits(unsigned int bits)
{
    return LOS_OK;
}

void rt_hw_interrupt_set_triger_mode(int hwiNum, unsigned int mode)
{
    return;
}

unsigned int rt_hw_interrupt_get_triger_mode(int hwiNum)
{
    return LOS_OK;
}