/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 */
/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2026. All rights
 * reserved. Description : LiteOS adapt FreeRTOS. Author : Huawei LiteOS Team
 * Create : 2026-1-13
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
 * HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "los_memory.h"
#include "los_sem.h"
#include "los_mux.h"
#include "los_queue.h"
#include "los_queue_pri.h"
#include "los_typedef.h"

#if ( ( configUSE_QUEUE_SETS == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )
#include "los_sem_pri.h"
#include "los_mux_pri.h"
#endif

#define FR_NAME_MAX 20
#define INVALID_VALUE 0xFFFFFFFF

typedef struct QueuePointers {
    int8_t *pcTail;
    int8_t *pcReadFrom;
} QueuePointers_t;

typedef struct SemaphoreData {
    TaskHandle_t xMutexHolder;
    UBaseType_t uxRecursiveCallCount;
} SemaphoreData_t;

typedef struct QueueDefinition {
    int8_t *pcHead;     /**< Points to the beginning of the queue storage area. */
    union {
        UINT32 queueId;
        UINT32 semId;
        UINT32 muxId;
    } Id;
    UINT32 uxItemSize;
    uint8_t *pucQueueStorage;
    uint8_t ucQueueType;
#if ((configSUPPORT_STATIC_ALLOCATION == 1) && (configSUPPORT_DYNAMIC_ALLOCATION == 1))
    uint8_t ucStaticallyAllocated;
#endif
    union {
        QueuePointers_t xQueue;     /**< Data required exclusively when this structure is used as a queue. */
        SemaphoreData_t xSemaphore; /**< Data required exclusively when this structure is used as a semaphore. */
    } u;
    volatile UBaseType_t uxMessagesWaiting; /**< The number of items currently in the queue. */

#if ( ( configUSE_QUEUE_SETS == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )
    UINT32 semInitCount;
    struct QueueDefinition *pxQueueSetContainer;
#endif
} xQUEUE;
typedef xQUEUE Queue_t;

#if ( ( configUSE_QUEUE_SETS == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )
static UINT32 prvQueueReadable(Queue_t *pxQueue);
#endif

static UINT32 los_err_to_freertos(UINT32 los_err)
{
    if (los_err == LOS_OK) {
        return pdPASS;
    }
    switch (los_err) {
        case LOS_ERRNO_QUEUE_ISFULL:
            return errQUEUE_FULL;
        case LOS_ERRNO_QUEUE_TIMEOUT:
            return pdFAIL;
        case LOS_ERRNO_QUEUE_ISEMPTY:
            return pdFAIL;
        default:
            return pdFAIL;
    }
}

#if (configSUPPORT_STATIC_ALLOCATION == 1)
#ifdef LOSCFG_QUEUE_STATIC_ALLOCATION
static inline BOOL prvCreateParaCheck(const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize,
    uint8_t *pucQueueStorage, StaticQueue_t *pxStaticQueue)
{
    if (pxStaticQueue == NULL) {
        return false;
    }

    if ((pucQueueStorage == NULL) != (uxItemSize == 0U)) {
        return false;
    }

    return true;
}
#endif

