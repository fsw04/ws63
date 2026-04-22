/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2025. All rights reserved.
 * Description : LiteOS adapt rtthread sem.
 * Author : Huawei LiteOS Team
 * Create : 2025-7-18
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
#include "los_sem_pri.h"
#include "los_task.h"
#include "los_sem.h"

#define MAX_SEMS LOSCFG_BASE_IPC_SEM_LIMIT
static VOID *sem_map[MAX_SEMS] = {0};

static UINT8 find_sem_adapter(rt_sem_t sem)
{
    for (UINT8 i = 0; i < MAX_SEMS; i++) {
        if (sem_map[i] == (VOID *)sem)
            return i;
    }
    return MAX_SEMS;
}

static rt_err_t create_sem_adapter(rt_sem_t sem, rt_uint32_t value)
{
    UINT32 sem_id;
    UINT32 ret = LOS_SemCreate(value, &sem_id);
    if (ret != LOS_OK) {
        return -RT_ERROR;
    }
 
    sem_map[sem_id] = (VOID *)sem;
    return RT_EOK;
}

static UINT32 los_to_rt_errno(UINT32 los_err)
{
    if (los_err == LOS_OK) {
        return RT_EOK;
    }

    switch (los_err) {
        case LOS_ERRNO_SEM_UNAVAILABLE:
        case LOS_ERRNO_SEM_TIMEOUT:
            return -RT_ETIMEOUT;

        default:
            return -RT_ERROR;
    }
}

rt_err_t rt_sem_init(rt_sem_t    sem,
                     const char *name,
                     rt_uint32_t value,
                     rt_uint8_t  flag)
{
    if ((sem == RT_NULL) || (name == NULL) || (value >= 0x10000U) || (flag == RT_IPC_FLAG_PRIO) || (flag != RT_IPC_FLAG_FIFO)) { // LiteOS不支持PRIO
        return -RT_ERROR;
    }

    rt_err_t ret = create_sem_adapter(sem, value);
    if (ret != RT_EOK) {
        return ret;
    }
    sem->parent.parent.type = RT_Object_Class_Semaphore | RT_Object_Class_Static;
    sem->parent.parent.flag = flag;
    sem->value = value;
    sem->max_value = value;
#if RT_NAME_MAX > 0
    rt_strncpy(sem->parent.parent.name, name, RT_NAME_MAX - 1);  /* copy name */
    sem->parent.parent.name[RT_NAME_MAX - 1] = '\0';
#else
    sem->parent.parent.name = name;
#endif /* RT_NAME_MAX > 0 */
    return RT_EOK;
}

rt_err_t rt_sem_detach(rt_sem_t sem)
{
    if (sem == RT_NULL) {
        return -RT_ERROR;
    }
    if (!(sem->parent.parent.type & RT_Object_Class_Static)) {
        return -RT_ERROR;
    }

    UINT8 semId = find_sem_adapter(sem);
    if (semId == MAX_SEMS) {
        return -RT_ERROR;
    }
    UINT32 ret = LOS_SemDelete(semId);
    if (ret != LOS_OK) {
        return -RT_ERROR;
    }

    sem_map[semId] = 0;
    sem->parent.parent.type = 0;
    return RT_EOK;
}

rt_sem_t rt_sem_create(const char *name, rt_uint32_t value, rt_uint8_t flag)
{
    if ((name == NULL) || (value >= 0x10000U) || (flag == RT_IPC_FLAG_PRIO) || (flag != RT_IPC_FLAG_FIFO)) { // LiteOS不支持PRIO
        return RT_NULL;
    }

    rt_sem_t sem = (rt_sem_t)LOS_MemAlloc(OS_SYS_MEM_ADDR, sizeof(struct rt_semaphore));
    if (sem == NULL) {
        return RT_NULL;
    }
    rt_memset(sem, 0, sizeof(struct rt_semaphore));
    rt_err_t ret = create_sem_adapter(sem, value);
    if (ret != RT_EOK) {
        LOS_MemFree(OS_SYS_MEM_ADDR, sem);
        return RT_NULL;
    }
    sem->parent.parent.type = RT_Object_Class_Semaphore;
    sem->parent.parent.flag = flag;
    sem->value = value;
    sem->max_value = value;
#if RT_NAME_MAX > 0
    rt_strncpy(sem->parent.parent.name, name, RT_NAME_MAX - 1);  /* copy name */
    sem->parent.parent.name[RT_NAME_MAX - 1] = '\0';
#else
    sem->parent.parent.name = name;
#endif /* RT_NAME_MAX > 0 */
    return sem;
}

rt_err_t rt_sem_delete(rt_sem_t sem)
{
    if (sem == RT_NULL) {
        return -RT_ERROR;
    }

    if (sem->parent.parent.type & RT_Object_Class_Static) {
        return -RT_ERROR;
    }

    UINT8 semId = find_sem_adapter(sem);
    if (semId == MAX_SEMS) {
        LOS_MemFree(OS_SYS_MEM_ADDR, sem);
        return -RT_ERROR;
    }

    UINT32 ret = LOS_SemDelete(semId);
    if (ret != LOS_OK) {
        LOS_MemFree(OS_SYS_MEM_ADDR, sem);
        return -RT_ERROR;
    }
 
    ret = LOS_MemFree(OS_SYS_MEM_ADDR, sem);
    if (ret != LOS_OK) {
        return -RT_ERROR;
    }
    sem = RT_NULL;
    sem_map[semId] = 0;
    return RT_EOK;
}

rt_err_t rt_sem_take(rt_sem_t sem, rt_int32_t time)
{
    if (sem == RT_NULL) {
        return -RT_ERROR;
    }
    UINT8 semId = find_sem_adapter(sem);
    if (semId == MAX_SEMS) {
        return -RT_ERROR;
    }

    UINT32 ret = LOS_SemPend(semId, time);
    if (ret == LOS_OK && sem->value > 0) {
        sem->value--;
    }
    return los_to_rt_errno(ret);
}

rt_err_t rt_sem_trytake(rt_sem_t sem)
{
    if (sem == RT_NULL) {
        return -RT_ERROR;
    }
    UINT8 semId = find_sem_adapter(sem);
    if (semId == MAX_SEMS) {
        return -RT_ERROR;
    }

    UINT32 ret = LOS_SemPend(semId, 0);
    if (ret == LOS_OK && sem->value > 0) {
        sem->value--;
    }
    return los_to_rt_errno(ret);
}

rt_err_t rt_sem_release(rt_sem_t sem)
{
    if (sem == RT_NULL) {
        return -RT_ERROR;
    }
    UINT8 semId = find_sem_adapter(sem);
    if (semId == MAX_SEMS) {
        return -RT_ERROR;
    }

    UINT32 ret = LOS_SemPost(semId);
    if (ret == LOS_OK && sem->value < sem->max_value) {
        sem->value++;
    }
    return los_to_rt_errno(ret);
}
