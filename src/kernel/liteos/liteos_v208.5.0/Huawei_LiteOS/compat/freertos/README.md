
# FreeRTOS 适配

# 1 概述

这是基于 **LiteOS内核** 开发的 **FreeRTOS 适配层**代码。可以让在 FreeRTOS 上运行的系统，无痛迁移到 LiteOS 上。内核 API 基于 **FreeRTOS 11.1.0** 。目前已验证多款 FreeRTOS SDK 在该适配层上运行。
>FreeRTOS 文档：https://www.freertos.org/zh-cn-cmn-s/Documentation/02-Kernel/04-API-references/01-Task-creation/00-TaskHandle

# 2 支持的接口

+ **中断 (Interrupt)**
  `portENTER_CRITICAL`, `portEXIT_CRITICAL`, `portCLEAR_INTERRUPT_MASK_FROM_ISR`, `portSET_INTERRUPT_MASK_FROM_ISR`
+ **任务 (Task)**
  `xTaskGetTickCount`, `xTaskGetCurrentTaskHandle`, `xTaskCreate`, `xTaskCreateStatic`, `vTaskDelete`, `vTaskDelay`, `vTaskDelayUntil`, `xTaskDelayUntil`, `uxTaskPriorityGet`, `vTaskPrioritySet`, `vTaskSuspend`, `vTaskResume`, `xTaskResumeFromISR`, `xTaskAbortDelay`, `uxTaskPriorityGetFromISR`, `uxTaskBasePriorityGet`, `uxTaskBasePriorityGetFromISR`, `uxTaskGetSystemState`, `vTaskGetInfo`, `xTaskGetApplicationTaskTag`, `uxTaskGetStackHighWaterMark`, `xTaskCallApplicationTaskHook`, `vTaskSetApplicationTag`, `vTaskSetThreadLocalStoragePointer`, `pvTaskGetThreadLocalStoragePointer`, `vTaskSetTimeOutState`, `xTaskCheckForTimeOut`, `eTaskConfirmSleepModeStatus`, `taskYIELD`, `taskENTER_CRITICAL`, `taskEXIT_CRITICAL`, `taskENTER_CRITICAL_FROM_ISR`, `taskEXIT_CRITICAL_FROM_ISR`, `vTaskStartScheduler`, `vTaskEndScheduler`, `vTaskSuspendAll`, `xTaskResumeAll`, `vTaskStepTick`, `xTaskCatchUpTicks`
+ **任务通知 (Task Notify)**
  `xTaskNotifyGive`, `xTaskNotifyGiveIndexed`, `vTaskNotifyGiveFromISR`, `vTaskNotifyGiveIndexedFromISR`, `ulTaskNotifyTake`, `ulTaskNotifyTakeIndexed`, `xTaskNotify`, `xTaskNotifyIndexed`, `xTaskNotifyAndQuery`, `xTaskNotifyAndQueryIndexed`, `xTaskNotifyAndQueryFromISR`, `xTaskNotifyAndQueryIndexedFromISR`, `xTaskNotifyFromISR`, `xTaskNotifyIndexedFromISR`, `xTaskNotifyWait`, `xTaskNotifyWaitIndexed`, `xTaskNotifyStateClear`, `xTaskNotifyStateClearIndexed`, `ulTasknotifyValueClear`, `ulTaskNotifyValueClearIndexed`
+ **队列 (Queue)**
  `uxQueueMessagesWaiting`, `uxQueueMessagesWaitingFromISR`, `uxQueueSpacesAvailable`, `vQueueDelete`, `xQueueCreate`, `xQueueCreateStatic`, `xQueueGetStaticBuffers`, `xQueueIsQueueEmptyFromISR`, `xQueueIsQueueFullFromISR`, `xQueueOverwrite`, `xQueueOverwriteFromISR`, `xQueueReceive`, `xQueueReceiveFromISR`, `xQueueReset`, `xQueueSend`, `xQueueSendFromISR`, `xQueueSendToBack`, `xQueueSendToBackFromISR`, `xQueueSendToFront`, `xQueueSendToFrontFromISR`
+ **队列集 (Queue Set)**
  `xQueueCreateSet`, `xQueueAddToSet`, `xQueueRemoveFromSet`, `xQueueSelectFromSet`, `xQueueSelectFromSetFromISR`
