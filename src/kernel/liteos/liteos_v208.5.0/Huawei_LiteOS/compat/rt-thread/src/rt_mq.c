/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2025. All rights reserved.
 * Description : LiteOS adapt rtthread mq.
 * Author : Huawei LiteOS Team
 * Create : 2025-08-07
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
#include "los_queue_debug_pri.h"

#define MAX_MQS LOSCFG_BASE_IPC_QUEUE_LIMIT

static VOID *mq_map[MAX_MQS] = {0};

static UINT32 find_mq_adapter(rt_mq_t mq)
{
    for (UINT32 i = 0; i < MAX_MQS; i++) {
        if (mq_map[i] == (VOID *)mq)
            return i;
    }
    return MAX_MQS;
}

static rt_err_t create_mq_adapter(rt_mq_t mq, UINT32 mq_id)
{
    if (mq_id >= MAX_MQS) {
        return -RT_ERROR;
    }
    mq_map[mq_id] = (VOID *)mq;
    return RT_EOK;
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
rt_err_t rt_mq_init(rt_mq_t     mq,
                    const char *name,
                    void       *msgpool,
                    rt_size_t   msg_size,
                    rt_size_t   pool_size,
                    rt_uint8_t  flag)
{
    UINT32 mqId;
    UINT16 max_msgs;
    UINT32 ret;
    rt_size_t msg_align_size;
    size_t rt_msg_total_size, liteos_msg_total_size;

    if (mq == RT_NULL) {
        return -RT_ERROR;
    }
    if ((flag != RT_IPC_FLAG_FIFO) && (flag != RT_IPC_FLAG_PRIO)) {
        return -RT_ERROR;
    }
    if (flag == RT_IPC_FLAG_PRIO) {
        return -RT_ERROR; // LiteOS不支持PRIO
    }

    msg_align_size = RT_ALIGN(msg_size, RT_ALIGN_SIZE);
    
    rt_msg_total_size = msg_align_size + sizeof(struct rt_mq_message);
    liteos_msg_total_size = msg_align_size + sizeof(QueueMsgHead);
    
    max_msgs = pool_size / (rt_msg_total_size > liteos_msg_total_size ?
                           rt_msg_total_size : liteos_msg_total_size);
    
    if (max_msgs == 0) {
        return -RT_EINVAL;
    }

    ret = LOS_QueueCreateStatic(name, max_msgs, &mqId, (UINT32)flag, msg_align_size, msgpool, pool_size);
    if (ret != LOS_OK) {
        return -RT_ERROR;
    }

    ret = create_mq_adapter(mq, mqId);
    if (ret != RT_EOK) {
        return -RT_ERROR;
    }
    mq->parent.parent.type = RT_Object_Class_MessageQueue | RT_Object_Class_Static;
    mq->parent.parent.flag = flag;
#if RT_NAME_MAX > 0
    rt_strncpy(mq->parent.parent.name, name, RT_NAME_MAX - 1);  /* copy name */
    mq->parent.parent.name[RT_NAME_MAX - 1] = '\0';
#else
    mq->parent.parent.name = name;
#endif /* RT_NAME_MAX > 0 */
    return RT_EOK;
}
#endif

rt_err_t rt_mq_detach(rt_mq_t mq)
{
    if (mq == RT_NULL) {
        return -RT_ERROR;
    }

    if (!(mq->parent.parent.type & RT_Object_Class_Static)) {
        return -RT_ERROR;
    }

    UINT32 mqId = find_mq_adapter(mq);
    if (mqId == MAX_MQS) {
        return -RT_ERROR;
    }
    UINT32 ret = LOS_QueueDelete(mqId);
    if (ret != LOS_OK) {
        return -RT_ERROR;
    }

    mq_map[mqId] = 0;
    mq->parent.parent.type = 0;
    return RT_EOK;
}

rt_mq_t rt_mq_create(const char *name,
                     rt_size_t   msg_size,
                     rt_size_t   max_msgs,
                     rt_uint8_t  flag)
{
    UINT32 mqId;
    UINT32 ret;
    rt_size_t msg_align_size;

    if (flag != RT_IPC_FLAG_FIFO) {
        return RT_NULL;
    }

    msg_align_size = RT_ALIGN(msg_size, RT_ALIGN_SIZE);
       
    if (max_msgs == 0) {
        return RT_NULL;
    }

    struct rt_messagequeue *mq = (struct rt_messagequeue *)LOS_MemAlloc(OS_SYS_MEM_ADDR, (msg_align_size + \
                                                                        sizeof(struct rt_mq_message)) * max_msgs);
    
    if (mq == NULL) {
        return RT_NULL;
    }
    rt_memset(mq, 0, (msg_align_size + sizeof(struct rt_mq_message)) * max_msgs);
    ret = LOS_QueueCreate(name, max_msgs, &mqId, flag, msg_align_size);
    if (ret != LOS_OK) {
        LOS_MemFree(OS_SYS_MEM_ADDR, mq);
        return RT_NULL;
    }

    ret = create_mq_adapter(mq, mqId);
    if (ret != RT_EOK) {
        LOS_MemFree(OS_SYS_MEM_ADDR, mq);
        return RT_NULL;
    }

    mq->parent.parent.type = RT_Object_Class_Semaphore;
    mq->parent.parent.flag = flag;
#if RT_NAME_MAX > 0
    rt_strncpy(mq->parent.parent.name, name, RT_NAME_MAX - 1);  /* copy name */
    mq->parent.parent.name[RT_NAME_MAX - 1] = '\0';
#else
    mq->parent.parent.name = name;
#endif /* RT_NAME_MAX > 0 */
    return mq;
}

rt_err_t rt_mq_delete(rt_mq_t mq)
{
    if (mq == RT_NULL) {
        return -RT_ERROR;
    }

    if (mq->parent.parent.type & RT_Object_Class_Static) {
        return -RT_ERROR;
    }

    UINT32 mqId = find_mq_adapter(mq);
    if (mqId == MAX_MQS) {
        return -RT_ERROR;
    }
    UINT32 ret = LOS_QueueDelete(mqId);
    if (ret != LOS_OK) {
        /* 队列删除失败，不释放内存，保持系统状态不变 */
        return -RT_ERROR;
    }
    /* 队列删除成功后才释放内存 */
    ret = LOS_MemFree(OS_SYS_MEM_ADDR, mq);
    if (ret != LOS_OK) {
        /* 内存释放失败是严重错误，但已无法回滚队列删除操作 */
        return -RT_ERROR;
    }

    mq_map[mqId] = 0;
    return RT_EOK;
}

rt_err_t rt_mq_send_wait(rt_mq_t     mq,
                         const void *buffer,
                         rt_size_t   size,
                         rt_int32_t  timeout)
{
    UINT32 ret;
    UINT32 mqId = find_mq_adapter(mq);
    if (mqId == MAX_MQS) {
        return -RT_ERROR;
    }
    if (mq == RT_NULL || buffer == RT_NULL || size == 0) {
        return -RT_ERROR;
    }

    ret = LOS_QueueWriteCopy(mqId, (VOID *)buffer, size, timeout);

    return los_to_rt_errno(ret);
}

rt_err_t rt_mq_send(rt_mq_t mq, const void *buffer, rt_size_t size)
{
    return rt_mq_send_wait(mq, buffer, size, 0);
}

rt_err_t rt_mq_urgent(rt_mq_t mq, const void *buffer, rt_size_t size)
{
    if (mq == RT_NULL || buffer == RT_NULL || size == 0) {
        return -RT_ERROR;
    }

    UINT32 mqId = find_mq_adapter(mq);
    if (mqId == MAX_MQS) {
        return -RT_ERROR;
    }
    UINT32 ret = LOS_QueueWriteHeadCopy(mqId, (VOID *)buffer, size, 0);
    return los_to_rt_errno(ret);
}

rt_ssize_t rt_mq_recv(rt_mq_t mq, void *buffer, rt_size_t  size, rt_int32_t timeout)
{
    if (mq == RT_NULL || buffer == RT_NULL || size == 0) {
        return -RT_ERROR;
    }

    UINT32 mqId = find_mq_adapter(mq);
    if (mqId == MAX_MQS) {
        return -RT_ERROR;
    }
    UINT32 bufferSize = size;
    UINT32 ret = LOS_QueueReadCopy(mqId, buffer, &bufferSize, timeout);
    if (ret == LOS_OK) {
        return size;
    }
    return los_to_rt_errno(ret);
}