QueueHandle_t xQueueGenericCreateStatic(const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize,
    uint8_t *pucQueueStorage, StaticQueue_t *pxStaticQueue, const uint8_t ucQueueType)
{
    Queue_t *pxNewQueue = NULL;
    UINT32 count = uxQueueLength;
    UINT32 ret = LOS_OK;

    if (!prvCreateParaCheck(uxQueueLength, uxItemSize, pucQueueStorage, pxStaticQueue)) {
        return NULL;
    }
    pxNewQueue = (Queue_t *)LOS_MemAlloc(OS_SYS_MEM_ADDR, sizeof(Queue_t));
    if (pxNewQueue == NULL) {
        return NULL;
    }

    if (ucQueueType == queueQUEUE_TYPE_BASE) {
#ifdef LOSCFG_QUEUE_STATIC_ALLOCATION
        char name[FR_NAME_MAX] = {0};
        ret = LOS_QueueCreateStatic(name, uxQueueLength, &pxNewQueue->Id.queueId, 0,
                                    uxItemSize, pucQueueStorage,
                                    (uxItemSize + (UINT32)sizeof(QueueMsgHead)) * uxQueueLength);
#else
        LOS_MemFree(OS_SYS_MEM_ADDR, pxNewQueue);
        return NULL;
#endif
    } else if ((ucQueueType == queueQUEUE_TYPE_RECURSIVE_MUTEX) ||
               (ucQueueType == queueQUEUE_TYPE_MUTEX)) {
        ret = LOS_MuxCreate(&pxNewQueue->Id.muxId);
    } else if (ucQueueType == queueQUEUE_TYPE_COUNTING_SEMAPHORE) {
        ret = LOS_SemCreate(count, &pxNewQueue->Id.semId);
    } else if (ucQueueType == queueQUEUE_TYPE_BINARY_SEMAPHORE) {
        ret = LOS_BinarySemCreate(count, &pxNewQueue->Id.semId);
    } else {
        LOS_MemFree(OS_SYS_MEM_ADDR, pxNewQueue);
        return NULL;
    }
    if (ret != LOS_OK) {
        LOS_MemFree(OS_SYS_MEM_ADDR, pxNewQueue);
        return NULL;
    }
    pxNewQueue->uxItemSize = (UINT32)uxItemSize;
    pxNewQueue->pucQueueStorage = pucQueueStorage;
    pxNewQueue->ucQueueType = ucQueueType;

#if ( ( configUSE_QUEUE_SETS == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )
    pxNewQueue->semInitCount = count;
    pxNewQueue->pxQueueSetContainer = NULL;
#endif
#if (configSUPPORT_DYNAMIC_ALLOCATION == 1)
    pxNewQueue->ucStaticallyAllocated = pdTRUE;
#endif
    return pxNewQueue;
}
#endif

#if (configSUPPORT_DYNAMIC_ALLOCATION == 1)
QueueHandle_t xQueueGenericCreate(
    const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize, const uint8_t ucQueueType)
{
    Queue_t *pxNewQueue = NULL;
    UINT32 ret = LOS_OK;
    UINT32 count = uxQueueLength;
    char name[FR_NAME_MAX] = {0};

    if ((SIZE_MAX / uxQueueLength) < uxItemSize) {
        return NULL;
    }

    if ((UBaseType_t)(SIZE_MAX - sizeof(Queue_t)) < (uxQueueLength * uxItemSize)) {
        return NULL;
    }

    pxNewQueue = (Queue_t *)LOS_MemAlloc(OS_SYS_MEM_ADDR, sizeof(Queue_t));
    if (pxNewQueue == NULL) {
        return NULL;
    }

    if (ucQueueType == queueQUEUE_TYPE_BASE) {
        ret = LOS_QueueCreate(name, uxQueueLength, &pxNewQueue->Id.queueId, 0, uxItemSize);
    } else if (ucQueueType == queueQUEUE_TYPE_RECURSIVE_MUTEX || ucQueueType == queueQUEUE_TYPE_MUTEX) {
        ret = LOS_MuxCreate(&pxNewQueue->Id.muxId);
    } else if (ucQueueType == queueQUEUE_TYPE_COUNTING_SEMAPHORE) {
        ret = LOS_SemCreate(count, &pxNewQueue->Id.semId);
    } else if (ucQueueType == queueQUEUE_TYPE_BINARY_SEMAPHORE) {
        ret = LOS_BinarySemCreate(count, &pxNewQueue->Id.semId);
    } else {
        return pxNewQueue;
    }
    if (ret != LOS_OK) {
        LOS_MemFree(OS_SYS_MEM_ADDR, pxNewQueue);
        return NULL;
    }
    pxNewQueue->uxItemSize = (UINT32)uxItemSize;
    pxNewQueue->ucQueueType = ucQueueType;

#if ( ( configUSE_QUEUE_SETS == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )
    pxNewQueue->semInitCount = count;
    pxNewQueue->pxQueueSetContainer = NULL;
#endif

    return (QueueHandle_t)pxNewQueue;
}
#endif