+ **流缓冲区 (Stream)**
  `xStreamBufferCreate`, `xStreamBufferCreateStatic`, `xStreamBufferSend`, `xStreamBufferSendFromISR`, `xStreamBufferReceive`, `xStreamBufferReceiveFromISR`, `vStreamBufferDelete`, `xStreamBufferBytesAvailable`, `xStreamBufferSpacesAvailable`, `xStreamBufferSetTriggerLevel`, `xStreamBufferReset`, `xStreamBufferResetFromISR`, `xStreamBufferIsEmpty`, `xStreamBufferIsFull`, `xStreamBufferGetStaticBuffers`, `uxStreamBufferGetStreamBufferNotificationIndex`, `vStreamBufferSetStreamBufferNotificationIndex`, `xStreamBatchingBufferCreate`, `xStreamBufferSendCompletedFromISR`, `xStreamBufferReceiveCompletedFromISR`, `xStreamBatchingBufferCreateStatic`
+ **消息缓冲区 (Message)**
  `xMessageBufferCreate`, `xMessageBufferCreateStatic`, `xMessageBufferSend`, `xMessageBufferSendFromISR`, `xMessageBufferReceive`, `xMessageBufferReceiveFromISR`, `vMessageBufferDelete`, `xMessageBufferSpacesAvailable`, `xMessageBufferReset`, `xMessageBufferResetFromISR`, `xMessageBufferIsEmpty`, `xMessageBufferIsFull`, `xMessageBufferSendCompletedFromISR`, `xMessageBufferReceiveCompletedFromISR`, `xMessageBufferGetStaticBuffers`, `xMessageBufferSpaceAvailable`
+ **信号量/互斥锁**
  `xSemaphoreCreateBinary`, `xSemaphoreCreateBinaryStatic`, `vSemaphoreCreateBinary`, `xSemaphoreCreateCounting`, `xSemaphoreCreateCountingStatic`, `xSemaphoreCreateMutex`, `xSemaphoreCreateMutexStatic`, `xSemaphoreCreateRecursiveMutex`, `xSemaphoreCreateRecursiveMutexStatic`, `xSemaphoreGetMutexHolder`, `uxSemaphoreGetCount`, `xSemaphoreTake`, `xSemaphoreTakeFromISR`, `xSemaphoreTakeRecursive`, `xSemaphoreGive`, `xSemaphoreGiveFromISR`, `xSemaphoreGiveRecursive`, `vSemaphoreDelete`
+ **软件定时器 (Timer)**
  `xTimerCreate`, `xTimerCreateStatic`, `xTimerIsTimerActive`, `xTimerStart`, `xTimerStartFromISR`, `xTimerStop`, `xTimerStopFromISR`, `xTimerChangePeriod`, `xTimerChangePeriodFromISR`, `xTimerDelete`, `xTimerReset`, `xTimerResetFromISR`, `pvTimerGetTimerID`, `vTimerSetTimerID`, `vTimerSetReloadMode`, `xTimerGetTimerID`, `xTimerGetReloadMode`, `xTimerPendFunctionCall`, `xTimerPendFunctionCallFromISR`, `pcTimerGetName`, `xTimerGetPeriod`, `xTimerGetExpiryTime`
+ **事件组 (Event)**
  `xEventGroupCreate`, `xEventGroupCreateStatic`, `vEventGroupDelete`, `xEventGroupWaitBits`, `xEventGroupSetBits`, `xEventGroupSetBitsFromISR`, `xEventGroupClearBits`, `xEventGroupClearBitsFromISR`, `xEventGroupGetBits`, `xEventGroupGetBitsFromISR`, `xEventGroupSync`, `xEventGroupGetStaticBuffer`
+ **内存 (Memory)**
  `pvPortMalloc`, `vPortFree`

# 3 接口适配差异

因为代码基于 LiteOS 内核开发，部分功能可能和 FreeRTOS 内核源码有所差异。

## 3.1 中断

+ portSET_INTERRUPT_MASK_FROM_ISR：暂不支持按优先级屏蔽中断，该宏的效果等同于portDISABLE_INTERRUPTS

## 3.2 队列集

- xQueueAddToSet
  当前接口新增约束：当队列不为空/互斥量被其他线程占用/信号量被其他线程持有时，无法将句柄添加到队列集
- xQueueRemoveFromSet
  当前接口约束补充说明：当队列不为空/互斥量被其他线程占用/信号量被其他线程持有时，无法将句柄从队列集移出（freeRTOS原文为：仅当队列或信号量为空时，才能从队列集中删除 RTOS 队列或信号量）
