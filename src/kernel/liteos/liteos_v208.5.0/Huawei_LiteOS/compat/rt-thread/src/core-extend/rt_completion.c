/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2025. All rights reserved.
 * Description : LiteOS adapt rtthread completion.
 * Author : Huawei LiteOS Team
 * Create : 2025-11-24
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
#include <rtdbg.h>

#include <rtthread.h>
#include <rthw.h>
#include <rtdevice.h>
#include "linux/completion.h"
#include "los_memory.h"

#ifdef LOSCFG_COMPAT_LINUX_COMPLETION

void rt_completion_init(struct rt_completion *completion)
{
    completion_t *los_completion;
    if (completion == RT_NULL) {
        return;
    }
    los_completion = (completion_t *)LOS_MemAlloc(m_aucSysMem0, sizeof(completion_t));
    init_completion(los_completion);
    completion->susp_thread_n_flag = (rt_base_t)los_completion;
}

rt_err_t rt_completion_wait(struct rt_completion *completion,
                            rt_int32_t            timeout)
{
    return rt_completion_wait_flags(completion, timeout, RT_UNINTERRUPTIBLE);
}

rt_err_t rt_completion_wait_noisr(struct rt_completion *completion,
                                  rt_int32_t            timeout)
{
    return rt_completion_wait_flags_noisr(completion, timeout, RT_UNINTERRUPTIBLE);
}

rt_err_t rt_completion_wait_flags(struct rt_completion *completion,
                                  rt_int32_t timeout, int suspend_flag)
{
    return rt_completion_wait_flags_noisr(completion, timeout, suspend_flag);
}

rt_err_t rt_completion_wait_flags_noisr(struct rt_completion *completion,
                                        rt_int32_t timeout, int suspend_flag)
{
    UINT32 ret = RT_EOK;
    RT_UNUSED(suspend_flag);
    if (completion == RT_NULL || (completion_t *)completion->susp_thread_n_flag == RT_NULL) {
        return RT_EINVAL;
    }
    if (timeout == RT_WAITING_FOREVER) {
        ret = wait_for_completion_timeout((completion_t *)completion->susp_thread_n_flag, LOS_WAIT_FOREVER);
    } else if (timeout >= 0) {
        ret = wait_for_completion_timeout((completion_t *)completion->susp_thread_n_flag, timeout);
    } else {
        return RT_EINVAL;
    }
    return ret == 0? -RT_ETIMEOUT : RT_EOK;
}

void rt_completion_done(struct rt_completion *completion)
{
    rt_completion_wakeup_by_errno(completion, -1);
}

rt_err_t rt_completion_wakeup(struct rt_completion *completion)
{
    return rt_completion_wakeup_by_errno(completion, -1);
}

rt_err_t rt_completion_wakeup_by_errno(struct rt_completion *completion,
                                       rt_err_t thread_errno)
{
    RT_UNUSED(thread_errno);
    if (completion == RT_NULL || (completion_t *)completion->susp_thread_n_flag == RT_NULL) {
        return RT_EINVAL;
    }
    complete((completion_t *)completion->susp_thread_n_flag);
    return RT_EOK;
}

void rt_completion_destory(struct rt_completion *completion)
{
    if (completion == RT_NULL || (completion_t *)completion->susp_thread_n_flag == RT_NULL) {
        return;
    }
    LOS_MemFree((void*)m_aucSysMem0, (completion_t *)completion->susp_thread_n_flag);
    completion->susp_thread_n_flag = (rt_base_t)NULL;
}

#endif /* LOSCFG_COMPAT_LINUX_COMPLETION */