void vQueueDelete(QueueHandle_t xQueue)
{
    Queue_t *const pxQueue = xQueue;
    UINT32 ret = LOS_OK;

    if (pxQueue->ucQueueType == queueQUEUE_TYPE_BASE) {
        ret = LOS_QueueDelete(pxQueue->Id.queueId);
    } else if (pxQueue->ucQueueType == queueQUEUE_TYPE_RECURSIVE_MUTEX ||
        pxQueue->ucQueueType == queueQUEUE_TYPE_MUTEX) {
        ret = LOS_MuxDelete(pxQueue->Id.muxId);
    } else if (pxQueue->ucQueueType == queueQUEUE_TYPE_BINARY_SEMAPHORE ||
        pxQueue->ucQueueType == queueQUEUE_TYPE_COUNTING_SEMAPHORE) {
        ret = LOS_SemDelete(pxQueue->Id.semId);
    } else {
        return;
    }
    if (ret != LOS_OK) {
        return;
    }
    ret = LOS_MemFree(OS_SYS_MEM_ADDR, pxQueue);
    if (ret != LOS_OK) {
        return;
    }
}

static UINT32 xOnlyQueueSend(
    QueueHandle_t pxQueue, const void *const pvItemToQueue, TickType_t xTicksToWait, const BaseType_t xCopyPosition)
{
    UINT32 ret;
    if (pvItemToQueue == NULL || pxQueue->uxItemSize == (UBaseType_t)0U) {
        return LOS_NOK;
    }
    if (xCopyPosition == queueSEND_TO_BACK) {
        ret = LOS_QueueWriteCopy(pxQueue->Id.queueId, (VOID *)pvItemToQueue, pxQueue->uxItemSize, xTicksToWait);
    } else if (xCopyPosition == queueSEND_TO_FRONT) {
        ret = LOS_QueueWriteHeadCopy(pxQueue->Id.queueId, (VOID *)pvItemToQueue, pxQueue->uxItemSize, xTicksToWait);
    } else if (xCopyPosition == queueOVERWRITE) {
        void *pvBuffer;
        pvBuffer = (VOID *)LOS_MemAlloc(OS_SYS_MEM_ADDR, pxQueue->uxItemSize);
        if (pvBuffer == NULL) {
            return LOS_NOK;
        }
        LOS_QueueReadCopy(pxQueue->Id.queueId, pvBuffer, &pxQueue->uxItemSize, 0);
        ret = LOS_QueueWriteHeadCopy(pxQueue->Id.queueId, (VOID *)pvItemToQueue, pxQueue->uxItemSize, xTicksToWait);
    } else {
        return LOS_NOK;
    }
    return ret;
}

BaseType_t xQueueGenericSend(
    QueueHandle_t xQueue, const void *const pvItemToQueue, TickType_t xTicksToWait, const BaseType_t xCopyPosition)
{
    Queue_t *const pxQueue = xQueue;
    UINT32 ret;
#if ( ( configUSE_QUEUE_SETS == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )
    UINT32 dataCountBefore = 0, dataCountAfter = 0;
#endif

    if (pxQueue == NULL) {
        return pdFAIL;
    }

    if (xQueue->ucQueueType == queueQUEUE_TYPE_BASE) {  // 队列
#if ( ( configUSE_QUEUE_SETS == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )
        dataCountBefore = prvQueueReadable(pxQueue);
        ret = xOnlyQueueSend(pxQueue, pvItemToQueue, xTicksToWait, xCopyPosition);
        dataCountAfter = prvQueueReadable(pxQueue);
#else
        ret = xOnlyQueueSend(pxQueue, pvItemToQueue, xTicksToWait, xCopyPosition);
#endif
    } else if (xQueue->ucQueueType == queueQUEUE_TYPE_RECURSIVE_MUTEX ||
        xQueue->ucQueueType == queueQUEUE_TYPE_MUTEX) { // 互斥量
        ret = LOS_MuxPost(pxQueue->Id.muxId);
    } else if (xQueue->ucQueueType == queueQUEUE_TYPE_BINARY_SEMAPHORE ||
        xQueue->ucQueueType == queueQUEUE_TYPE_COUNTING_SEMAPHORE) {    // 信号量
        ret = LOS_SemPost(pxQueue->Id.semId);
    } else {
        return pdFAIL;
    }

#if ( ( configUSE_QUEUE_SETS == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )
    if (ret != LOS_OK) {    // send操作成功之后才通知队列集
        return los_err_to_freertos(ret);
    }
    if (xQueue->pxQueueSetContainer == NULL) {
        return los_err_to_freertos(ret);
    }
    if (xCopyPosition == queueOVERWRITE && dataCountBefore == dataCountAfter) {
        return los_err_to_freertos(ret);
    }

    // 当前句柄在队列集中，所以需要同时将句柄加入队列集队列
    Queue_t* pxQueueSet = xQueue->pxQueueSetContainer;
    VOID *temp = xQueue;
    ret = LOS_QueueWriteCopy(pxQueueSet->Id.queueId, (VOID *)(&temp), pxQueueSet->uxItemSize, xTicksToWait);
#endif

    return los_err_to_freertos(ret);
}