- xQueueSelectFromSetFromISR
  当前接口与xQueueReceiveFromISR类似，底层调用xQueueReceive，其中xTicksToWait参数为0

## 3.3 定时器

- xTimerPendFunctionCall
  当前接口改为同步立即执行，依赖异步与任务上下文的应用逻辑。
- vTimerSetReloadMode
  LiteOS不支持定时器模式的更改，因此该接口不能实际修改定时器模式

## 3.4 信号量和互斥锁

- SemaphoreHandle_t xSemaphoreCreateCounting( UBaseType_t uxMaxCount,
  UBaseType_t uxInitialCount);
  对于信号量：FreeRTOS 中“队列长度”在这里用于设置信号量的初始计数（`count`），而不是最大计数，LiteOS 的 `LOS_SemCreate(count)` 通常意味着“当前计数 = 最大计数 = count”；这与 FreeRTOS `xQueueCreateCountingSemaphore(uxMax, uxInitial)` 的语义有一定差异。

## 3.5 内存

+ `vPortHeapResetState`为空实现

## 3.6 任务

+ `xTaskGetTickCount`是自系统启动开始计数,原生是任务启动调度开始计数。
+ `xTaskCreate`和`xTaskCreateStatic`原生FreeRtos不会自动释放任务资源，当前实现会自动释放任务资源。
+ `eTaskConfirmSleepModeStatus`当前为空实现。
+ `TaskStartScheduler`启动流程中已经启动调度，故为空实现。
+ `vTaskEndScheduler`为空实现。
+ `vTaskStepTick`和`xTaskCatchUpTicks`为空实现。
+ 通知值相关函数中：原生实现是将任务阻塞，当前适配机制是将任务挂起。

# 4 使用

## 4.1 LiteOS 启用 FreeRTOS 适配层需要开启的宏

| 宏                                | 作用                 | 备注                                                                 |
| ----------------------------------- | ---------------------- | ---------------------------------------------------------------------- |
| LOSCFG_COMPAT_FREERTOS          | FreeRTOS适配层总开关 |                                                                      |
| LOSCFG_TASK_STACK_STATIC_ALLOCATION|静态任务栈| 必须开启，影响编译|
| LOSCFG_COMPAT_FREERTOS_HEAP=3  | 采用何种内存管理方案 | 定义3为liteos申请内存接口，1、2、4、5为FreeRTOS内存管理方式，默认为3 |
| LOSCFG_QUEUE_STATIC_ALLOCATION | 是否支持静态队列申请 | 未定义不影响编译，xTaskCreateStatic接口返回NULL                      |
| LOSCFG_BASE_CORE_SWTMR| timer相关功能 | 必须开，影响编译 |
| LOSCFG_QUEUE_STATIC_ALLOCATION| timer相关功能 | 必须开，影响编译 |
## 4.2 FreeRTOS 宏配置

`FreeRTOSConfig.h`文件路径如下，可以手动修改香港配置

```c
kernel/liteos/liteos_v208.5.0/Huawei_LiteOS/targets/ws63/include/FreeRTOSConfig.h
```

## 4.3 修改cmake，引入适配层代码

