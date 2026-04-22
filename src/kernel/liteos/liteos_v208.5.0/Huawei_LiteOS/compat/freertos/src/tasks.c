/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2026. All rights reserved.
 * Description : LiteOS adapt FreeRTOS task.
 * Author : Huawei LiteOS Team
 * Create : 2026-1-13
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
/* Standard includes. */
#include <stdlib.h>
#include <string.h>

/* liteos includes. */
#include "los_task.h"
#include "los_task_base.h"
#include "los_task_pri.h"
#include "los_spinlock.h"
#include "los_stackinfo_pri.h"
#include "los_mp_pri.h"
#include "los_swtmr.h"
#include "los_swtmr_pri.h"

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "stack_macros.h"

#ifndef configINITIAL_TICK_COUNT
    #define configINITIAL_TICK_COUNT    0
#endif

#ifndef portMAX_DELAY
    #define portMAX_DELAY    (TickType_t) 0xffffffffUL
#endif

/* Values that can be assigned to the ucNotifyState member of the TCB. */
#define taskNOT_WAITING_NOTIFICATION              ((uint8_t) 0) /* Must be zero as it is the initialised value. */
#define taskWAITING_NOTIFICATION                  ((uint8_t) 1)
#define taskNOTIFICATION_RECEIVED                 ((uint8_t) 2)

/* Other file private variables. --------------------------------*/
PRIVILEGED_DATA static volatile BaseType_t xNumOfOverflows = (BaseType_t) 0;
PRIVILEGED_DATA static volatile TickType_t xTickCount = (TickType_t) configINITIAL_TICK_COUNT;
size_t xCriticalNesting = (size_t)0xaaaaaaaa;

/*
 * 用于适配liteos的tskTCB
 */
typedef struct tskTaskControlBlock {
    UINT32 taskId;
    #if (configUSE_APPLICATION_TASK_TAG == 1)
        TaskHookFunction_t pxTaskTag;
    #endif
    #if (configUSE_TASK_NOTIFICATIONS == 1)
        UINT16 swtmrId;
        volatile uint32_t ulNotifiedValue[configTASK_NOTIFICATION_ARRAY_ENTRIES];
        volatile uint8_t ucNotifyState[configTASK_NOTIFICATION_ARRAY_ENTRIES];
    #endif
    #if (INCLUDE_xTaskAbortDelay == 1)
        uint8_t ucDelayAborted;
    #endif
    #if (configNUM_THREAD_LOCAL_STORAGE_POINTERS > 0)
    void *pvThreadLocalStoragePointers[configNUM_THREAD_LOCAL_STORAGE_POINTERS];
    #endif /* configNUM_THREAD_LOCAL_STORAGE_POINTERS > 0 */
} tskTCB;
typedef tskTCB TCB_t;

#if (LOSCFG_BASE_CORE_TSK_LIMIT >= 1)
    portDONT_DISCARD PRIVILEGED_DATA TCB_t *volatile pxCurrentTCBs[LOSCFG_BASE_CORE_TSK_LIMIT] = {0};
    #define pxCurrentTCB    xTaskGetCurrentTaskHandle()
#endif

/* spinlock for freertos task module, only available on SMP mode */
SPIN_LOCK_S g_fr_taskSpin;
LITE_OS_SEC_BSS  SPIN_LOCK_INIT(g_fr_taskSpin);

#define FR_TASK_LOCK(state)       LOS_SpinLockSave(&g_fr_taskSpin, &(state))
#define FR_TASK_UNLOCK(state)     LOS_SpinUnlockRestore(&g_fr_taskSpin, (state))

static eTaskState xTaskStatusAdapter(UINT16 taskStatus)
{
    if (taskStatus & OS_TASK_STATUS_UNUSED) {
        return eDeleted;
    }

    if (taskStatus & OS_TASK_STATUS_SUSPEND) {
        return eSuspended;
    }

    if (taskStatus & OS_TASK_STATUS_READY) {
        return eReady;
    }

    if (taskStatus & OS_TASK_STATUS_PEND) {
        return eBlocked;
    }

    if (taskStatus & OS_TASK_STATUS_RUNNING) {
        return eRunning;
    }

    return eInvalid;
}

static BaseType_t xTaskMapTaskId(TaskHandle_t xtask, UINT32 taskId)
{
    UINT32 intSave;
    if (taskId >= LOSCFG_BASE_CORE_TSK_LIMIT) {
        return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
    }
    FR_TASK_LOCK(intSave);
    pxCurrentTCBs[taskId] = xtask;
    FR_TASK_UNLOCK(intSave);
    return pdPASS;
}

TaskHandle_t xTaskGetCurrentTaskHandle(void)
{
    TaskHandle_t xReturn;
    UINT32 intSave;
    UINT32 taskId = LOS_CurTaskIDGet();
    if (taskId > LOSCFG_BASE_CORE_TSK_LIMIT) {
        return NULL;
    }
    FR_TASK_LOCK(intSave);
    xReturn = pxCurrentTCBs[taskId];
    FR_TASK_UNLOCK(intSave);

    return xReturn;
}

