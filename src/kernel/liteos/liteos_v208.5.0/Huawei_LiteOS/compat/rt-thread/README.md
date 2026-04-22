# 1 概述

这是基于 **LiteOS内核** 开发的 **RT-Thread 适配层**代码。可以让在 RT-Thread 上运行的系统，无痛迁移到 LiteOS 上。内核 API 基于 **RT-Thread 5.2.1** 。目前已验证多款 RT-Thread 软件包在该适配层上运行。
> RT-Thread API文档：https://supperthomas.github.io/RTT_doxygen_API/index.html
> RT-Thread 使用文档：https://www.rt-thread.org/document/site/#/rt-thread-version/rt-thread-standard/programming-manual/basic/basic

# 2 支持的接口
基础内核：


+ **线程管理**
  `rt_thread_init`, `rt_thread_create`, `rt_thread_startup`, `rt_thread_detach`, `rt_thread_delete`, `rt_thread_self`, `rt_thread_yield`, `rt_thread_sleep`, `rt_thread_delay`, `rt_thread_mdelay`, `rt_thread_control`, `rt_thread_suspend`, `rt_thread_resume`, `rt_schedule`, `rt_enter_critical`, `rt_exit_critical`, `rt_critical_level`, `rt_thread_idle_gethandler`
+ **时钟管理**
  `rt_tick_get`, `rt_tick_set`, `rt_tick_increase`, `rt_tick_from_millisecond`
+ **定时器管理**
  `rt_timer_init`, `rt_timer_detach`, `rt_timer_create`, `rt_timer_delete`, `rt_timer_start`, `rt_timer_stop`, `rt_timer_control`
+ **中断管理**
  `rt_interrupt_enter`, `rt_interrupt_leave`, `rt_interrupt_get_nest`, `rt_hw_interrupt_disable`, `rt_hw_interrupt_enable`, `rt_hw_interrupt_init`, `rt_hw_interrupt_mask`, `rt_hw_interrupt_umask`, `rt_hw_interrupt_install`, `rt_hw_interrupt_get_irq`, `rt_hw_interrupt_ack`, `rt_hw_interrupt_set_target_cpus`, `rt_hw_interrupt_get_target_cpus`, `rt_hw_interrupt_set_triger_mode`, `rt_hw_interrupt_get_triger_mode`, `rt_hw_interrupt_set_pending`, `rt_hw_interrupt_get_pending`, `rt_hw_interrupt_clear_pending`, `rt_hw_interrupt_set_priority`, `rt_hw_interrupt_get_priority`, `rt_hw_interrupt_set_prior_group_bits`, `rt_hw_interrupt_get_prior_group_bits`, `rt_hw_ipi_send`, `rt_hw_ipi_handler_install`
+ **信号量 (Semaphore)**
  `rt_sem_init`, `rt_sem_detach`, `rt_sem_create`, `rt_sem_delete`, `rt_sem_take`, `rt_sem_trytake`, `rt_sem_release`
+ **互斥量 (Mutex)**
  `rt_mutex_init`, `rt_mutex_detach`, `rt_mutex_create`, `rt_mutex_delete`, `rt_mutex_take`, `rt_mutex_release`
+ **事件 (Event)**
  `rt_event_init`, `rt_event_detach`, `rt_event_create`, `rt_event_delete`, `rt_event_send`, `rt_event_recv`
+ **邮箱 (Mailbox)**
  `rt_mb_init`, `rt_mb_detach`, `rt_mb_create`, `rt_mb_delete`, `rt_mb_send`, `rt_mb_send_wait`, `rt_mb_recv`
+ **消息队列 (Message Queue)**
  `rt_mq_init`, `rt_mq_detach`, `rt_mq_create`, `rt_mq_delete`, `rt_mq_send`, `rt_mq_send_wait`, `rt_mq_urgent`, `rt_mq_recv`
+ **动态内存管理**
  `rt_malloc`, `rt_realloc`, `rt_calloc`, `rt_malloc_align`, `rt_free`, `rt_free_align`, `rt_memheap_init`
