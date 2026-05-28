#include "app_init.h"
#include "soc_osal.h"
#include "common_def.h"
#include "max30102.h"

static void max30102_app_entry(void)
{
    osal_printk("\r\n================================\r\n");
    osal_printk("   MAX30102 Start!      \r\n");
    osal_printk("================================\r\n\n");

    /* 先等系统完全启动，tick 中断就绪 */
    /* 用粗略轮询等几秒，不占用 CPU（让出调度） */
    int i;
    for (i = 0; i < 50; i++) {
        osal_printk("MAX30102 wait %d\r\n", i);
        /* 不 sleep，直接让其他任务跑 */
    }

    max30102_task_start();
}

app_run(max30102_app_entry);