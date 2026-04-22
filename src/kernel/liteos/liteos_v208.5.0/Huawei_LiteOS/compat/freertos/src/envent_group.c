/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2026. All rights reserved.
 * Description : LiteOS adapt FreeRTOS Event Group.
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
#include "event_groups.h"

/* LiteOS includes. */
#include "los_event.h"
#include "los_memory.h"
#include "los_task.h"

typedef struct EventGroupDef_t {
    EVENT_CB_S event;
    UINT8      isStatic;
} EventGroup_t;

#if (configSUPPORT_STATIC_ALLOCATION == 1)
EventGroupHandle_t xEventGroupCreateStatic(StaticEventGroup_t *pxEventGroupBuffer)
{
    if ((pxEventGroupBuffer == NULL) || (sizeof(StaticEventGroup_t) <= sizeof(EventGroup_t))) {
        return NULL;
    }
    EventGroup_t *pxEventBits = (EventGroup_t *)pxEventGroupBuffer;
    UINT32 ret = LOS_EventInit(&pxEventBits->event);
    if (ret != LOS_OK) {
        return NULL;
    }
    pxEventBits->isStatic = pdTRUE;
    return (EventGroupHandle_t)pxEventBits;
}

BaseType_t xEventGroupGetStaticBuffer(EventGroupHandle_t xEventGroup, StaticEventGroup_t **ppxEventGroupBuffer)
{
    BaseType_t xReturn;
    EventGroup_t *pxEventBits = xEventGroup;

    if (pxEventBits == NULL || ppxEventGroupBuffer == NULL) {
        return pdFALSE;
    }

#if (configSUPPORT_DYNAMIC_ALLOCATION == 1)
    /* Check if the event group was statically allocated. */
    if (pxEventBits->isStatic == (uint8_t)pdTRUE) {
        *ppxEventGroupBuffer = (StaticEventGroup_t *)pxEventBits;
        xReturn = pdTRUE;
    } else {
        xReturn = pdFALSE;
    }
#else
    *ppxEventGroupBuffer = (StaticEventGroup_t *) pxEventBits;
    xReturn = pdTRUE;
#endif /* configSUPPORT_DYNAMIC_ALLOCATION */
    return xReturn;
}
#endif /* configSUPPORT_STATIC_ALLOCATION */

#if (configSUPPORT_DYNAMIC_ALLOCATION == 1)
EventGroupHandle_t xEventGroupCreate(void)
{
    UINT32 ret;
    EventGroup_t *pxEventBits = (EventGroup_t *)LOS_MemAlloc(OS_SYS_MEM_ADDR, sizeof(EventGroup_t));
    if (pxEventBits == NULL) {
        return NULL;
    }

    ret = LOS_EventInit(&pxEventBits->event);
    if (ret != LOS_OK) {
        LOS_MemFree(OS_SYS_MEM_ADDR, pxEventBits);
        return NULL;
    }
    pxEventBits->isStatic = pdFALSE;
    return (EventGroupHandle_t)pxEventBits;
}
#endif /* configSUPPORT_DYNAMIC_ALLOCATION */

EventBits_t xEventGroupWaitBits(EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToWaitFor,
    const BaseType_t xClearOnExit, const BaseType_t xWaitForAllBits, TickType_t xTicksToWait)
{
    if (xEventGroup == NULL || (uxBitsToWaitFor == 0)) {
        return 0;
    }
    EventGroup_t *pxEventBits = (EventGroup_t *)xEventGroup;
    UINT32 mode = 0;

    /* Map FreeRTOS wait logic to LiteOS mode */
    if (xWaitForAllBits != pdFALSE) {
        mode |= LOS_WAITMODE_AND;
    } else {
        mode |= LOS_WAITMODE_OR;
    }

    if (xClearOnExit != pdFALSE) {
        mode |= LOS_WAITMODE_CLR;
    }

    UINT32 recved;
    UINT32 ret = LOS_EventRead(&pxEventBits->event, (UINT32)uxBitsToWaitFor, mode, (UINT32)xTicksToWait);
    if (ret == LOS_ERRNO_EVENT_READ_TIMEOUT) {
        recved = pxEventBits->event.uwEventID;
    } else if ((ret & LOS_ERRNO_EVENT_SETBIT_INVALID) != 0) {
        return 0;
    } else {
        recved = ret;
    }

    return (EventBits_t)recved;
}

