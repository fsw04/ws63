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
#include "rtdef.h"
#include "los_memory.h"
#include "rttypes.h"

#ifdef RT_USING_HOOK
static void (*rt_malloc_hook)(void **ptr, rt_size_t size);
static void (*rt_free_hook)(void **ptr);

/**
 * @ingroup group_Hook
 * @{
 */

/**
 * @brief This function will set a hook function, which will be invoked when a memory
 *        block is allocated from heap memory.
 *
 * @param hook the hook function.
 */
void rt_malloc_sethook(void (*hook)(void **ptr, rt_size_t size))
{
    rt_malloc_hook = hook;
}

/**
 * @brief This function will set a hook function, which will be invoked when a memory
 *        block is released to heap memory.
 *
 * @param hook the hook function
 */
void rt_free_sethook(void (*hook)(void **ptr))
{
    rt_free_hook = hook;
}

/**@}*/

#endif /* RT_USING_HOOK */

rt_err_t rt_memheap_init(struct rt_memheap *memheap, const char *name,
                         void *start_addr, rt_uint32_t size)
{
    if (memheap == NULL) {
        return RT_NULL;
    }

    memheap->start_addr     = start_addr;
    memheap->pool_size      = RT_ALIGN_DOWN(size, RT_ALIGN_SIZE);
    memheap->available_size = memheap->pool_size - (
        2 * RT_ALIGN(sizeof(struct rt_memheap_item), RT_ALIGN_SIZE)); // 2 is double
    memheap->max_used_size  = memheap->pool_size - memheap->available_size;

    return RT_EOK;
}

void *rt_malloc(rt_size_t size)
{
    if (size == 0) {
        return NULL;
    }

    void *ptr = LOS_MemAlloc(m_aucSysMem0, size);

    RT_OBJECT_HOOK_CALL(rt_malloc_hook, (&ptr, size));

    return ptr;
}

void rt_free(void *rmem)
{
    if (rmem == NULL) {
        return;
    }

    RT_OBJECT_HOOK_CALL(rt_free_hook, (rmem));

    LOS_MemFree(m_aucSysMem0, rmem);
}

void *rt_realloc(void *rmem, rt_size_t newsize)
{
    if (newsize == 0) {
        return RT_NULL;
    }

    if (rmem == NULL) {
        return rt_malloc(newsize);
    }
    UINT32 *ret = LOS_MemRealloc(m_aucSysMem0, rmem, newsize);
    return ret;
}

void *rt_calloc(rt_size_t count, rt_size_t size)
{
    if (count == 0 || size == 0) {
        return RT_NULL;
    }

    UINT32 *ptr = rt_malloc(count * size);
    if (ptr == NULL) {
        return RT_NULL;
    }
    (void)memset_s(ptr, count * size, 0, count * size);
    return ptr;
}

rt_weak void rt_memory_info(rt_size_t *total, rt_size_t *used, rt_size_t *max_used)
{
    if (total != RT_NULL) {
        UINT32 pool_total = LOS_MemPoolSizeGet(m_aucSysMem0);
        *total = pool_total;
    }

    if (used != RT_NULL) {
        UINT32 pool_used = LOS_MemTotalUsedGet(m_aucSysMem0);
        *used = pool_used;
    }

    if (max_used != RT_NULL) {
        UINT32 peak_used = OS_NULL_INT;
#ifdef LOSCFG_MEM_TASK_STAT
        peak_used = LOS_MemTotalPeakUsedGet(m_aucSysMem0);
#endif
        *max_used = peak_used;
    }
}
