/*
 * debug_helper.c - 调试辅助代码
 * 添加到项目中用于验证MAX30102数据读取是否正常
 */

#include "../inc/MAX30102.h"
#include "../inc/blood.h"
#include "osal_debug.h"
#include "cmsis_os2.h"

/* 
 * 调试函数1：检查原始数据波动
 * 正常的手指脉搏数据应该有明显的周期性波动
 * 如果数值长时间不变或变化很小，说明读取时序有问题
 */
void debug_check_raw_data(void)
{
    uint32_t prev_red = 0, prev_ir = 0;
    int unchanged_count = 0;

    printf("\r\n=== 原始数据波动检查 ===\r\n");

    for (int i = 0; i < 20; i++) {
        max30102_read_fifo();

        int red_diff = (fifo_red > prev_red) ? (fifo_red - prev_red) : (prev_red - fifo_red);
        int ir_diff = (fifo_ir > prev_ir) ? (fifo_ir - prev_ir) : (prev_ir - fifo_ir);

        printf("Sample %2d: Red=%6lu (diff=%4d), IR=%6lu (diff=%4d)\r\n",
               i, fifo_red, red_diff, fifo_ir, ir_diff);

        if (red_diff < 100 && ir_diff < 100) {
            unchanged_count++;
        }

        prev_red = fifo_red;
        prev_ir = fifo_ir;
        osal_mdelay(10);  // 10ms采样间隔
    }

    printf("\r\n结果: %d/20 个样本变化很小", unchanged_count);
    if (unchanged_count > 10) {
        printf(" ⚠️ 警告：可能读到重复数据！\r\n");
    } else {
        printf(" ✓ 数据波动正常\r\n");
    }
}

/*
 * 调试函数2：测量实际采样间隔
 * 正常应该约10ms（100Hz）
 */
void debug_check_sample_interval(void)
{
    uint32_t timestamps[20];

    printf("\r\n=== 采样间隔检查 ===\r\n");

    for (int i = 0; i < 20; i++) {
        timestamps[i] = osal_get_jiffies();  // 或使用其他时间获取函数
        max30102_read_fifo();
        osal_mdelay(10);
    }

    printf("采样间隔(ms): ");
    for (int i = 1; i < 20; i++) {
        int interval = timestamps[i] - timestamps[i-1];
        printf("%d ", interval);
    }
    printf("\r\n");
}

/*
 * 调试函数3：检查去直流后的数据
 * 正常应该接近0，且有过零点
 */
void debug_check_dc_removed(void)
{
    int32_t sum = 0;
    int32_t data[100];

    printf("\r\n=== 去直流检查 ===\r\n");

    // 采集100个点
    for (int i = 0; i < 100; i++) {
        max30102_read_fifo();
        data[i] = fifo_ir;
        sum += fifo_ir;
        osal_mdelay(10);
    }

    int32_t mean = sum / 100;
    int zero_cross = 0;

    printf("IR均值: %ld\r\n", mean);
    printf("去直流后前20个点: ");

    for (int i = 0; i < 20; i++) {
        int32_t dc_removed = data[i] - mean;
        printf("%ld ", dc_removed);

        if (i > 0) {
            if ((data[i-1] - mean < 0 && dc_removed >= 0) ||
                (data[i-1] - mean >= 0 && dc_removed < 0)) {
                zero_cross++;
            }
        }
    }

    printf("\r\n过零次数: %d\r\n", zero_cross);
    if (zero_cross < 3) {
        printf("⚠️ 警告：过零次数太少，可能数据异常！\r\n");
    } else {
        printf("✓ 波形正常\r\n");
    }
}

/*
 * 调试函数4：对比修复前后的数据特征
 * 在blood_data_update()中添加此调试输出
 */
void debug_print_buffer_stats(void)
{
    uint32_t min_val = 0xFFFFFFFF, max_val = 0;
    uint64_t sum = 0;

    for (int i = 0; i < 500; i++) {
        if (s2[i] < min_val) min_val = s2[i];
        if (s2[i] > max_val) max_val = s2[i];
        sum += s2[i];
    }

    uint32_t mean = (uint32_t)(sum / 500);
    uint32_t peak_to_peak = max_val - min_val;

    printf("\r\n=== 缓冲区统计 ===\r\n");
    printf("IR最小值: %lu\r\n", min_val);
    printf("IR最大值: %lu\r\n", max_val);
    printf("IR均值:   %lu\r\n", mean);
    printf("峰峰值:   %lu\r\n", peak_to_peak);

    if (peak_to_peak < 5000) {
        printf("⚠️ 警告：峰峰值太小，信号弱或数据异常！\r\n");
    } else if (peak_to_peak > 50000) {
        printf("⚠️ 警告：峰峰值太大，可能饱和！\r\n");
    } else {
        printf("✓ 信号强度正常\r\n");
    }
}