BaseType_t xQueueReceive(QueueHandle_t xQueue, void *const pvBuffer, TickType_t xTicksToWait)
{
    Queue_t *const pxQueue = xQueue;
    UINT32 ret;

    if (pxQueue == NULL) {
        return pdFAIL;
    }

    if (xQueue->ucQueueType == queueQUEUE_TYPE_BASE) {
        ret = LOS_QueueReadCopy(pxQueue->Id.queueId, pvBuffer, &pxQueue->uxItemSize, xTicksToWait);
    } else if (xQueue->ucQueueType == queueQUEUE_TYPE_RECURSIVE_MUTEX ||
        xQueue->ucQueueType == queueQUEUE_TYPE_MUTEX) {
        ret = LOS_MuxPend(pxQueue->Id.muxId, (UINT32)xTicksToWait);
    } else if (pxQueue->ucQueueType == queueQUEUE_TYPE_BINARY_SEMAPHORE || \
                pxQueue->ucQueueType == queueQUEUE_TYPE_COUNTING_SEMAPHORE) {
        ret = LOS_SemPend(pxQueue->Id.semId, (UINT32)xTicksToWait);
    } else {
        return pdFAIL;
    }
    return los_err_to_freertos(ret);
}

UBaseType_t uxQueueSpacesAvailable(const QueueHandle_t xQueue)
{
    UBaseType_t uxReturn = INVALID_VALUE;
    Queue_t *const pxQueue = xQueue;
    QUEUE_INFO_S queueInfo;

    if (pxQueue == NULL) {
        return pdFAIL;
    }
    
    UINT32 ret = LOS_QueueInfoGet(pxQueue->Id.queueId, &queueInfo);
    if (ret == LOS_OK) {
        uxReturn = queueInfo.usWritableCnt;
    }
    return uxReturn;
}

UBaseType_t uxQueueMessagesWaiting(const QueueHandle_t xQueue)
{
    UBaseType_t uxReturn = INVALID_VALUE;
    Queue_t *const pxQueue = xQueue;
    QUEUE_INFO_S queueInfo;

    if (pxQueue == NULL) {
        return pdFAIL;
    }

    if (xQueue->ucQueueType == queueQUEUE_TYPE_BASE) {
        UINT32 ret = LOS_QueueInfoGet(pxQueue->Id.queueId, &queueInfo);
        if (ret == LOS_OK) {
            uxReturn = queueInfo.usReadableCnt;
        }
    } else if (xQueue->ucQueueType == queueQUEUE_TYPE_RECURSIVE_MUTEX ||
        xQueue->ucQueueType == queueQUEUE_TYPE_MUTEX) {
        LosMuxCB *muxInfo = GET_MUX(pxQueue->Id.muxId);
        uxReturn = muxInfo->muxCount;
    } else if (xQueue->ucQueueType == queueQUEUE_TYPE_BINARY_SEMAPHORE ||
        xQueue->ucQueueType == queueQUEUE_TYPE_COUNTING_SEMAPHORE) {
        LosSemCB *semInfo = GET_SEM(pxQueue->Id.semId);
        uxReturn = semInfo->semCount;
    } else {
        return pdFAIL;
    }
    return uxReturn;
}

BaseType_t xQueueGenericSendFromISR(QueueHandle_t xQueue, const void *const pvItemToQueue,
    BaseType_t *const pxHigherPriorityTaskWoken, const BaseType_t xCopyPosition)
{
    return xQueueGenericSend(xQueue, pvItemToQueue, 0, xCopyPosition);
}

