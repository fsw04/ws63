/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2025. All rights reserved.
 * Description : LiteOS adapt rtthread mutex.
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

#include "rtthread.h"
#include "rtdef.h"
#include "los_mux.h"
#include "los_mux_pri.h"
#include "los_mux_debug_pri.h"

#define MAX_MUXS LOSCFG_BASE_IPC_MUX_LIMIT

static VOID *mutex_map[MAX_MUXS] = {0};

static UINT8 find_mutex_adapter(rt_mutex_t mutex)
{
    for (UINT8 i = 0; i < MAX_MUXS; i++) {
        if (mutex_map[i] == (VOID *)mutex)
            return i;
    }
    return MAX_MUXS;
}

static rt_err_t create_mutex_adapter(rt_mutex_t mutex)
{
    UINT32 mutex_id;
    UINT32 ret =  LOS_MuxCreate(&mutex_id);
    if (ret != LOS_OK) {
        return -RT_ERROR;
    }
 
    mutex_map[mutex_id] = (VOID *)mutex;
    return RT_EOK;
}

static UINT32 los_to_rt_errno(UINT32 los_err)
{
    if (los_err == LOS_OK) {
        return RT_EOK;
    }
    switch (los_err) {
        case LOS_ERRNO_MUX_UNAVAILABLE:
        case LOS_ERRNO_MUX_TIMEOUT:
            return -RT_ETIMEOUT;

        default:
            return -RT_ERROR;
    }
}

rt_err_t rt_mutex_init(rt_mutex_t mutex, const char *name, rt_uint8_t flag)
{
    if (mutex == RT_NULL) {
        return -RT_ERROR;
    }
 
    // 修改create_mutex_adapter返回错误码
    rt_err_t ret = create_mutex_adapter(mutex);
    if (ret != RT_EOK) {
        return ret;
    }

    mutex->parent.parent.type = RT_Object_Class_Mutex | RT_Object_Class_Static;
    mutex->parent.parent.flag = flag;
#if RT_NAME_MAX > 0
    rt_strncpy(mutex->parent.parent.name, name, RT_NAME_MAX - 1);  /* copy name */
    mutex->parent.parent.name[RT_NAME_MAX - 1] = '\0';
#else
    mutex->parent.parent.name = name;
#endif /* RT_NAME_MAX > 0 */
    return RT_EOK;
}

rt_err_t rt_mutex_detach(rt_mutex_t mutex)
{
    if (mutex == RT_NULL) {
        return -RT_ERROR;
    }
    if (!(mutex->parent.parent.type & RT_Object_Class_Static)) {
        return -RT_ERROR;
    }

    UINT8 mutexId = find_mutex_adapter(mutex);
    if (mutexId == MAX_MUXS) {
        return -RT_ERROR;
    }
    UINT32 ret = LOS_MuxDelete(mutexId);
    if (ret != LOS_OK) {
        return -RT_ERROR;
    }

    mutex_map[mutexId] = 0;
    mutex->parent.parent.type = 0;
    return RT_EOK;
}

rt_mutex_t rt_mutex_create(const char *name, rt_uint8_t flag)
{
    if (name == NULL) {
        return RT_NULL;
    }

    struct rt_mutex *mutex = (struct rt_mutex *)LOS_MemAlloc(OS_SYS_MEM_ADDR, sizeof(struct rt_mutex));
    if (mutex == NULL) {
        return RT_NULL;
    }
    rt_memset(mutex, 0, sizeof(struct rt_mutex));
    // 修改create_mutex_adapter返回错误码
    rt_err_t ret = create_mutex_adapter(mutex);
    if (ret != RT_EOK) {
        LOS_MemFree(OS_SYS_MEM_ADDR, mutex);
        return RT_NULL;
    }
    mutex->parent.parent.type = RT_Object_Class_Mutex;
    mutex->parent.parent.flag = flag;
#if RT_NAME_MAX > 0
    rt_strncpy(mutex->parent.parent.name, name, RT_NAME_MAX - 1);  /* copy name */
    mutex->parent.parent.name[RT_NAME_MAX - 1] = '\0';
#else
    mutex->parent.parent.name = name;
#endif /* RT_NAME_MAX > 0 */
    return mutex;
}

rt_err_t rt_mutex_delete(rt_mutex_t mutex)
{
    if (mutex == RT_NULL) {
        return -RT_ERROR;
    }
    if (mutex->parent.parent.type & RT_Object_Class_Static) {
        return -RT_ERROR;
    }

    UINT8 mutexId = find_mutex_adapter(mutex);
    if (mutexId == MAX_MUXS) {
        LOS_MemFree(OS_SYS_MEM_ADDR, mutex);
        return -RT_ERROR;
    }

    UINT32 ret = LOS_MuxDelete(mutexId);
    if (ret != LOS_OK) {
        LOS_MemFree(OS_SYS_MEM_ADDR, mutex);
        return -RT_ERROR;
    }
 
    ret = LOS_MemFree(OS_SYS_MEM_ADDR, mutex);
    if (ret != LOS_OK) {
        return -RT_ERROR;
    }

    mutex_map[mutexId] = 0;
    return RT_EOK;
}

rt_err_t rt_mutex_take(rt_mutex_t mutex, rt_int32_t time)
{
    if (mutex == RT_NULL) {
        return -RT_ERROR;
    }
    UINT8 mutexId = find_mutex_adapter(mutex);
    if (mutexId == MAX_MUXS) {
        return -RT_ERROR;
    }

    UINT32 ret = LOS_MuxPend(mutexId, time);
    return los_to_rt_errno(ret);
}

rt_err_t rt_mutex_trytake(rt_mutex_t mutex)
{
    if (mutex == RT_NULL) {
        return -RT_ERROR;
    }
    UINT8 mutexId = find_mutex_adapter(mutex);
    if (mutexId == MAX_MUXS) {
        return -RT_ERROR;
    }

    UINT32 ret = LOS_MuxPend(mutexId, 0);
    return los_to_rt_errno(ret);
}

rt_err_t rt_mutex_release(rt_mutex_t mutex)
{
    if (mutex == RT_NULL) {
        return -RT_ERROR;
    }
    UINT8 mutexId = find_mutex_adapter(mutex);
    if (mutexId == MAX_MUXS) {
        return -RT_ERROR;
    }

    UINT32 ret = LOS_MuxPost(mutexId);
    return los_to_rt_errno(ret);
}
