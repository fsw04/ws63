/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2026. All rights reserved.
 * Description : LiteOS adapt FreeRTOS Timer.
 * Author : Huawei LiteOS Team
 * Create : 2026-01-05
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

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

/* LiteOS Kernel Header Files */
#include "los_swtmr_pri.h"
#include "los_memory.h"
#include "los_task.h"

typedef void (* TimerCallbackFunction_t)(TimerHandle_t xTimer);

typedef struct tmrTimerControl {
    const char *pcTimerName;
    void *pvTimerID;
    TimerCallbackFunction_t pxCallback;
    TickType_t xTimerPeriodInTicks;
    UINT16 usSwTmrID;
    UINT8  ucStatus;
} xTIMER;

typedef xTIMER Timer_t;

#define TIMER_STATUS_IS_ACTIVE                  (0x01U)
#define TIMER_STATUS_IS_STATICALLY_ALLOCATED    (0x02U)
#define TIMER_STATUS_IS_AUTORELOAD              (0x04U)

#if (configUSE_TIMERS == 1)
static void prvTimerCallbackWrapper(UINT32 uwArg)
{
    Timer_t *pxTimer = (Timer_t *)(UINTPTR)uwArg;
    if (pxTimer == NULL) {
        return;
    }
    if (pxTimer->pxCallback != NULL) {
        pxTimer->pxCallback((TimerHandle_t) pxTimer);
    }
}

#if (configSUPPORT_DYNAMIC_ALLOCATION == 1)
TimerHandle_t xTimerCreate(const char * const pcTimerName, const TickType_t xTimerPeriodInTicks,
    const BaseType_t xAutoReload, VOID * const pvTimerID, TimerCallbackFunction_t pxCallbackFunction)
{
    UINT8 ucMode;
    UINT32 uwRet;

    Timer_t *pxNewTimer = (Timer_t *)LOS_MemAlloc(OS_SYS_MEM_ADDR, sizeof(Timer_t));
    if (pxNewTimer == NULL) {
        return NULL;
    }
    pxNewTimer->pcTimerName = pcTimerName;
    pxNewTimer->pvTimerID = pvTimerID;
    pxNewTimer->pxCallback = pxCallbackFunction;
    pxNewTimer->xTimerPeriodInTicks = xTimerPeriodInTicks;
    pxNewTimer->ucStatus = 0;
    if (xAutoReload != pdFALSE) {
        pxNewTimer->ucStatus |= (uint8_t)TIMER_STATUS_IS_AUTORELOAD;
    }

    ucMode = (xAutoReload != pdFALSE) ? LOS_SWTMR_MODE_PERIOD : LOS_SWTMR_MODE_ONCE;
    uwRet = LOS_SwtmrCreate((UINT32) xTimerPeriodInTicks, ucMode, \
        (SWTMR_PROC_FUNC)prvTimerCallbackWrapper, \
        &pxNewTimer->usSwTmrID, (UINT32)(UINTPTR)pxNewTimer);
    if (uwRet != LOS_OK) {
        LOS_MemFree(OS_SYS_MEM_ADDR, pxNewTimer);
        return NULL;
    }
    return (TimerHandle_t) pxNewTimer;
}

#endif /* configSUPPORT_DYNAMIC_ALLOCATION */