BaseType_t xQueueReceiveFromISR(QueueHandle_t xQueue, void *const pvBuffer, BaseType_t *const pxHigherPriorityTaskWoken)
{
    if (pxHigherPriorityTaskWoken != NULL) {
        *pxHigherPriorityTaskWoken = pdFALSE;
    }

    return xQueueReceive(xQueue, pvBuffer, 0);
}

BaseType_t xQueueGenericReset(QueueHandle_t xQueue, BaseType_t xNewQueue)
{
    Queue_t *const pxQueue = xQueue;
    void *pvBuffer;
    UINT32 ret;
    QUEUE_INFO_S queueInfo;
    UINT16 msgsize;
    int max_iterations = 1000;

    if (pxQueue == NULL) {
        return pdFAIL;
    }

    ret = LOS_QueueInfoGet(pxQueue->Id.queueId, &queueInfo);
    if (ret == LOS_OK) {
        msgsize = queueInfo.usQueueSize;
        pvBuffer = (VOID *)LOS_MemAlloc(OS_SYS_MEM_ADDR, msgsize);
    } else {
        return pdFAIL;
    }

    while ((max_iterations--) > 0) {
        ret = LOS_QueueReadCopy(pxQueue->Id.queueId, pvBuffer, &pxQueue->uxItemSize, 0);
        if (ret == LOS_ERRNO_QUEUE_ISEMPTY) {
            break;
        } else if (ret != LOS_OK) {
            LOS_MemFree(OS_SYS_MEM_ADDR, pvBuffer);
            return pdFAIL;
        }
    }

    LOS_MemFree(OS_SYS_MEM_ADDR, pvBuffer);
    return pdPASS;
}

BaseType_t xQueuePeek(QueueHandle_t xQueue, void *const pvBuffer, TickType_t xTicksToWait)
{
    configASSERT(0);
    return pdFAIL;
}

BaseType_t xQueuePeekFromISR(QueueHandle_t xQueue, void *const pvBuffer)
{
    configASSERT(0);
    return pdFAIL;
}

BaseType_t xQueueIsQueueEmptyFromISR(const QueueHandle_t xQueue)
{
    BaseType_t xReturn;
    if (uxQueueMessagesWaiting(xQueue) == (UBaseType_t)0) {
        xReturn = pdTRUE;
    } else {
        xReturn = pdFALSE;
    }

    return xReturn;
}

BaseType_t xQueueIsQueueFullFromISR(const QueueHandle_t xQueue)
{
    BaseType_t xReturn;

    if (uxQueueSpacesAvailable(xQueue) == (UBaseType_t)0) {
        xReturn = pdTRUE;
    } else {
        xReturn = pdFALSE;
    }
    return xReturn;
}

#if (configSUPPORT_STATIC_ALLOCATION == 1)
BaseType_t xQueueGenericGetStaticBuffers(
    QueueHandle_t xQueue, uint8_t **ppucQueueStorage, StaticQueue_t **ppxStaticQueue)
{
    BaseType_t xReturn = pdFALSE;
    Queue_t *const pxQueue = xQueue;
    if (pxQueue == NULL) {
        return pdFAIL;
    }
    #if (configSUPPORT_DYNAMIC_ALLOCATION == 1)
    {
        if (pxQueue->ucStaticallyAllocated == (uint8_t)pdTRUE) {
            if (ppucQueueStorage != NULL) {
                *ppucQueueStorage = (uint8_t *)pxQueue->pucQueueStorage;
                xReturn = pdTRUE;
            }
        } else {
            xReturn = pdFALSE;
        }
    }
    #else
    {
        if (ppucQueueStorage != NULL) {
            *ppucQueueStorage = (uint8_t *) pxQueue->pcHead;
        }
        *ppxStaticQueue = (StaticQueue_t *) pxQueue;
        xReturn = pdTRUE;
    }
    #endif
    return xReturn;
}
#endif

UBaseType_t uxQueueMessagesWaitingFromISR(const QueueHandle_t xQueue)
{
    return uxQueueMessagesWaiting(xQueue);
}

BaseType_t xQueueGiveFromISR(QueueHandle_t xQueue, BaseType_t *const pxHigherPriorityTaskWoken)
{
    return xQueueGenericSendFromISR(xQueue, NULL, pxHigherPriorityTaskWoken, queueSEND_TO_BACK);
}