eTaskState eTaskGetState(TaskHandle_t xTask)
{
    UINT32 taskId;
    if (xTask == NULL) {
        taskId = LOS_CurTaskIDGet();
    } else {
        taskId = xTask->taskId;
    }
    const LosTaskCB *taskCB =  OS_TCB_FROM_TID(taskId);
    return xTaskStatusAdapter(taskCB->taskStatus);
}

BaseType_t xTaskCreate(TaskFunction_t pxTaskCode,
                       const char * const pcName,
                       const configSTACK_DEPTH_TYPE uxStackDepth,
                       void * const pvParameters,
                       UBaseType_t uxPriority,
                       TaskHandle_t * const pxCreatedTask)
{
    UINT32 ret;
    TSK_INIT_PARAM_S initParam = {0};
    TaskHandle_t pxNewTCB;
    if (pxTaskCode == NULL) {
        return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
    }

    pxNewTCB = (TaskHandle_t)LOS_MemAlloc(OS_SYS_MEM_ADDR, sizeof(tskTCB));
#if (configUSE_TASK_NOTIFICATIONS == 1)
    for (UINT32 i = 0; i < configTASK_NOTIFICATION_ARRAY_ENTRIES; i++) {
        pxNewTCB->ulNotifiedValue[i] = 0;
        pxNewTCB->ucNotifyState[i] = 0;
    }
#endif
    initParam.pcName = (char *)pcName;
    initParam.pfnTaskEntry = (TSK_ENTRY_FUNC)pxTaskCode;
    initParam.pArgs = pvParameters;
    initParam.uwStackSize = uxStackDepth * sizeof(StackType_t);
    initParam.usTaskPrio = uxPriority;

#if (INCLUDE_xTaskAbortDelay == 1)
    pxNewTCB->ucDelayAborted = pdFALSE;
#endif

    ret = LOS_TaskCreateOnly(&(pxNewTCB->taskId), &initParam);
    if (ret != LOS_OK) {
        LOS_MemFree(OS_SYS_MEM_ADDR, pxNewTCB);
        return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
    }
    ret = xTaskMapTaskId(pxNewTCB, (pxNewTCB)->taskId);
    if (ret != pdPASS) {
        LOS_MemFree(OS_SYS_MEM_ADDR, pxNewTCB);
        return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
    }

    if (pxCreatedTask != NULL) {
        *pxCreatedTask = pxNewTCB;
    }

    ret = LOS_TaskResume(pxNewTCB->taskId);
    if (ret != LOS_OK) {
        LOS_MemFree(OS_SYS_MEM_ADDR, pxNewTCB);
        return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
    }

    return pdPASS;
}

TaskHandle_t xTaskCreateStatic(TaskFunction_t pxTaskCode,
                               const char * const pcName,
                               const configSTACK_DEPTH_TYPE uxStackDepth,
                               void * const pvParameters,
                               UBaseType_t uxPriority,
                               StackType_t * const puxStackBuffer,
                               StaticTask_t * const pxTaskBuffer)
{
    UINT32 ret;
    TSK_INIT_PARAM_S initParam = {0};

    if (pxTaskCode == NULL) {
        return NULL;
    }

    if (puxStackBuffer == NULL) {
        return NULL;
    }

    TaskHandle_t task = (TaskHandle_t)LOS_MemAlloc(OS_SYS_MEM_ADDR, sizeof(tskTCB));
    if (task == NULL) {
        return NULL;
    }
#if (configUSE_TASK_NOTIFICATIONS == 1)
    for (UINT32 i = 0; i < configTASK_NOTIFICATION_ARRAY_ENTRIES; i++) {
        task->ulNotifiedValue[i] = 0;
        task->ucNotifyState[i] = 0;
    }
#endif
    initParam.pcName = (char *)pcName;
    initParam.pfnTaskEntry = (TSK_ENTRY_FUNC)pxTaskCode;
    initParam.pArgs = pvParameters;
    initParam.uwStackSize = uxStackDepth * sizeof(StackType_t);
    initParam.usTaskPrio = uxPriority;

#if (INCLUDE_xTaskAbortDelay == 1)
    task->ucDelayAborted = pdFALSE;
#endif

    ret = LOS_TaskCreateOnlyStatic(&(task->taskId), &initParam, puxStackBuffer);
    if (ret != LOS_OK) {
        LOS_MemFree(OS_SYS_MEM_ADDR, task);
        return NULL;
    }

    ret = xTaskMapTaskId(task, task->taskId);
    if (ret != pdPASS) {
        LOS_MemFree(OS_SYS_MEM_ADDR, task);
        return NULL;
    }

    ret = LOS_TaskResume(task->taskId);
    if (ret != LOS_OK) {
        LOS_MemFree(OS_SYS_MEM_ADDR, task);
        return NULL;
    }

    (void)pxTaskBuffer;
    return task;
}