#if (configSUPPORT_STATIC_ALLOCATION == 1)
TimerHandle_t xTimerCreateStatic(const char * const pcTimerName, const TickType_t xTimerPeriodInTicks,
    const BaseType_t xAutoReload, VOID * const pvTimerID, TimerCallbackFunction_t pxCallbackFunction,
    StaticTimer_t *pxTimerBuffer)
{
    UINT8 ucMode;
    UINT32 uwRet;
    Timer_t *pxNewTimer = (Timer_t *) pxTimerBuffer;

    if (pxNewTimer == NULL) {
        return NULL;
    }
    /* Initialize static timer fields */
    pxNewTimer->pcTimerName = pcTimerName;
    pxNewTimer->pvTimerID = pvTimerID;
    pxNewTimer->pxCallback = pxCallbackFunction;
    pxNewTimer->xTimerPeriodInTicks = xTimerPeriodInTicks;
    pxNewTimer->ucStatus = TIMER_STATUS_IS_STATICALLY_ALLOCATED;
    if (xAutoReload != pdFALSE) {
        pxNewTimer->ucStatus |= (uint8_t)TIMER_STATUS_IS_AUTORELOAD;
    }

    ucMode = (xAutoReload != pdFALSE) ? LOS_SWTMR_MODE_PERIOD : LOS_SWTMR_MODE_ONCE;
    uwRet = LOS_SwtmrCreate((UINT32)xTimerPeriodInTicks, ucMode, \
        (SWTMR_PROC_FUNC)prvTimerCallbackWrapper, \
        &pxNewTimer->usSwTmrID, (UINT32)(UINTPTR)pxNewTimer);
    if (uwRet != LOS_OK) {
        return NULL;
    }

    return (TimerHandle_t)pxNewTimer;
}
#endif /* configSUPPORT_STATIC_ALLOCATION */

BaseType_t xTimerGenericCommandFromTask(TimerHandle_t xTimer, const BaseType_t xCommandID,
    const TickType_t xOptionalValue, BaseType_t *const pxHigherPriorityTaskWoken, const TickType_t xTicksToWait)
{
    Timer_t *pxTimer = (Timer_t *)xTimer;
    BaseType_t xReturn = pdFAIL;
    (void) xTicksToWait;

    if (pxTimer == NULL) {
        return pdFAIL;
    }
    switch (xCommandID) {
        case tmrCOMMAND_START:
        case tmrCOMMAND_START_FROM_ISR:
        case tmrCOMMAND_RESET:
        case tmrCOMMAND_RESET_FROM_ISR:
            if (LOS_SwtmrStart(pxTimer->usSwTmrID) == LOS_OK) {
                pxTimer->ucStatus |= TIMER_STATUS_IS_ACTIVE;
                xReturn = pdPASS;
            }
            break;

        case tmrCOMMAND_STOP:
        case tmrCOMMAND_STOP_FROM_ISR:
            if (LOS_SwtmrStop(pxTimer->usSwTmrID) == LOS_OK) {
                pxTimer->ucStatus &= ~TIMER_STATUS_IS_ACTIVE;
                xReturn = pdPASS;
            }
            break;
        case tmrCOMMAND_CHANGE_PERIOD:
        case tmrCOMMAND_CHANGE_PERIOD_FROM_ISR:
            if (OsSwtmrStartTimer(pxTimer->usSwTmrID, xOptionalValue, xOptionalValue) == LOS_OK) {
                pxTimer->xTimerPeriodInTicks = xOptionalValue;
                pxTimer->ucStatus |= TIMER_STATUS_IS_ACTIVE;
                xReturn = pdPASS;
            }
            break;

        case tmrCOMMAND_DELETE:
            if (LOS_SwtmrDelete(pxTimer->usSwTmrID) == LOS_OK) {
#if (configSUPPORT_DYNAMIC_ALLOCATION == 1)
                if ((pxTimer->ucStatus & TIMER_STATUS_IS_STATICALLY_ALLOCATED) != 0) {
                    LOS_MemFree(OS_SYS_MEM_ADDR, pxTimer);
                }
#endif
                xReturn = pdPASS;
            }
            break;
        default:
            break;
    }

    if (pxHigherPriorityTaskWoken != NULL) {
        *pxHigherPriorityTaskWoken = pdFALSE;
    }
    return xReturn;
}

BaseType_t xTimerGenericCommandFromISR(TimerHandle_t xTimer, const BaseType_t xCommandID,
    const TickType_t xOptionalValue, BaseType_t *const pxHigherPriorityTaskWoken, const TickType_t xTicksToWait)
{
    return xTimerGenericCommandFromTask (xTimer, xCommandID, xOptionalValue, pxHigherPriorityTaskWoken, 0);
}