#define SEMAPHORE_QUEUE_ITEM_LENGTH    ((UBaseType_t) 0)
#define MUTEX_GIVE_BLOCK_TIME          ((TickType_t) 0U)

/* 创建计数信号量（动态） */
#if (configUSE_COUNTING_SEMAPHORES == 1)
#if (configSUPPORT_DYNAMIC_ALLOCATION == 1)
QueueHandle_t xQueueCreateCountingSemaphore(const UBaseType_t uxMaxCount, const UBaseType_t uxInitialCount)
{
    if ((uxMaxCount == 0U) || (uxInitialCount > uxMaxCount)) {
        return NULL;
    }

    QueueHandle_t xHandle = xQueueGenericCreate(uxInitialCount, SEMAPHORE_QUEUE_ITEM_LENGTH, \
        queueQUEUE_TYPE_COUNTING_SEMAPHORE);
    if (xHandle != NULL) {
        ((Queue_t *) xHandle)->uxMessagesWaiting = uxInitialCount;
    }

    return xHandle;
}
#endif /* (configSUPPORT_DYNAMIC_ALLOCATION == 1) */

#if (configSUPPORT_STATIC_ALLOCATION == 1)
QueueHandle_t xQueueCreateCountingSemaphoreStatic(const UBaseType_t uxMaxCount,
                                                  const UBaseType_t uxInitialCount,
                                                  StaticQueue_t *pxStaticQueue)
{
    if ((uxMaxCount == 0U) || (uxInitialCount > uxMaxCount)) {
        return NULL;
    }
    QueueHandle_t xHandle = xQueueGenericCreateStatic(uxInitialCount, SEMAPHORE_QUEUE_ITEM_LENGTH, NULL, \
        pxStaticQueue, queueQUEUE_TYPE_COUNTING_SEMAPHORE);
    if (xHandle != NULL) {
        ((Queue_t *) xHandle)->uxMessagesWaiting = uxInitialCount;
    }
    return xHandle;
}
#endif /* (configSUPPORT_STATIC_ALLOCATION == 1) */
#endif

#if (configUSE_MUTEXES == 1)
static void InitMutex(Queue_t * pxNewQueue)
{
    if (pxNewQueue == NULL) {
        return;
    }

    pxNewQueue->u.xSemaphore.xMutexHolder = xTaskGetCurrentTaskHandle();
    pxNewQueue->pcHead = NULL;

    /* In case this is a recursive mutex. */
    pxNewQueue->u.xSemaphore.uxRecursiveCallCount = 0;

    /* Start with the semaphore in the expected state. */
    (void)xQueueGenericSend(pxNewQueue, NULL, (TickType_t) 0U, queueSEND_TO_BACK);
}

#if (configSUPPORT_DYNAMIC_ALLOCATION == 1)
QueueHandle_t xQueueCreateMutex(const uint8_t ucQueueType)
{
    const UBaseType_t uxMutexLength = (UBaseType_t) 1;
    const UBaseType_t uxMutexSize = (UBaseType_t) 0;
    QueueHandle_t xNewQueue = xQueueGenericCreate(uxMutexLength, uxMutexSize, ucQueueType);
    InitMutex((Queue_t *) xNewQueue);
    return xNewQueue;
}
#endif
#if (configSUPPORT_STATIC_ALLOCATION == 1)
QueueHandle_t xQueueCreateMutexStatic(const uint8_t ucQueueType, StaticQueue_t *pxStaticQueue)
{
    const UBaseType_t uxMutexLength = (UBaseType_t) 1;
    const UBaseType_t uxMutexSize = (UBaseType_t) 0;
    QueueHandle_t xNewQueue = xQueueGenericCreateStatic(uxMutexLength, uxMutexSize, NULL, pxStaticQueue, ucQueueType);
    InitMutex((Queue_t *) xNewQueue);

    return xNewQueue;
}
#endif