TickType_t xTaskGetTickCount(void)
{
    // liteos为自系统启动开始，freertos为调度开始。
    return LOS_TickCountGet();
}

void vTaskDelete(TaskHandle_t xTaskToDelete)
{
    UINT32 taskId;
    if (xTaskToDelete == NULL) {
        taskId = LOS_CurTaskIDGet();
    } else {
        taskId = xTaskToDelete->taskId;
    }

    if (pxCurrentTCBs[taskId] == NULL) {
        return;
    }

    LOS_TaskDelete(pxCurrentTCBs[taskId]->taskId);
    LOS_MemFree(OS_SYS_MEM_ADDR, pxCurrentTCBs[taskId]);
    pxCurrentTCBs[taskId] = NULL;
}

void vTaskDelay(const TickType_t xTicksToDelay)
{
    LOS_TaskDelay(xTicksToDelay);
}

BaseType_t xTaskDelayUntil(TickType_t *pxPreviousWakeTime, const TickType_t xTimeIncrement)
{
    if (pxPreviousWakeTime == NULL) {
        return pdFALSE;
    }
    UINT32 intSave;
    BaseType_t retVal = pdFALSE;
    TickType_t cur_tick = LOS_TickCountGet();
    /* 计算下一个唤醒点(无符号tick，溢出用减法) */
    FR_TASK_LOCK(intSave);
    TickType_t next_wake = *pxPreviousWakeTime + xTimeIncrement;
    if ((TickType_t)(cur_tick - *pxPreviousWakeTime) < xTimeIncrement) {
        /* 还没到时间，计算剩余tick */
        TickType_t left_tick = next_wake - cur_tick;
        *pxPreviousWakeTime = next_wake;
        FR_TASK_UNLOCK(intSave);
        UINT32 losRet = LOS_TaskDelay(left_tick);
        return (losRet == LOS_OK) ? pdTRUE : pdFALSE;
    } else {
        /* 已经超期：仍推进基准时间，避免累计漂移 */
        *pxPreviousWakeTime = next_wake;
        retVal = pdFALSE;
    }
    FR_TASK_UNLOCK(intSave);
    return retVal;
}

UBaseType_t uxTaskPriorityGet(const TaskHandle_t xTask)
{
    UINT32 taskId;
    if (xTask == NULL) {
        taskId = LOS_CurTaskIDGet();
    } else {
        taskId = xTask->taskId;
    }

    return LOS_TaskPriGet(taskId);
}

void vTaskPrioritySet(TaskHandle_t xTask, UBaseType_t uxNewPriority)
{
    UINT32 taskId;
    if (xTask == NULL) {
        taskId = LOS_CurTaskIDGet();
    } else {
        taskId = xTask->taskId;
    }

    LOS_TaskPriSet(taskId, uxNewPriority);
}

void vTaskSuspend(TaskHandle_t xTaskToSuspend)
{
    UINT32 taskId;
    if (xTaskToSuspend == NULL) {
        taskId = LOS_CurTaskIDGet();
    } else {
        taskId = xTaskToSuspend->taskId;
    }
    LOS_TaskSuspend(taskId);
}

void vTaskResume(TaskHandle_t xTaskToResume)
{
    if (xTaskToResume == NULL) {
        return;
    }

    LOS_TaskResume(xTaskToResume->taskId);
}

BaseType_t xTaskResumeFromISR(TaskHandle_t xTaskToResume)
{
    if (xTaskToResume == NULL) {
        return pdFALSE;
    }

    UINT32 ret = LOS_TaskResume(xTaskToResume->taskId);
    if (ret == LOS_OK) {
        return pdTRUE;
    }

    return pdFALSE;
}

BaseType_t xTaskAbortDelay(TaskHandle_t xTask)
{
    // 强制唤醒任务暂不支持
    return pdFALSE;
}

UBaseType_t uxTaskPriorityGetFromISR(const TaskHandle_t xTask)
{
    UINT32 taskId;
    if (xTask == NULL) {
        taskId = LOS_CurTaskIDGet();
    } else {
        taskId = xTask->taskId;
    }

    UBaseType_t pri = LOS_TaskPriGet(taskId);

    return pri;
}

UBaseType_t uxTaskBasePriorityGet(const TaskHandle_t xTask)
{
    UINT32 taskId;
    if (xTask == NULL) {
        taskId = LOS_CurTaskIDGet();
    } else {
        taskId = xTask->taskId;
    }
    const LosTaskCB *taskCB =  OS_TCB_FROM_TID(taskId);
    if (taskCB == NULL) {
        return 0;
    }
    if (taskCB->priBitMap != 0) {
        return taskCB->priBitMap;
    } else {
        return taskCB->priority;
    }
}

UBaseType_t uxTaskBasePriorityGetFromISR(const TaskHandle_t xTask)
{
    UINT32 intSave;

    FR_TASK_LOCK(intSave);
    UBaseType_t pri = uxTaskBasePriorityGet(xTask);
    FR_TASK_UNLOCK(intSave);

    return pri;
}

