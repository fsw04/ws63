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

#include "los_membox.h"
#include "rtthread.h"
#include "rtdef.h"
#include "rttypes.h"
#include "los_memory.h"
#include "los_task.h"
#include "securec.h"

#ifdef LOSCFG_AARCH64
#define OS_MEMBOX_MAGIC 0xa55a5aa5a55a5aa5
#else
#define OS_MEMBOX_MAGIC 0xa55a5aa5
#endif

#ifdef RT_USING_HOOK
static void (*rt_mp_alloc_hook)(struct rt_mempool *mp, void *block);
static void (*rt_mp_free_hook)(struct rt_mempool *mp, void *block);

/**
 * This function will set a hook function, which will be invoked when a memory
 * block is allocated from memory pool.
 *
 * @param hook the hook function
 */
void rt_mp_alloc_sethook(void (*hook)(struct rt_mempool *mp, void *block))
{
    rt_mp_alloc_hook = hook;
}

/**
 * This function will set a hook function, which will be invoked when a memory
 * block is released to memory pool.
 *
 * @param hook the hook function
 */
void rt_mp_free_sethook(void (*hook)(struct rt_mempool *mp, void *block))
{
    rt_mp_free_hook = hook;
}
#endif

#ifdef LOSCFG_KERNEL_MEMBOX
#ifdef LOSCFG_KERNEL_MEMBOX_STATIC
static void set_magic(void *addr) // copy from @los_membox.c
{
    ((LOS_MEMBOX_NODE *)(addr))->pstNext = (LOS_MEMBOX_NODE *)OS_MEMBOX_MAGIC;
}

static void *get_node_addr(void *addr) // copy from @los_membox.c
{
    return ((LOS_MEMBOX_NODE *)(VOID *)((UINT8 *)(addr) - OS_MEMBOX_NODE_HEAD_SIZE));
}

rt_err_t rt_mp_init(struct rt_mempool *mp, const char *name, void *start,
                    rt_size_t size, rt_size_t block_size)
{
    if (mp == NULL || name == NULL || start == NULL || size == 0 || block_size == 0) {
        return -RT_ERROR;
    }
#if RT_NAME_MAX > 0
    (void)strncpy_s(mp->parent.name, RT_NAME_MAX + 1, name, RT_NAME_MAX);  /* copy name */
#else
    mp->parent.name = name;
#endif /* RT_NAME_MAX > 0 */
    mp->start_address = start;
    mp->size = size;
    // block size is calculated based on the LiteOS structure
    mp->size = RT_ALIGN_DOWN(size, RT_ALIGN_SIZE);

    /* align the block size */
    mp->block_size = RT_ALIGN(block_size, RT_ALIGN_SIZE);
    mp->block_free_count = mp->block_total_count;
    mp->block_list = NULL; // temporarily not maintaining the block_list
    UINT32 ret = LOS_MemboxInit(start, mp->size, block_size);
    if (ret != LOS_OK) {
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t rt_mp_detach(struct rt_mempool *mp)
{
    if (mp == NULL || mp->start_address == NULL) {
        return -RT_ERROR;
    }

    mp->start_address = NULL;
    mp->size = 0;
    mp->block_size = 0;
    mp->block_free_count = 0;
    mp->block_total_count = 0;

    return RT_EOK;
}

rt_mp_t rt_mp_create(const char *name, rt_size_t block_size, rt_size_t block_count)
{
    UINT32 ret;
    UINT32 size;
    rt_mp_t mp = NULL;

    if (name == NULL || block_size == 0 || block_count == 0) {
        return RT_NULL;
    }

    mp = (rt_mp_t)LOS_MemAlloc(m_aucSysMem0, sizeof(struct rt_mempool));
    if (mp == NULL) {
        return RT_NULL;
    }

    block_count++; // in LOS, the first node doesn't used

    block_size = RT_ALIGN(block_size, RT_ALIGN_SIZE);

    size = (block_size + sizeof(rt_uint8_t *)) * block_count;
    // get start address from system heap
    VOID *start = LOS_MemAlloc(m_aucSysMem0, size);

    if (start == NULL) {
        LOS_MemFree(m_aucSysMem0, mp);
        return RT_NULL;
    }
    ret = rt_mp_init(mp, name, start, size, block_size);
    if (ret != RT_EOK) {
        LOS_MemFree(m_aucSysMem0, start);
        LOS_MemFree(m_aucSysMem0, mp);
        return RT_NULL;
    }

    return mp;
}

rt_err_t rt_mp_delete(rt_mp_t mp)
{
    if (mp == NULL) {
        return -RT_ERROR;
    }
    rt_mp_detach(mp);
    LOS_MemFree(m_aucSysMem0, mp->start_address);
    LOS_MemFree(m_aucSysMem0, mp);
    return RT_EOK;
}

void *rt_mp_alloc(rt_mp_t mp, rt_int32_t time)
{
    if (time == 0 || mp == NULL || mp->start_address == NULL) {
        return RT_NULL;
    }

    LOS_TaskLock();

    UINT32 *mem = LOS_MemboxAlloc(mp->start_address);
    if (mem == NULL) {
        LOS_TaskUnlock();
        return RT_NULL;
    }

    LOS_MEMBOX_NODE *node = NULL;
    mp->block_free_count--;
    node = get_node_addr(mem); // get node addr
    node->pstNext = (void *)mp;

    LOS_TaskUnlock();

    RT_OBJECT_HOOK_CALL(rt_mp_alloc_hook, (mp, mem));

    return mem;
}

void rt_mp_free(void *node)
{
    if (node == NULL) {
        return;
    }

    void *addr = get_node_addr(node); // for:1.get the mapped mp; 2.restore the magic before free check
    
    rt_mp_t mp = (void *)(((LOS_MEMBOX_NODE *)addr)->pstNext); // 1.get the mapped mp
    if (mp == NULL) {
        return;
    }

    RT_OBJECT_HOOK_CALL(rt_mp_free_hook, (mp, node));

    LOS_TaskLock();

    set_magic(addr); // 2.restore magic before free check

    UINT32 ret = LOS_MemboxFree(mp->start_address, node); // releases the memory of the node
    if (ret != LOS_OK) {
        LOS_TaskUnlock();
        return;
    }

    mp->block_free_count++;

    LOS_TaskUnlock();
}
#endif
#endif