EventBits_t xEventGroupClearBits(EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToClear)
{
    if (xEventGroup == NULL) {
        return 0;
    }
    EventGroup_t *pxEventBits = (EventGroup_t *)xEventGroup;
    EventBits_t uxReturn = (EventBits_t)pxEventBits->event.uwEventID;
    UINT32 intSave = LOS_IntLock();
    pxEventBits->event.uwEventID &= ~((UINT32)uxBitsToClear);
    LOS_IntRestore(intSave);

    return uxReturn;
}

#if (configUSE_TRACE_FACILITY == 1)
BaseType_t xEventGroupClearBitsFromISR(EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToClear)
{
    return xEventGroupClearBits(xEventGroup, uxBitsToClear);
}
#endif

VOID vEventGroupClearBitsCallback(VOID *pvEventGroup, uint32_t ulBitsToClear)
{
    if (pvEventGroup == NULL) {
        return;
    }
    (VOID) xEventGroupClearBits(pvEventGroup, (EventBits_t)ulBitsToClear);
}

EventBits_t xEventGroupGetBitsFromISR(EventGroupHandle_t xEventGroup)
{
    return xEventGroupClearBits(xEventGroup, 0);
}

EventBits_t xEventGroupSetBits(EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet)
{
    if (xEventGroup == NULL) {
        return 0;
    }
    EventGroup_t *pxEventBits = (EventGroup_t *)xEventGroup;
    UINT32 ret = LOS_EventWrite(&pxEventBits->event, (UINT32)uxBitsToSet);
    if (ret != LOS_OK) {
        return 0;
    }

    /* FreeRTOS returns the event group value AFTER the bits have been set. */
    EventBits_t uxReturn = (EventBits_t)pxEventBits->event.uwEventID;
    return uxReturn;
}

VOID vEventGroupSetBitsCallback(VOID *pvEventGroup, uint32_t ulBitsToSet)
{
    if (pvEventGroup == NULL) {
        return;
    }
    (VOID)xEventGroupSetBits(pvEventGroup, (EventBits_t)ulBitsToSet);
}


void vEventGroupDelete(EventGroupHandle_t xEventGroup)
{
    if (xEventGroup == NULL) {
        return;
    }
    EventGroup_t *pxEventBits = (EventGroup_t *)xEventGroup;
    LOS_EventDestroy(&pxEventBits->event);

    if (pxEventBits->isStatic != pdTRUE) {
        (VOID)LOS_MemFree(OS_SYS_MEM_ADDR, pxEventBits);
        return;
    } else {
        (VOID)memset_s(pxEventBits, sizeof(EventGroup_t), 0, sizeof(EventGroup_t));
    }
}

#if ((configUSE_TRACE_FACILITY == 1) && (INCLUDE_xTimerPendFunctionCall == 1) && (configUSE_TIMERS == 1))
BaseType_t xEventGroupSetBitsFromISR(
    EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet, BaseType_t *pxHigherPriorityTaskWoken)
{
    xEventGroupSetBits(xEventGroup, uxBitsToSet);
    if (pxHigherPriorityTaskWoken != NULL) {
        *pxHigherPriorityTaskWoken = pdFALSE;
    }

    return pdPASS;
}

#endif /* if (configUSE_TRACE_FACILITY == 1) ... */

EventBits_t xEventGroupSync(EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet,
    const EventBits_t uxBitsToWaitFor, TickType_t xTicksToWait)
{
    if (xEventGroup == NULL) {
        return 0;
    }
    EventGroup_t *pxEventBits = (EventGroup_t *)xEventGroup;
    UINT32 mode = 0;

    if (uxBitsToSet != 0U) {
        (VOID)LOS_EventWrite(&pxEventBits->event, (UINT32)uxBitsToSet);
    }
    if (uxBitsToWaitFor == 0U) {
        return (EventBits_t)pxEventBits->event.uwEventID;
    }
    mode |= LOS_WAITMODE_AND;
    (VOID)LOS_EventRead(&pxEventBits->event, (UINT32)uxBitsToWaitFor, mode, (UINT32)xTicksToWait);

    return (EventBits_t)pxEventBits->event.uwEventID;
}