+ **内存池管理**
  `rt_mp_init`, `rt_mp_detach`, `rt_mp_create`, `rt_mp_delete`, `rt_mp_alloc`, `rt_mp_free`
+ **系统钩子 (Hook)**
  `rt_thread_idle_sethook`, `rt_thread_idle_delhook`, `rt_interrupt_enter_sethook`, `rt_interrupt_leave_sethook`, `rt_malloc_sethook`, `rt_free_sethook`, `rt_mp_alloc_sethook`, `rt_mp_free_sethook`, `rt_scheduler_sethook`, `rt_thread_suspend_sethook`, `rt_thread_resume_sethook`, `rt_thread_inited_sethook`
+ **字符串操作**
  `rt_strstr`, `rt_strcasecmp`, `rt_strncpy`, `rt_strncmp`, `rt_strcmp`, `rt_strnlen`, `rt_strlen`, `rt_strdup`, `rt_snprintf`, `rt_vsprintf`, `rt_sprintf`
+ **内存操作**
  `rt_memset`, `rt_memcpy`, `rt_memmove`, `rt_memcmp`

拓展内核：

+ **内核基础服务**
  `rt_show_version`, `rt_console_get_device`, `rt_console_set_device`, `rt_kputs`, `rt_kprintf`, `__rt_ffs`
+ **错误代码管理**
  `rt_get_errno`, `rt_set_errno`
+ **原子操作 (Atomic)**
  `rt_atomic_load`, `rt_atomic_store`, `rt_atomic_exchange`, `rt_atomic_add`, `rt_atomic_sub`, `rt_atomic_xor`, `rt_atomic_and`, `rt_atomic_or`, `rt_atomic_flag_test_and_set`, `rt_atomic_flag_clear`, `rt_atomic_compare_exchange_strong`
+ **等待队列 (Wait Queue)**
  `rt_wqueue_wait`, `rt_wqueue_wakeup`
+ **工作队列 (Work Queue)**
  `rt_workqueue_destroy`, `rt_workqueue_dowork`, `rt_workqueue_cancel_work`, `rt_workqueue_cancel_work_sync`, `rt_work_init`
+ **套接字 (Socket)**
  `socket`, `bind`, `listen`, `accept`, `connect`, `send`, `recv`, `sendto`, `recvfrom`, `closesocket`, `shutdown`, `setsockopt`, `getsockopt`, `getpeername`, `getsockname`, `ioctlsocket`, `gethostbyname`
+ **环形缓冲区 (RingBuffer)**
  `rt_ringbuffer_init`, `rt_ringbuffer_reset`, `rt_ringbuffer_put`, `rt_ringbuffer_put_force`, `rt_ringbuffer_putchar`, `rt_ringbuffer_putchar_force`, `rt_ringbuffer_get`, `rt_ringbuffer_getchar`, `rt_ringbuffer_data_len`, `rt_ringbuffer_create`, `rt_ringbuffer_destroy`, `rt_ringbuffer_get_size`
+ **环形块状缓冲区 (RBB)**
  `rt_rbb_init`, `rt_rbb_create`, `rt_rbb_destroy`, `rt_rbb_get_buf_size`, `rt_rbb_blk_alloc`, `rt_rbb_blk_put`, `rt_rbb_blk_get`, `rt_rbb_blk_free`, `rt_rbb_blk_queue_get`, `rt_rbb_blk_queue_len`, `rt_rbb_blk_queue_buf`, `rt_rbb_blk_queue_free`, `rt_rbb_next_blk_queue_len`
+ **完成信号量 (Completion)**
  `rt_completion_init`, `rt_completion_wait`, `rt_completion_done`
+ **管道 (Pipe)**
  `rt_pipe_open`, `rt_pipe_close`, `rt_pipe_read`, `rt_pipe_write`, `rt_pipe_create`, `rt_pipe_delete`
+ **日志组件 (ULOG)**
  `ulog_init`, `ulog_deinit`, `ulog_flush`, `ulog_hexdump`, `ulog_raw`