UBaseType_t uxTaskGetNumberOfTasks(void)
{
    return g_taskUsed;
}

UBaseType_t uxTaskGetSystemState(TaskStatus_t * const pxTaskStatusArray,
                                 const UBaseType_t uxArraySize,
                                 configRUN_TIME_COUNTER_TYPE * const pulTotalRunTime)
{
    UBaseType_t task_num = uxTaskGetNumberOfTasks();
    if (pxTaskStatusArray == NULL || uxArraySize < task_num) {
        return 0;
    }
    const LosTaskCB *taskCB = NULL;
    UINT32 intSave;
    FR_TASK_LOCK(intSave);
    for (UBaseType_t loop = 0; loop < task_num; loop++) {
        taskCB = g_osTaskCBArray + loop;
        pxTaskStatusArray[loop].xHandle = pxCurrentTCBs[taskCB->taskId];
        pxTaskStatusArray[loop].pcTaskName = taskCB->taskName;
        pxTaskStatusArray[loop].xTaskNumber = taskCB->taskId;
        pxTaskStatusArray[loop].eCurrentState = xTaskStatusAdapter(taskCB->taskStatus);
        pxTaskStatusArray[loop].uxCurrentPriority = taskCB->priority;
        pxTaskStatusArray[loop].uxBasePriority = taskCB->priBitMap != 0 ? taskCB->priBitMap : taskCB->priority;
        pxTaskStatusArray[loop].pxStackBase = (StackType_t *)taskCB->topOfStack;
        UINTPTR uwBottomOfStack = TRUNCATE(((UINTPTR)taskCB->topOfStack + taskCB->stackSize),
                                           LOSCFG_STACK_POINT_ALIGN_SIZE);
        OsStackWaterLineGet((const UINTPTR *)uwBottomOfStack,
                            (const UINTPTR *)taskCB->topOfStack, &pxTaskStatusArray[loop].usStackHighWaterMark);
#ifdef LOSCFG_WCFS_SCHEDULER
        pxTaskStatusArray[loop].ulRunTimeCounter = taskCB->runtick;
#endif
    }
    FR_TASK_UNLOCK(intSave);
    if (pulTotalRunTime != NULL) {
        *pulTotalRunTime = LOS_TickCountGet();
    }
    return task_num;
}

void vTaskGetInfo(TaskHandle_t xTask,
                  TaskStatus_t * pxTaskStatus,
                  BaseType_t xGetFreeStackSpace,
                  eTaskState eState)
{
    UINT32 taskId;
    if (pxTaskStatus == NULL) {
        return;
    }
    if (xTask == NULL) {
        taskId = LOS_CurTaskIDGet();
    } else {
        taskId = xTask->taskId;
    }
    const LosTaskCB *taskCB =  OS_TCB_FROM_TID(taskId);
    if (taskCB == NULL) {
        return;
    }
    UINT32 intSave;
    FR_TASK_LOCK(intSave);
    pxTaskStatus->xHandle = pxCurrentTCBs[taskCB->taskId];
    pxTaskStatus->pcTaskName = taskCB->taskName;
    pxTaskStatus->xTaskNumber = taskCB->taskId;
    pxTaskStatus->eCurrentState = xTaskStatusAdapter(taskCB->taskStatus);
    pxTaskStatus->uxCurrentPriority = taskCB->priority;
    pxTaskStatus->uxBasePriority = taskCB->priBitMap != 0 ? taskCB->priBitMap : taskCB->priority;
    pxTaskStatus->pxStackBase = (StackType_t *)taskCB->topOfStack;
    UINTPTR uwBottomOfStack = TRUNCATE(((UINTPTR)taskCB->topOfStack + taskCB->stackSize),
                                       LOSCFG_STACK_POINT_ALIGN_SIZE);
    OsStackWaterLineGet((const UINTPTR *)uwBottomOfStack,
                        (const UINTPTR *)taskCB->topOfStack, &pxTaskStatus->usStackHighWaterMark);
#ifdef LOSCFG_WCFS_SCHEDULER
    pxTaskStatus->ulRunTimeCounter = taskCB->runtick;
#endif
    FR_TASK_UNLOCK(intSave);
    xGetFreeStackSpace = pxTaskStatus->usStackHighWaterMark;
    (void)xGetFreeStackSpace;
}

#if (configUSE_APPLICATION_TASK_TAG == 1)
TaskHookFunction_t xTaskGetApplicationTaskTag(TaskHandle_t xTask)
{
    UINT32 taskId;
    TaskHookFunction_t xReturn;
    if (xTask == NULL) {
        taskId = LOS_CurTaskIDGet();
        xReturn = pxCurrentTCBs[taskId]->pxTaskTag;
    } else {
        xReturn = xTask->pxTaskTag;
    }
    return xReturn;
}