在`src/kernel/liteos/liteos_v208.5.0/Huawei_LiteOS/compat/CMakeLists.txt`下面，新增
`

```
list(APPEND MODULE_${LOSCFG_COMPAT_FREERTOS} freertos)
```

# 5 FreeRTOS 内核适配层验证 Demo

复杂用例参考[FreeRTOS 官方 Demo](https://www.freertos.org/zh-cn-cmn-s/Documentation/02-Kernel/03-Supported-devices/04-Demos/Device-independent-demo/Hardware-independent-RTOS-example) 。

## 创建第一个FreeRTOS 任务模块的用例

(可参考[创建第一个Hello World的工程 | BearPi-Pico H3863 | 小熊派BearPi](https://www.bearpi.cn/core_board/bearpi/pico/h3863/software/%E5%88%9B%E5%BB%BA%E7%AC%AC%E4%B8%80%E4%B8%AAHello%20World%E7%9A%84%E5%B7%A5%E7%A8%8B.html))

编译环境准备：1、[fbb_ws63源码](https://gitcode.com/HiSpark/fbb_ws63/tree/master)。2、硬件环境参考：[Window环境下开发环境搭建 | BearPi-Pico H3863 | 小熊派BearPi](https://www.bearpi.cn/core_board/bearpi/pico/h3863/software/%E7%8E%AF%E5%A2%83%E6%90%AD%E5%BB%BAwindows_IDE.html)

1. 在 application/samples 中创建一个 demo 代码目录，同时在该目录下创建 demo.c、demo.h、CMakeLists.txt 文件
2. 注册demo 目录到工程
  （1）在 CMkakeLists.txt 中为组件模板添加源码文件以及源码头文件路径。
  ```
  set(SOURCES_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/demo.c
	)

	set(PUBLIC_HEADER_LIST
    	${CMAKE_CURRENT_SOURCE_DIR}
	)

	set(SOURCES "${SOURCES_LIST}" PARENT_SCOPE)
	set(PUBLIC_HEADER "${PUBLIC_HEADER_LIST}" PARENT_SCOPE)
  ```
  （2）在/samples/demo/的/samples/文件夹CmakeLists.txt里新增
  ```
  add_subdirectory_if_exist(demo)
  ```
3. 在demo.c、demo.h中添加自己的FreeRTOS 相关的业务代码，这里以在demo.c中添加一个创建任务，并在任务中打印消息为例,`app_run`函数为应用程序的入口函数

完整demo.c示例：

```
#include "arch/task.h"
#include "common_def.h"
#include "osal_debug.h"
#include "cmsis_os2.h"
#include "app_init.h"
#include "FreeRTOS.h" // FreeRTOS
#include "task.h"   // FreeRTOS
#include "demo.h"

static StaticTask_t xTaskBuffer;
#define STACK_DEPTH  1024   /* The stack depth is 1024, and the actual stack size is STACK_DEPTH x sizeof(StackType_t). */
static __attribute__((aligned(LOSCFG_STACK_POINT_ALIGN_SIZE))) StackType_t xStack[STACK_DEPTH * sizeof(StackType_t)];
static void vTaskCode(void * pvParameters)
{
    unused(pvParameters);
    for( ;; )
    {
        /* Running the service code. */
        osal_printk("Hello World  BearPi\r\n");
        /* The task was created. Use the NULL handle to delete the this task. */
        vTaskDelete(NULL);
    }
}

/* Function that creates a xtask. */
static void tasks_test_entry(void)
{
    TaskHandle_t xHandle = NULL;
    vTaskSuspendAll();
    /* Create the task. */
    xHandle = xTaskCreateStatic(vTaskCode,     /* Function that implements the task. */
                                "vTaskCode",   /* Text name for the task. */
                                STACK_DEPTH,   /* Number of indexes in the xStack array. */
                                (void *) 1,    /* Parameter passed into the task. */
                                6,             /* Priority at which the task is created. */
                                xStack,        /* Array to use as the task's stack. */
                                &xTaskBuffer); /* Variable to hold the task's data structure. */
    if (xHandle != NULL) {
        osal_printk("vTaskCode is ready\r\n");
    }
    xTaskResumeAll();
}

/* Run the tasks_test_entry. */
app_run(tasks_test_entry);
```

4. 开启对应宏配置和demo.c需要的头文件路径：
  （1） 修改宏配置`LOSCFG_COMPAT_FREERTOS=y`和`LOSCFG_TASK_STACK_STATIC_ALLOCATION=y` 到src/kernel/liteos/liteos_v208.5.0/Huawei_LiteOS/tools/build/config/ws63.config文件中。
  （2）添加头文件路径到CMakeLists.txt的`PUBLIC_HEADER_LIST`中。
  `${CMAKE_CURRENT_SOURCE_DIR}/../../../kernel/liteos/liteos_v208.5.0/Huawei_LiteOS/open_source/FreeRTOS/FreeRTOS-Kernel-11.1.0/include/`
5. 编译烧录
  编译：进入到src目录执行./build.py -c ws63（更详细参考创建第一个Hello World的工程 | BearPi-Pico H3863 | 小熊派BearPi)
  
  ```
  cd ./src
  ./build.py -c ws63
  ```
  
  烧录：（详细烧录可以参考开发环境搭建）
  通过上面编译可以在.\src\output\ws63\fwpkg\ws63-liteos-app目录下生成fwpkg文件，并将该文件烧录进单板。

  
  连接串口工具运行输出
  ```c
  vTaskCode is readey
  ...
  Hello World BearPi
  ```
  说明运行成功