+ **动态模块 (DLModule)**
  `dlopen`, `dlsym`, `dlclose`, `dlmodule_load`, `dlmodule_exec`, `dlmodule_exit`, `dlmodule_find`, `dlmodule_symbol_find`
+ **自旋锁与多核支持**
  `rt_spin_lock_init`, `rt_spin_lock`, `rt_spin_unlock`, `rt_spin_lock_irqsave`, `rt_spin_unlock_irqrestore`, `rt_cpu_self`, `rt_cpu_index`, `rt_cpus_lock`, `rt_cpus_unlock`, `rt_cpus_lock_status_restore`

# 3 接口适配差异

## 3.1 初始化

1. INIT_EXPORT：不支持RTT自带的INIT_EXPORT，如果系统启动自动初始化，用liteos的宏 LOS_SYS_INIT
2. liteos已经有初始化实现，不需要专门去调用初始化接口。
   liteos和rtthread初始化接口映射

注：如果要使用idel的钩子，还需要手动调用`rt_thread_idle_init`来挂载钩子函数的数组到liteos

## 3.2 消息队列

`rt_mq_init rt_mq_create`：不支持RT_IPC_FLAG_PRIO模式

## 3.3 RT-Thread的 object 相关操作

暂不支持

## 3.4 中断

+ 暂不支持修改中断优先级，即暂不支持`rt_hw_interrupt_set_priority_mask`和`rt_hw_interrupt_get_priority_mask`
+ LiteOS 暂不支持 SMP，因此相关函数为空实现
+ 部分接口的使用依赖宏`LOSCFG_DEBUG_HWI`的开启，具体需要参考源码

## 3.5 原子操作

RT_Thread提供的3种原子操作在LiteOS统一使用开关全局中断的方式软实现原子操作

## 3.6 动态模块

仅支持dlopen,dlsym,dlclose标准接口，其他接口为空实现

## 3.7 自旋锁

采用单核自旋锁实现，rt_spin_lock，rt_spin_unlock，rt_spin_lock_irqsave，rt_spin_unlock_irqrestore使用全局锁，`rt_cpu_self`和`rt_cpu_index`仅支持单核，其他接口为空实现

## 3.8 工作队列

不支持同步等待机制rt_workqueue_cancel_work_sync用rt_workqueue_cancel_work实现

## 3.9 事件

`rt_event_init`和`rt_event_create`在被调用时需要传入`name`，但是在适配接口中没有使用该参数

## 3.10 任务

任务优先级相关接口，在RT-Thread里优先级级别是根据数字从大到小排序的，在LiteOS里是从小到大排序的。

## 3.11 定时器管理

- rt_timer_init：接口中的参数name实际并未使用；接口返回值为void（原生接口返回错误码或者创建成功的句柄）。
- rt_timer_control：目前支持6种命令，相比原生接口，新增支持命令RT_TIMER_CTRL_GET_STATE（获取定时器状态）、RT_TIMER_CTRL_GET_REMAIN_TIME（获得软件定时器剩余Tick数）。
- rt_timer_detach,rt_timer_delete,rt_timer_start,rt_timer_stop：操作成功返回值为RT_EOK（实际值为0），操作失败返回错误码（非0值），具体错误码参考LiteOS错误码定义。

## 3.12 信号量

- `rt_sem_init`/`rt_sem_detach`： 相比原生RT-Thread只支持FIFO，不支持PRIO。

## 3.13 互斥锁

- `rt_mutex_init`/`rt_mutex_create`：flag参数只是保存了，未实际影响底层 mutex 行为。

## 3.14 消息队列

- `rt_mq_init`/`rt_mq_create`：只支持FIFO，不支持PRIO

# 4 使用

## 4.1 LiteOS 启用 RT-THREAD 要开启的宏