TaskHookFunction_t xTaskGetApplicationTaskTagFromISR(TaskHandle_t xTask)
{
    UINT32 intSave;
    TaskHookFunction_t xReturn;
    FR_TASK_LOCK(intSave);
    xReturn = xTaskGetApplicationTaskTag(xTask);
    FR_TASK_UNLOCK(intSave);
    return xReturn;
}

BaseType_t xTaskCallApplicationTaskHook(TaskHandle_t xTask, void *pvParameter)
{
    UINT32 taskId;
    BaseType_t ret;
    TaskHookFunction_t RunpxTaskTag = NULL;
    if (xTask == NULL) {
        taskId = LOS_CurTaskIDGet();
        RunpxTaskTag = pxCurrentTCBs[taskId]->pxTaskTag;
    } else {
        RunpxTaskTag = xTask->pxTaskTag;
    }

    if (RunpxTaskTag != NULL) {
        ret = RunpxTaskTag(pvParameter);
    } else {
        ret = pdFAIL;
    }
    return ret;
}

void vTaskSetApplicationTaskTag(TaskHandle_t xTask, TaskHookFunction_t pxTagValue)
{
    UINT32 taskId;
    if (xTask == NULL) {
        taskId = LOS_CurTaskIDGet();
        pxCurrentTCBs[taskId]->pxTaskTag = pxTagValue;
    } else {
        xTask->pxTaskTag = pxTagValue;
    }
}
#endif

#if (INCLUDE_uxTaskGetStackHighWaterMark == 1)
UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t xTask)
{
    UINT32 taskId, ret;
    if (xTask == NULL) {
        taskId = LOS_CurTaskIDGet();
    } else {
        taskId = xTask->taskId;
    }
    const LosTaskCB *taskCB = OS_TCB_FROM_TID(taskId);
    UBaseType_t usStackHighWaterMark = 0;
    UINTPTR uwBottomOfStack = TRUNCATE(((UINTPTR)taskCB->topOfStack + taskCB->stackSize),
                                       LOSCFG_STACK_POINT_ALIGN_SIZE);
    ret = OsStackWaterLineGet((const UINTPTR *)uwBottomOfStack,
                              (const UINTPTR *)taskCB->topOfStack, &usStackHighWaterMark);
    if (ret == LOS_NOK) {
        return 0;
    }
    return usStackHighWaterMark;
}
#endif

#if (INCLUDE_uxTaskGetStackHighWaterMark2 == 1)
configSTACK_DEPTH_TYPE uxTaskGetStackHighWaterMark2(TaskHandle_t xTask)
{
    UINT32 taskId, ret;
    if (xTask == NULL) {
        taskId = LOS_CurTaskIDGet();
    } else {
        taskId = xTask->taskId;
    }
    const LosTaskCB *taskCB = OS_TCB_FROM_TID(taskId);
    configSTACK_DEPTH_TYPE usStackHighWaterMark = 0;
    UINTPTR uwBottomOfStack = TRUNCATE(((UINTPTR)taskCB->topOfStack + taskCB->stackSize),
                                       LOSCFG_STACK_POINT_ALIGN_SIZE);
    ret = OsStackWaterLineGet((const UINTPTR *)uwBottomOfStack,
                              (const UINTPTR *)taskCB->topOfStack, &usStackHighWaterMark);
    if (ret == LOS_NOK) {
        return 0;
    }
    return usStackHighWaterMark;
}
#endif

#if (configNUM_THREAD_LOCAL_STORAGE_POINTERS > 0)
void vTaskSetThreadLocalStoragePointer(TaskHandle_t xTaskToSet, BaseType_t xIndex, void *pvValue)
{
    UINT32 taskId;
    TaskHandle_t xtaskCB = NULL;
    if (xTaskToSet == NULL) {
        taskId = LOS_CurTaskIDGet();
        xtaskCB = pxCurrentTCBs[taskId];
    } else {
        xtaskCB = xTaskToSet;
    }
    if ((xIndex >= 0) &&
            (xIndex < (BaseType_t) configNUM_THREAD_LOCAL_STORAGE_POINTERS)) {
        if (xtaskCB == NULL) {
            return;
        }
        xtaskCB->pvThreadLocalStoragePointers[xIndex] = pvValue;
    }
}

void *pvTaskGetThreadLocalStoragePointer(TaskHandle_t xTaskToQuery, BaseType_t xIndex)
{
    UINT32 taskId;
    void *pvReturn = NULL;
    TaskHandle_t xtaskCB = NULL;
    if (xTaskToQuery == NULL) {
        taskId = LOS_CurTaskIDGet();
        xtaskCB = pxCurrentTCBs[taskId];
    } else {
        xtaskCB = xTaskToQuery;
    }
    if ((xIndex >= 0) &&
            (xIndex < (BaseType_t) configNUM_THREAD_LOCAL_STORAGE_POINTERS)) {
        if (xtaskCB == NULL) {
            return NULL;
        }

        pvReturn = xtaskCB->pvThreadLocalStoragePointers[xIndex];
    } else {
        pvReturn = NULL;
    }
    return pvReturn;
}
#endif /* configNUM_THREAD_LOCAL_STORAGE_POINTERS > 0 */