TickType_t xTimerGetPeriod(TimerHandle_t xTimer)
{
    if (xTimer == NULL) {
        return 0;
    }
    Timer_t *pxTimer = (Timer_t *)xTimer;
    return pxTimer->xTimerPeriodInTicks;
}

void vTimerSetReloadMode(TimerHandle_t xTimer, const BaseType_t xAutoReload)
{
    if (xTimer == NULL) {
        return;
    }
    Timer_t *pxTimer = (Timer_t *)xTimer;
    if (xAutoReload != pdFALSE) {
        pxTimer->ucStatus |= (uint8_t)TIMER_STATUS_IS_AUTORELOAD;
    } else {
        pxTimer->ucStatus &= ((uint8_t) ~TIMER_STATUS_IS_AUTORELOAD);
    }
}

BaseType_t xTimerGetReloadMode(TimerHandle_t xTimer)
{
    if (xTimer == NULL) {
        return pdFALSE;
    }
    Timer_t *pxTimer = (Timer_t *)xTimer;
    if ((pxTimer->ucStatus & TIMER_STATUS_IS_AUTORELOAD) == 0U) {
        return pdFALSE;
    }
    return pdTRUE;
}

UBaseType_t uxTimerGetReloadMode(TimerHandle_t xTimer)
{
    return (UBaseType_t)xTimerGetReloadMode(xTimer);
}


TickType_t xTimerGetExpiryTime(TimerHandle_t xTimer)
{
    if (xTimer == NULL) {
        return 0;
    }
    Timer_t *pxTimer = (Timer_t *)xTimer;
    UINT32 uwRemainingTicks;
    UINT32 uwRet;

    uwRet = LOS_SwtmrTimeGet(pxTimer->usSwTmrID, &uwRemainingTicks);
    if (uwRet == LOS_OK) {
        return (TickType_t)(LOS_TickCountGet() + uwRemainingTicks);
    }
    return 0;
}


const char *pcTimerGetName(TimerHandle_t xTimer)
{
    if (xTimer == NULL) {
        return NULL;
    }
    Timer_t *pxTimer = (Timer_t *)xTimer;
    return pxTimer->pcTimerName;
}


BaseType_t xTimerIsTimerActive(TimerHandle_t xTimer)
{
    if (xTimer == NULL) {
        return pdFALSE;
    }
    Timer_t *pxTimer = (Timer_t *)xTimer;
    return ((pxTimer->ucStatus & TIMER_STATUS_IS_ACTIVE) != 0) ? pdTRUE : pdFALSE;
}


#if (INCLUDE_xTimerPendFunctionCall == 1)
BaseType_t xTimerPendFunctionCallFromISR (PendedFunction_t xFunctionToPend,
                                          VOID * pvParameter1,
                                          uint32_t ulParameter2,
                                          BaseType_t * pxHigherPriorityTaskWoken)
{
    (VOID)pxHigherPriorityTaskWoken;
    if (xFunctionToPend == NULL) {
        return pdFAIL;
    }

    xFunctionToPend(pvParameter1, ulParameter2);
    return pdPASS;
}

BaseType_t xTimerPendFunctionCall (PendedFunction_t xFunctionToPend,
                                   VOID * pvParameter1,
                                   uint32_t ulParameter2,
                                   TickType_t xTicksToWait)
{
    (VOID)xTicksToWait;
    if (xFunctionToPend == NULL) {
        return pdFAIL;
    }

    xFunctionToPend(pvParameter1, ulParameter2);
    return pdPASS;
}
#endif /* INCLUDE_xTimerPendFunctionCall */

void *pvTimerGetTimerID(const TimerHandle_t xTimer)
{
    if (xTimer == NULL) {
        return NULL;
    }
    Timer_t *pxTimer = (Timer_t *)xTimer;
    return pxTimer->pvTimerID;
}


void vTimerSetTimerID(TimerHandle_t xTimer, void *pvNewID)
{
    if (xTimer == NULL) {
        return;
    }
    Timer_t *pxTimer = (Timer_t *)xTimer;
    pxTimer->pvTimerID = pvNewID;
}
#endif /* configUSE_TIMERS == 1 */