| 宏                                      | 作用                                | 备注           |
| ----------------------------------------- | ------------------------------------- | ---------------- |
| LOSCFG_COMPAT_RT_THREAD              | rtthread适配层总开关                |                |
| LOSCFG_TASK_STACK_STATIC_ALLOCATION | rt_thread.c                        | 不开启影响编译 |
| LOSCFG_QUEUE_STATIC_ALLOCATION       | 影响LOS_TaskCreateStatic rt_mq.c  | 不开启影响编译 |
| LOSCFG_KERNEL_MEMBOX                  | 影响los_membox.c LOS_MEMBOX_NODE | 不开启影响编译 |
| LOSCFG_KERNEL_MEMBOX_STATIC          | 影响los_membox.c LOS_MEMBOX_NODE | 不开启影响编译 |
| LOSCFG_HWI_PRE_POST_PROCESS         | 影响rt_interrupt.c getIrqStatus 等接口    | 不开启影响编译 |
| LOSCFG_DEBUG_HWI                      | 影响rt_interrupt.c getIrqStatus 等接口    | 不开启影响编译 |
| LOSCFG_COMPAT_LINUX_WAITQUEUE        | 影响rt_waitqueue.c                 | 不开启影响编译 |
| LOSCFG_COMPAT_LINUX_WORKQUEUE        | 影响rt_workqueue.c                 | 不开启影响编译 |
| LOSCFG_COMPAT_LINUX_COMPLETION       | 影响rt_completion.c                | 不开启影响编译 |
| LOSCFG_KERNEL_DYNLOAD                 | 影响rt_dl.c                        | 不影响编译     |
| LOSCFG_DYNLOAD_DYN_FROM_FS          | 影响rt_dl.c                        | 不影响编译     |

## 4.2 RT-Thread 宏配置

rtthread相关的宏配置在rtconfig.h里。文件目录：..../work_code/kernel/liteos/liteos_v208.5.0/Huawei_LiteOS/targets/ws63/include/rtconfig.h

# 5 RT-Thread 软件包验证适配层 Demo

本demo以官方的测试用例kernel_samples的动态内存`dynmem_sample.c`为例，展示了如何在LiteOS上，基于RT-Thread的适配接口，运行一个RT-Thread的软件包的完整过程。

## 5.1 开启适配接口

打开LiteOS里RT-Thread适配接口相关的宏：在`/kernel/liteos/liteos_v208.5.0/Huawei_LiteOS/tools/build/config/ws63.config`后面新增

```
LOSCFG_COMPAT_RT_THREAD=y
LOSCFG_TASK_STACK_STATIC_ALLOCATION=y
LOSCFG_QUEUE_STATIC_ALLOCATION=y
LOSCFG_KERNEL_MEMBOX=y
LOSCFG_KERNEL_MEMBOX_STATIC=y
LOSCFG_HWI_PRE_POST_PROCESS=y
LOSCFG_DEBUG_HWI=y
LOSCFG_COMPAT_LINUX_WAITQUEUE=y
LOSCFG_COMPAT_LINUX_WORKQUEUE=y
LOSCFG_COMPAT_LINUX_COMPLETION=y
```

## 5.2 kernel_samples代码下载

下载kernel_samples源码：RT-Thread 官网 `https://packages.rt-thread.org/`可以下载最新的软件包。搜索kernel_samples后进入详情页面，从右边Repository可以选择从gitee或者github代码仓下载源码。


把下载后的代码放到LiteOS的`application/samples/`目录下



## 5.3 配置cmake

在/samples/CMakeLists.txt里新增

```
if(DEFINED LOSCFG_COMPAT_RT_THREAD)
    add_subdirectory_if_exist(kernel-sample-master)
endif()
```


在/samples/kernel-samples-master/里新增CMakeLists.txt，复制下面内容

```
# 定义源码列表，注意路径要写对
set(SOURCES_LIST
    # 定义要编译的文件
    ${CMAKE_CURRENT_SOURCE_DIR}/zh/dynmem_sample.c
)

set(SOURCES "${SOURCES}" ${SOURCES_LIST} PARENT_SCOPE)

# 添加头文件
set(PUBLIC_HEADER_LIST
    "${ROOT_DIR}/kernel/liteos/liteos_v208.5.0/Huawei_LiteOS/open_source/rt-thread/include"
)

set(PUBLIC_HEADER "${PUBLIC_HEADER_LIST}" PARENT_SCOPE)

```