void vTaskSetTimeOutState(TimeOut_t * const pxTimeOut)
{
    if (pxTimeOut == NULL) {
        return;
    }
    pxTimeOut->xOverflowCount = xNumOfOverflows;
    pxTimeOut->xTimeOnEntering = LOS_TickCountGet();
}

BaseType_t xTaskCheckForTimeOut(TimeOut_t * const pxTimeOut, TickType_t * const pxTicksToWait)
{
    BaseType_t xReturn;
    if (pxTimeOut == NULL || pxTicksToWait == NULL) {
        return pdTRUE;
    }
        
    const TickType_t xConstTickCount = LOS_TickCountGet();
    const TickType_t xElapsedTime = xConstTickCount - pxTimeOut->xTimeOnEntering;
#if (INCLUDE_vTaskSuspend == 1)
    if (*pxTicksToWait == portMAX_DELAY) {
        /* If INCLUDE_vTaskSuspend is set to 1 and the block time
        * specified is the maximum block time then the task should block
        * indefinitely, and therefore never time out. */
        return pdFALSE;
    }
#endif
    if (xConstTickCount < pxTimeOut->xTimeOnEntering) {
            xReturn = pdTRUE;
            *pxTicksToWait = (TickType_t) 0;
    } else if (xElapsedTime < *pxTicksToWait) {
            /* Not a genuine timeout. Adjust parameters for time remaining. */
            *pxTicksToWait -= xElapsedTime;
            vTaskSetTimeOutState(pxTimeOut);
            xReturn = pdFALSE;
    } else {
            *pxTicksToWait = (TickType_t) 0;
            xReturn = pdTRUE;
    }
    return xReturn;
}

eSleepModeStatus eTaskConfirmSleepModeStatus(void)
{
    // 不支持低功耗模式
    return eAbortSleep;
}

#if ((portCRITICAL_NESTING_IN_TCB == 1 ) || ( configNUMBER_OF_CORES > 1))
void vTaskEnterCritical(void)
{
    portENTER_CRITICAL();
}

void vTaskExitCritical(void)
{
    portEXIT_CRITICAL();
}
#endif

void vTaskStartScheduler(void)
{
    // liteos启动流程已经启动调度
    return;
}

void vTaskEndScheduler(void)
{
    // 此函数目前仅在 x86 实模式 PC 端口中实现。
    return;
}

void vTaskSuspendAll(void)
{
    LOS_TaskLock();
}

BaseType_t xTaskResumeAll(void)
{
    LOS_TaskUnlock();
    return pdFALSE;
}

void vTaskStepTick(TickType_t xTicksToJump)
{
    // 在低功耗休眠后（liteos），更新xTickCount
    xTickCount += xTicksToJump;
}

BaseType_t xTaskCatchUpTicks(TickType_t xTicksToCatchUp)
{
    BaseType_t xYieldOccurred;
    vTaskSuspendAll();
    vTaskStepTick(xTicksToCatchUp);
    xYieldOccurred = xTaskResumeAll();
    return xYieldOccurred;
}

TickType_t xTaskGetTickCountFromISR(void)
{
    // liteos为自系统启动开始，freertos为调度开始。
    return LOS_TickCountGet();
}

#if (configUSE_TASK_NOTIFICATIONS == 1)
/* 定时唤醒需要等待通知的任务 */
STATIC VOID wake_notifiedvalue_callback(UINTPTR arg)
{
    UINT32 taskId = (UINT32)arg;
    LOS_TaskResume(taskId);
    return;
}

static void wait_notifiedvalue_timeout(TCB_t *taskCB, TickType_t xTicksToWait)
{
    UINT32 ret;
    if (xTicksToWait == 0) {
        return;
    }
    if (xTicksToWait < portMAX_DELAY) {
        ret = LOS_SwtmrCreate(xTicksToWait, LOS_SWTMR_MODE_ONCE, (SWTMR_PROC_FUNC)wake_notifiedvalue_callback,
                              &(taskCB->swtmrId), (UINTPTR)(taskCB->taskId));
        if (ret != LOS_OK) {
            return;
        }
        LOS_SwtmrStart(taskCB->swtmrId);
    }
    LOS_TaskSuspend(taskCB->taskId);
}

STATIC UINT32 wake_notifiedvalue(UINT32 taskId)
{
    LOS_SwtmrStop(pxCurrentTCBs[taskId]->swtmrId);
    LOS_SwtmrDelete(pxCurrentTCBs[taskId]->swtmrId);
    LOS_TaskResume(taskId);
    return LOS_OK;
}