#if (INCLUDE_xSemaphoreGetMutexHolder == 1)
TaskHandle_t xQueueGetMutexHolder(QueueHandle_t xSemaphore)
{
    TaskHandle_t pxReturn;
    Queue_t * const pxSemaphore = (Queue_t *) xSemaphore;
    if (pxSemaphore == NULL) {
        return NULL;
    }

    if (pxSemaphore->ucQueueType == queueQUEUE_TYPE_BASE ||
        pxSemaphore->ucQueueType == queueQUEUE_TYPE_BINARY_SEMAPHORE ||
        pxSemaphore->ucQueueType == queueQUEUE_TYPE_COUNTING_SEMAPHORE) {
        return NULL;
    }

    if (pxSemaphore->pcHead == NULL) {
        pxReturn = pxSemaphore->u.xSemaphore.xMutexHolder;
    } else {
        pxReturn = NULL;
    }

    return pxReturn;
}
#endif /* (INCLUDE_xSemaphoreGetMutexHolder == 1) */
#endif

BaseType_t xQueueSemaphoreTake(QueueHandle_t xQueue, TickType_t xTicksToWait)
{
    Queue_t * const pxQueue = (Queue_t *)xQueue;
    /* Check the queue pointer is not NULL. */
    if (pxQueue == NULL) {
        return pdFAIL;
    }
    UINT32 ret;
    if (pxQueue->ucQueueType == queueQUEUE_TYPE_MUTEX || pxQueue->ucQueueType == queueQUEUE_TYPE_RECURSIVE_MUTEX) {
        ret = LOS_MuxPend(pxQueue->Id.muxId, (UINT32)xTicksToWait);
        return (ret == LOS_OK) ? pdPASS : pdFAIL;
    } else if (pxQueue->ucQueueType == queueQUEUE_TYPE_BINARY_SEMAPHORE || \
                pxQueue->ucQueueType == queueQUEUE_TYPE_COUNTING_SEMAPHORE) {
        ret = LOS_SemPend(pxQueue->Id.semId, (UINT32)xTicksToWait);
        return (ret == LOS_OK) ? pdPASS : pdFAIL;
    }
    return pdPASS;
}

#if (configUSE_RECURSIVE_MUTEXES == 1)
BaseType_t xQueueTakeMutexRecursive(QueueHandle_t xMutex, TickType_t xTicksToWait)
{
    BaseType_t xReturn;
    Queue_t * const pxMutex = (Queue_t *) xMutex;

    if (pxMutex == NULL) {
        return pdFAIL;
    }
    xReturn = xQueueSemaphoreTake(pxMutex, xTicksToWait);
    if (xReturn != pdFAIL) {
        (pxMutex->u.xSemaphore.uxRecursiveCallCount)++;
    }
    return xReturn;
}

BaseType_t xQueueGiveMutexRecursive(QueueHandle_t xMutex)
{
    BaseType_t xReturn;
    Queue_t * const pxMutex = (Queue_t *) xMutex;
    if (pxMutex == NULL) {
        return pdFAIL;
    }

    if (pxMutex->u.xSemaphore.xMutexHolder == xTaskGetCurrentTaskHandle()) {
        (pxMutex->u.xSemaphore.uxRecursiveCallCount)--;
        if (pxMutex->u.xSemaphore.uxRecursiveCallCount == (UBaseType_t) 0) {
            (void)xQueueGenericSend(pxMutex, NULL, MUTEX_GIVE_BLOCK_TIME, queueSEND_TO_BACK);
        }
        xReturn = pdPASS;
    } else {
        xReturn = pdFAIL;
    }

    return xReturn;
}
#endif /* configUSE_RECURSIVE_MUTEXES */

