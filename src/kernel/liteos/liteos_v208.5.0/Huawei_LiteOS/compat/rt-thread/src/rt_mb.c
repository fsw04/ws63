/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2025. All rights reserved.
 * Description : LiteOS adapt rtthread mailbox.
 * Author : Huawei LiteOS Team
 * Create : 2025-8-23
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

#include "los_queue.h"
#include "los_queue_pri.h"
#include "los_memory.h"
#define MAX_MBS LOSCFG_BASE_IPC_QUEUE_LIMIT
#define MB_MSG_MAX_LEN 4

static VOID *mb_map[MAX_MBS] = {0};

static UINT8 find_mb_adapter(rt_mailbox_t mb)
{
    for (UINT8 i = 0; i < MAX_MBS; i++) {
        if (mb_map[i] == (VOID *)mb)
            return i;
    }
    return MAX_MBS;
}

static VOID create_mb_adapter(rt_mailbox_t mb, UINT32 mb_id)
{
    mb_map[mb_id] = (VOID *)mb;
}

static UINT32 los_to_rt_errno(UINT32 los_err)
{
    if (los_err == LOS_OK) {
        return RT_EOK;
    }
    switch (los_err) {
        case LOS_ERRNO_QUEUE_ISFULL:
            return -RT_EFULL;
        case LOS_ERRNO_QUEUE_TIMEOUT:
            return -RT_ETIMEOUT;
        case LOS_ERRNO_QUEUE_ISEMPTY:
            return -RT_EEMPTY;
        default:
            return -RT_ERROR;
    }
}

#ifdef LOSCFG_QUEUE_STATIC_ALLOCATION
rt_err_t rt_mb_init(rt_mailbox_t mb, const char *name, void *msgpool, rt_size_t size, rt_uint8_t flag)
{
    UINT32 mbId;
    UINT32 ret;

    if (mb == RT_NULL) {
        return -RT_ERROR;
    }

    if ((flag != RT_IPC_FLAG_FIFO) && (flag != RT_IPC_FLAG_PRIO)) {
        return -RT_ERROR;
    }

    UINT16 mem_size = size * (MB_MSG_MAX_LEN + (UINT32)sizeof(QueueMsgHead));
    ret = LOS_QueueCreateStatic(name, size, &mbId, (UINT32)flag, MB_MSG_MAX_LEN, msgpool, mem_size);
    if (ret != LOS_OK) {
        return -RT_ERROR;
    }

    create_mb_adapter(mb, mbId);
    mb->parent.parent.type = RT_Object_Class_MailBox | RT_Object_Class_Static;
    mb->parent.parent.flag = flag;
#if RT_NAME_MAX > 0
    rt_strncpy(mb->parent.parent.name, name, RT_NAME_MAX - 1);  /* copy name */
    mb->parent.parent.name[RT_NAME_MAX - 1] = '\0';
#else
    mb->parent.parent.name = name;
#endif /* RT_NAME_MAX > 0 */
    return RT_EOK;
}
#endif

rt_err_t rt_mb_detach(rt_mailbox_t mb)
{
    if (mb == RT_NULL) {
        return -RT_ERROR;
    }

    if (!(mb->parent.parent.type & RT_Object_Class_Static)) {
        return -RT_ERROR;
    }

    UINT8 mbId = find_mb_adapter(mb);
    if (mbId == MAX_MBS) {
        return -RT_ERROR;
    }
    UINT32 ret = LOS_QueueDelete(mbId);
    if (ret != LOS_OK) {
        return -RT_ERROR;
    }

    mb_map[mbId] = 0;
    return RT_EOK;
}

rt_mailbox_t rt_mb_create(const char *name, rt_size_t size, rt_uint8_t flag)
{
    UINT32 mbId;
    UINT32 ret;
    struct rt_mailbox *mb = (struct rt_mailbox *)LOS_MemAlloc(OS_SYS_MEM_ADDR, sizeof(struct rt_mailbox));
    if (mb == NULL) {
        return RT_NULL;
    }
    rt_memset(mb, 0, sizeof(struct rt_mailbox));
    ret = LOS_QueueCreate(name, size, &mbId, flag, MB_MSG_MAX_LEN);
    if (ret != LOS_OK) {
        LOS_MemFree(OS_SYS_MEM_ADDR, mb);
        return RT_NULL;
    }
    create_mb_adapter(mb, mbId);
    mb->parent.parent.type = RT_Object_Class_MailBox;
    mb->parent.parent.flag = flag;
#if RT_NAME_MAX > 0
    rt_strncpy(mb->parent.parent.name, name, RT_NAME_MAX - 1);  /* copy name */
    mb->parent.parent.name[RT_NAME_MAX - 1] = '\0';
#else
    mb->parent.parent.name = name;
#endif /* RT_NAME_MAX > 0 */
    return mb;
}

rt_err_t rt_mb_delete(rt_mailbox_t mb)
{
    if (mb == RT_NULL) {
        return -RT_ERROR;
    }

    if (mb->parent.parent.type & RT_Object_Class_Static) {
        return -RT_ERROR;
    }

    UINT8 mbId = find_mb_adapter(mb);
    if (mbId == MAX_MBS) {
        return -RT_ERROR;
    }

    UINT32 ret = LOS_QueueDelete(mbId);
    if (ret != LOS_OK) {
        return -RT_ERROR;
    }

    ret = LOS_MemFree(OS_SYS_MEM_ADDR, mb);
    if (ret != LOS_OK) {
        return -RT_ERROR;
    }

    mb_map[mbId] = 0;
    return RT_EOK;
}

rt_err_t rt_mb_send_wait(rt_mailbox_t mb, rt_ubase_t value, rt_int32_t timeout)
{
    if (mb == RT_NULL) {
        return -RT_ERROR;
    }

    UINT8 mbId = find_mb_adapter(mb);
    if (mbId == MAX_MBS) {
        return -RT_ERROR;
    }

    UINT32 ret = LOS_QueueWriteCopy(mbId, &value, 4, timeout);
    return los_to_rt_errno(ret);
}

rt_err_t rt_mb_send(rt_mailbox_t mb, rt_ubase_t value)
{
    return rt_mb_send_wait(mb, value, 0);
}

rt_err_t rt_mb_urgent(rt_mailbox_t mb, rt_ubase_t value)
{
    if (mb == RT_NULL) {
        return -RT_ERROR;
    }

    UINT8 mbId = find_mb_adapter(mb);
    if (mbId == MAX_MBS) {
        return -RT_ERROR;
    }

    UINT32 ret = LOS_QueueWriteHeadCopy(mbId, &value, 4, 0);
    return los_to_rt_errno(ret);
}

rt_err_t rt_mb_recv(rt_mailbox_t mb, rt_ubase_t *value, rt_int32_t timeout)
{
    UINT8 mbId = find_mb_adapter(mb);
    UINT32 bufferSize = MB_MSG_MAX_LEN;
    UINT32 ret = LOS_QueueReadCopy(mbId, (VOID *)value, &bufferSize, timeout);
    if (ret == LOS_OK) {
        return RT_EOK;
    }
    return los_to_rt_errno(ret);
}