BaseType_t xTaskGenericNotify(TaskHandle_t xTaskToNotify,
                              UBaseType_t uxIndexToNotify,
                              uint32_t ulValue,
                              eNotifyAction eAction,
                              uint32_t * pulPreviousNotificationValue)
{
    if (uxIndexToNotify >= configTASK_NOTIFICATION_ARRAY_ENTRIES || xTaskToNotify == NULL) {
        return pdFAIL;
    }
    BaseType_t xReturn = pdPASS;
    uint8_t ucOriginalNotifyState;
    TCB_t *taskCB = pxCurrentTCBs[xTaskToNotify->taskId];
    
    taskENTER_CRITICAL();
    if (pulPreviousNotificationValue != NULL) {
        *pulPreviousNotificationValue = taskCB->ulNotifiedValue[uxIndexToNotify];
    }

    ucOriginalNotifyState = taskCB->ucNotifyState[uxIndexToNotify];

    taskCB->ucNotifyState[uxIndexToNotify] = taskNOTIFICATION_RECEIVED;

    switch (eAction) {
        case eSetBits:
            taskCB->ulNotifiedValue[uxIndexToNotify] |= ulValue;
            break;
        case eIncrement:
            (taskCB->ulNotifiedValue[uxIndexToNotify])++;
            break;
        case eSetValueWithOverwrite:
            taskCB->ulNotifiedValue[uxIndexToNotify] = ulValue;
            break;
        case eSetValueWithoutOverwrite:
            if (ucOriginalNotifyState != taskNOTIFICATION_RECEIVED) {
                taskCB->ulNotifiedValue[uxIndexToNotify] = ulValue;
            } else {
                /* The value could not be written to the task. */
                xReturn = pdFAIL;
            }
            break;
        case eNoAction:
            /* The task is being notified without its notify value being
             * updated. */
            break;
        default:
            /* Should not get here if all enums are handled.
            * Artificially force an assert by testing a value the
            * compiler can't assume is const. */
            configASSERT(xTickCount == (TickType_t) 0);
            break;
    }

    if (ucOriginalNotifyState == taskWAITING_NOTIFICATION) {
        wake_notifiedvalue(xTaskToNotify->taskId);
    } else {
        mtCOVERAGE_TEST_MARKER();
    }
    taskEXIT_CRITICAL();
    return xReturn;
}

BaseType_t xTaskGenericNotifyFromISR(TaskHandle_t xTaskToNotify,
                                     UBaseType_t uxIndexToNotify,
                                     uint32_t ulValue,
                                     eNotifyAction eAction,
                                     uint32_t * pulPreviousNotificationValue,
                                     BaseType_t * pxHigherPriorityTaskWoken)
{
    BaseType_t xReturn;

    xReturn = xTaskGenericNotify(xTaskToNotify, uxIndexToNotify, ulValue, eAction, pulPreviousNotificationValue);
    if (pxHigherPriorityTaskWoken != NULL) {
        *pxHigherPriorityTaskWoken = pdFALSE;
    }
    return xReturn;
}

void vTaskGenericNotifyGiveFromISR(TaskHandle_t xTaskToNotify,
                                   UBaseType_t uxIndexToNotify,
                                   BaseType_t * pxHigherPriorityTaskWoken)
{
    xTaskNotifyGiveIndexed(xTaskToNotify, uxIndexToNotify);
    if (pxHigherPriorityTaskWoken != NULL) {
        *pxHigherPriorityTaskWoken = pdFALSE;
    }
}

uint32_t ulTaskGenericNotifyTake(UBaseType_t uxIndexToWaitOn,
                                 BaseType_t xClearCountOnExit,
                                 TickType_t xTicksToWait)
{
    uint32_t ulReturn;
    UINT32 intSave;
    if (uxIndexToWaitOn >= configTASK_NOTIFICATION_ARRAY_ENTRIES) {
        return 0;
    }
    UINT32 taskId = LOS_CurTaskIDGet();
    TCB_t *taskCB = pxCurrentTCBs[taskId];

    FR_TASK_LOCK(intSave);
    /* Only block if the notification count is not already non-zero. */
    if (taskCB->ulNotifiedValue[uxIndexToWaitOn] == 0U) {
        /* Mark this task as waiting for a notification. */
        taskCB->ucNotifyState[uxIndexToWaitOn] = taskWAITING_NOTIFICATION;
        if (xTicksToWait > (TickType_t) 0) {
            wait_notifiedvalue_timeout(taskCB, xTicksToWait);
            FR_TASK_UNLOCK(intSave);
        } else {
            mtCOVERAGE_TEST_MARKER();
        }
    } else {
        mtCOVERAGE_TEST_MARKER();
    }

    FR_TASK_UNLOCK(intSave);

    FR_TASK_LOCK(intSave);
    ulReturn = taskCB->ulNotifiedValue[uxIndexToWaitOn];
    if (ulReturn != 0U) {
        if (xClearCountOnExit != pdFALSE) {
            taskCB->ulNotifiedValue[uxIndexToWaitOn] = (uint32_t) 0U;
        } else {
            taskCB->ulNotifiedValue[uxIndexToWaitOn] = ulReturn - (uint32_t) 1;
        }
    } else {
        mtCOVERAGE_TEST_MARKER();
    }

    taskCB->ucNotifyState[uxIndexToWaitOn] = taskNOT_WAITING_NOTIFICATION;
    FR_TASK_UNLOCK(intSave);

    return ulReturn;
}