## 5.4 main函数里调用测试函数

ws63的启动函数在`/application/ws63/ws63_liteos_application/main.c`
先声明dynmem_sample.c的测试函数，再在app_main里调用

```c
...
extern int printf(const char *fmt, ...);
/* 线程入口 */
void thread1_entry(void *parameter)
{
    (void)parameter;
    int i;
...
```
注：
dynmem_sample.c的`thread1_entry`函数加下面代码，防止程序报错参数未使用

```
(void)parameter;
```


修改dynmem_sample.c的宏：修改优先级为0，在LiteOS里，任务优先级是从小到大排序的。任务栈大小改为2048

```
#define THREAD_PRIORITY      0
#define THREAD_STACK_SIZE    2048
```

## 5.5 设置rt_kprintf输出

rt_kprintf是RT-Thread里的打印函数，但是默认是空函数，需要重写`rt_weak void rt_hw_console_output(const char *str)`来设置默认打印。
把下面函数加到main.c里，编译时会自动覆盖源码的`__weak`同名函数。

```
void rt_hw_console_output(const char *str)
{
    PRINTK("%s", str);
}

```

## 5.6 编译 烧录 运行

参考 https://gitcode.com/HiSpark/fbb_ws63/blob/master/tools/README.md
把`.fwpkg`文件烧录到单板上后运行

## 5.7 运行结果

串口工具会按照测试用例正确打印，运行成功
```
[2026-03-17 18:46:03.110] get memory :1 byte
[2026-03-17 18:46:03.110] free memory :1 byte
[2026-03-17 18:46:03.110] get memory :2 byte
[2026-03-17 18:46:03.110] free memory :2 byte
[2026-03-17 18:46:03.110] get memory :4 byte
[2026-03-17 18:46:03.110] free memory :4 byte
[2026-03-17 18:46:03.110] get memory :8 byte
[2026-03-17 18:46:03.110] free memory :8 byte
[2026-03-17 18:46:03.110] get memory :16 byte
[2026-03-17 18:46:03.110] free memory :16 byte
[2026-03-17 18:46:03.110] get memory :32 byte
[2026-03-17 18:46:03.110] free memory :32 byte
[2026-03-17 18:46:03.110] get memory :64 byte
[2026-03-17 18:46:03.110] free memory :64 byte
[2026-03-17 18:46:03.110] get memory :128 byte
[2026-03-17 18:46:03.110] free memory :128 byte
[2026-03-17 18:46:03.110] get memory :256 byte
[2026-03-17 18:46:03.110] free memory :256 byte
[2026-03-17 18:46:03.110] get memory :512 byte
[2026-03-17 18:46:03.110] free memory :512 byte
[2026-03-17 18:46:03.110] get memory :1024 byte
[2026-03-17 18:46:03.110] free memory :1024 byte
[2026-03-17 18:46:03.110] get memory :2048 byte
[2026-03-17 18:46:03.110] free memory :2048 byte
[2026-03-17 18:46:03.110] get memory :4096 byte
[2026-03-17 18:46:03.110] free memory :4096 byte
[2026-03-17 18:46:03.110] get memory :8192 byte
[2026-03-17 18:46:03.110] free memory :8192 byte
[2026-03-17 18:46:03.110] get memory :16384 byte
[2026-03-17 18:46:03.110] free memory :16384 byte
[2026-03-17 18:46:03.110] get memory :32768 byte
[2026-03-17 18:46:03.110] free memory :32768 byte
[2026-03-17 18:46:03.110] get memory :65536 byte
[2026-03-17 18:46:03.110] free memory :65536 byte
[2026-03-17 18:46:03.110] get memory :131072 byte
[2026-03-17 18:46:03.110] free memory :131072 byte
[2026-03-17 18:46:03.110] get memory :262144 byte
[2026-03-17 18:46:03.110] free memory :262144 byte
[2026-03-17 18:46:03.110] try to get 524288 byte memory failed!`