#if ( ( configUSE_QUEUE_SETS == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )
    static UINT32 prvQueueReadable(Queue_t *pxQueue)
    {
        UINT32 queueReadable = 0;
        QUEUE_INFO_S queueInfo;
        (void)LOS_QueueInfoGet(pxQueue->Id.queueId, &queueInfo);
        queueReadable = queueInfo.usReadableCnt;
        return queueReadable;
    }

    static BOOL prvIsQueueEmpty(QueueSetMemberHandle_t xQueueOrSemaphore)
    {
        if (xQueueOrSemaphore == NULL) {
            return false;
        }

        if (xQueueOrSemaphore->ucQueueType == queueQUEUE_TYPE_BASE) {    // 队列
            QUEUE_INFO_S queueInfo;
            UINT32 ret = LOS_QueueInfoGet(xQueueOrSemaphore->Id.queueId, &queueInfo);
            if (ret != LOS_OK) {
                return false;
            }
            if (queueInfo.usReadableCnt != 0) { // 队列中有可读的数据
                return false;
            }
            return true;
        }
        if (xQueueOrSemaphore->ucQueueType == queueQUEUE_TYPE_MUTEX ||
            xQueueOrSemaphore->ucQueueType == queueQUEUE_TYPE_RECURSIVE_MUTEX) { // 互斥量
            LosMuxCB *muxHandle = NULL;
            muxHandle = GET_MUX(xQueueOrSemaphore->Id.muxId);
            if (muxHandle->muxCount != 0 || muxHandle->owner != NULL) {  // 互斥量被其他线程占用
                return false;
            }
            return true;
        }
        if (xQueueOrSemaphore->ucQueueType == queueQUEUE_TYPE_COUNTING_SEMAPHORE ||
            xQueueOrSemaphore->ucQueueType == queueQUEUE_TYPE_BINARY_SEMAPHORE) {    // 信号量
            LosSemCB *semHandle = NULL;
            semHandle = GET_SEM(xQueueOrSemaphore->Id.semId);
            if (semHandle->semCount == xQueueOrSemaphore->semInitCount) {    // 当前可用信号量数=初始化时的信号量数，表示没有任何线程占用信号量
                return true;
            }
            return false;
        }
        return false;   // 传来的句柄不在队列、信号量、互斥量中，直接返回false
    }

    QueueSetHandle_t xQueueCreateSet(const UBaseType_t uxEventQueueLength)
    {
        QueueSetHandle_t pxQueue = NULL;

        pxQueue = xQueueGenericCreate(uxEventQueueLength, (UBaseType_t) sizeof(Queue_t *), queueQUEUE_TYPE_SET);
        if (pxQueue == NULL) {
            return NULL;
        }
        pxQueue->pxQueueSetContainer = NULL;
        pxQueue->semInitCount = 0;

        return pxQueue;
    }

    BaseType_t xQueueAddToSet(QueueSetMemberHandle_t xQueueOrSemaphore, QueueSetHandle_t xQueueSet)
    {
        BaseType_t xReturn = pdPASS;

        if (xQueueOrSemaphore == NULL || xQueueSet == NULL) {
            return pdFAIL;
        }

        taskENTER_CRITICAL();
        {
            if (xQueueOrSemaphore->pxQueueSetContainer == NULL && prvIsQueueEmpty(xQueueOrSemaphore)) {
                xQueueOrSemaphore->pxQueueSetContainer = xQueueSet;
            } else {
                xReturn = pdFAIL;
            }
        }
        taskEXIT_CRITICAL();

        return xReturn;
    }

    BaseType_t xQueueRemoveFromSet(QueueSetMemberHandle_t xQueueOrSemaphore, QueueSetHandle_t xQueueSet)
    {
        if (xQueueOrSemaphore == NULL || xQueueSet == NULL) {
            return pdFAIL;
        }

        if (xQueueOrSemaphore->pxQueueSetContainer != xQueueSet) {  // 句柄不在队列集中
            return pdFAIL;
        }
        if (!prvIsQueueEmpty(xQueueOrSemaphore)) { // 判空，只有空的队列/信号量能从队列集中移出
            return pdFAIL;
        }

        taskENTER_CRITICAL(); {
            /* The queue is no longer contained in the set. */
            xQueueOrSemaphore->pxQueueSetContainer = NULL;
        }
        taskEXIT_CRITICAL();
        return pdPASS;
    }

    QueueSetMemberHandle_t xQueueSelectFromSet(QueueSetHandle_t xQueueSet, TickType_t const xTicksToWait)
    {
        QueueSetMemberHandle_t xReturn = NULL;

        // 获取队列队首
        xQueueReceive((QueueHandle_t)xQueueSet, &xReturn, xTicksToWait);

        return xReturn;
    }
    
    QueueSetMemberHandle_t xQueueSelectFromSetFromISR(QueueSetHandle_t xQueueSet)
    {
        QueueSetMemberHandle_t xReturn = NULL;

        xQueueReceive((QueueHandle_t)xQueueSet, &xReturn, 0);

        return xReturn;
    }

//#endif /* configUSE_QUEUE_SETS && configSUPPORT_DYNAMIC_ALLOCATI ON*/
#endif