BaseType_t xTaskGenericNotifyWait(UBaseType_t uxIndexToWaitOn,
                                  uint32_t ulBitsToClearOnEntry,
                                  uint32_t ulBitsToClearOnExit,
                                  uint32_t * pulNotificationValue,
                                  TickType_t xTicksToWait)
{
    BaseType_t xReturn;
    UINT32 intSave;
    if (uxIndexToWaitOn >= configTASK_NOTIFICATION_ARRAY_ENTRIES) {
        return pdFAIL;
    }
    UINT32 taskId = LOS_CurTaskIDGet();
    TCB_t *taskCB = pxCurrentTCBs[taskId];

    FR_TASK_LOCK(intSave);
    /* Only block if a notification is not already pending. */
    if (taskCB->ucNotifyState[uxIndexToWaitOn] != taskNOTIFICATION_RECEIVED) {
        /* Clear bits in the task's notification value as bits may get
        * set by the notifying task or interrupt. This can be used
        * to clear the value to zero. */
        taskCB->ulNotifiedValue[uxIndexToWaitOn] &= ~ulBitsToClearOnEntry;
        /* Mark this task as waiting for a notification. */
        taskCB->ucNotifyState[uxIndexToWaitOn] = taskWAITING_NOTIFICATION;
        if (xTicksToWait > (TickType_t) 0) {
            wait_notifiedvalue_timeout(taskCB, xTicksToWait);
            FR_TASK_UNLOCK(intSave);
        } else {
            mtCOVERAGE_TEST_MARKER();
        }
    } else {
        mtCOVERAGE_TEST_MARKER();
    }
    FR_TASK_UNLOCK(intSave);

    FR_TASK_LOCK(intSave);
    if (pulNotificationValue != NULL) {
        /* Output the current notification value, which may or may not
        * have changed. */
        *pulNotificationValue = taskCB->ulNotifiedValue[uxIndexToWaitOn];
    }
    /* If ucNotifyValue is set then either the task never entered the
    * blocked state (because a notification was already pending) or the
    * task unblocked because of a notification.  Otherwise the task
    * unblocked because of a timeout. */
    if (taskCB->ucNotifyState[uxIndexToWaitOn] != taskNOTIFICATION_RECEIVED) {
        /* A notification was not received. */
        xReturn = pdFALSE;
    } else {
        /* A notification was already pending or a notification was
        * received while the task was waiting. */
        taskCB->ulNotifiedValue[uxIndexToWaitOn] &= ~ulBitsToClearOnExit;
        xReturn = pdTRUE;
    }

    taskCB->ucNotifyState[uxIndexToWaitOn] = taskNOT_WAITING_NOTIFICATION;
    FR_TASK_UNLOCK(intSave);

    return xReturn;
}

BaseType_t xTaskGenericNotifyStateClear(TaskHandle_t xTask, UBaseType_t uxIndexToClear)
{
    BaseType_t xReturn;

    if (uxIndexToClear >= configTASK_NOTIFICATION_ARRAY_ENTRIES) {
        return pdFAIL;
    }
    UINT32 taskId;
    if (xTask == NULL) {
        taskId = LOS_CurTaskIDGet();
    } else {
        taskId = xTask->taskId;
    }
    TCB_t *taskCB = pxCurrentTCBs[taskId];
    taskENTER_CRITICAL();
    if (taskCB->ucNotifyState[uxIndexToClear] == taskNOTIFICATION_RECEIVED) {
        taskCB->ucNotifyState[uxIndexToClear] = taskNOT_WAITING_NOTIFICATION;
        xReturn = pdPASS;
    } else {
        xReturn = pdFAIL;
    }
    taskEXIT_CRITICAL();
    return xReturn;
}

uint32_t ulTaskGenericNotifyValueClear(TaskHandle_t xTask, UBaseType_t uxIndexToClear, uint32_t ulBitsToClear)
{
    uint32_t ulReturn;

    if (uxIndexToClear >= configTASK_NOTIFICATION_ARRAY_ENTRIES) {
        return pdFAIL;
    }
    UINT32 taskId;
    if (xTask == NULL) {
        taskId = LOS_CurTaskIDGet();
    } else {
        taskId = xTask->taskId;
    }
    TCB_t *taskCB = pxCurrentTCBs[taskId];
    taskENTER_CRITICAL();
    /* Return the notification as it was before the bits were cleared,
    * then clear the bit mask. */
    ulReturn = taskCB->ulNotifiedValue[uxIndexToClear];
    taskCB->ulNotifiedValue[uxIndexToClear] &= ~ulBitsToClear;
    taskEXIT_CRITICAL();

    return ulReturn;
}

#endif /* configUSE_TASK_NOTIFICATIONS */
