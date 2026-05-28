#include "../inc/blood.h"
#include "../inc/MAX30102.h"
#include "pinctrl.h"
#include "osal_debug.h"
#include "cmsis_os2.h"
#include "soc_osal.h"

/* ========== 全局变量 ========== */
int heart = -999;
int32_t spo2 = -999;
int32_t heart0 = -999;
int32_t heart1 = -999;

/* 外部 FIFO 数据（定义在 MAX30102.c） */
extern uint32_t fifo_red;
extern uint32_t fifo_ir;

uint16_t g_fft_index = 0;

/* 数据缓冲区（滑动窗口） */
uint32_t s1[500];
uint32_t s2[500];
uint32_t s12[500];
uint32_t s22[500];

int8_t XL = 0;
int8_t XY = 0;

/* 手指检测参数 */
#define IR_SIGNAL_THRESHOLD_LOW     8000
#define IR_SIGNAL_THRESHOLD_HIGH    15000
#define FINGER_CONFIRM_FRAMES       3

int finger_state = 0;               /* 0=无手指, 1=预热中, 2=稳定 */
static int finger_confirm_cnt = 0;
static int hr_history[3] = {-999, -999, -999};
static int hr_idx = 0;

/* ========== STM32算法移植: 静态工作区 ========== */
static int32_t an_dx[BUFFER_SIZE - MA4_SIZE];
static int32_t an_x[BUFFER_SIZE];
static int32_t an_y[BUFFER_SIZE];

/* ========== 工具函数 ========== */
static int median_3(int a, int b, int c)
{
    int t;
    if (a > b) { t = a; a = b; b = t; }
    if (b > c) { t = b; b = c; c = t; }
    if (a > b) { t = a; a = b; b = t; }
    return b;
}

/* ========== 数据采集 ========== */

/**
 * @brief 【修复】采集500点数据
 *        STM32在100Hz采样率下，每10ms产生一个新样本
 *        原代码使用while循环快速读取，可能导致读到重复数据
 *        修复：使用定时采样，每10ms读取一次
 */
void blood_data_update(void)
{
    printf("512 ================\r\n");
    g_fft_index = 0;

    /* 【修复】使用定时采样替代中断轮询
     * MAX30102在100Hz下每10ms产生一个新样本
     * 每10ms检查一次中断状态，有新数据则读取
     */
    while (g_fft_index < 500) {
        uint8_t intr_status = max30102_read_reg(REG_INTR_STATUS_1);
        if (intr_status & 0x40) {  /* PPG_RDY 中断 */
            max30102_read_fifo();
            if (g_fft_index < 500) {
                s1[g_fft_index] = fifo_red;
                s2[g_fft_index] = fifo_ir;
                g_fft_index++;
            }
        }
        /* 【修复】等待下一个采样周期，避免读取过快 */
        osal_mdelay(5);  /* 5ms轮询一次，确保不遗漏样本 */
    }
    printf("500 ok ================\r\n");
}

/**
 * @brief 【修复】滑动窗口更新：保留400旧点，采集100新点
 */
void blood_data(void)
{
    g_fft_index = 400;

    while (g_fft_index < 500) {
        uint8_t intr_status = max30102_read_reg(REG_INTR_STATUS_1);
        if (intr_status & 0x40) {  /* PPG_RDY 中断 */
            max30102_read_fifo();
            if (g_fft_index < 500) {
                s1[g_fft_index] = fifo_red;
                s2[g_fft_index] = fifo_ir;
                g_fft_index++;
            }
        }
        /* 【修复】等待下一个采样周期 */
        osal_mdelay(5);
    }
    data_huan();
}

void data_clean(void)
{
    int i;
    for (i = 0; i < 400; i++) {
        s1[i] = s12[i];
        s2[i] = s22[i];
    }
}

void data_huan(void)
{
    int i, s = 0;
    for (i = 100; i < 500; i++) {
        s12[s] = s1[i];
        s22[s] = s2[i];
        s++;
    }
}

/* ========== 主循环：手指检测 + STM32算法 + 输出滤波 ========== */
void blood_Loop(void)
{
    int i;
    uint32_t ir_new_sum = 0;
    uint32_t ir_avg = 0;

    printf("jin ru ================\r\n");
    blood_data();

    /* 检测最新 100 个点的红外均值 */
    for (i = 400; i < 500; i++) {
        ir_new_sum += s2[i];
    }
    ir_avg = ir_new_sum / 100;

    /* ---------- 手指检测状态机 ---------- */
    if (ir_avg < IR_SIGNAL_THRESHOLD_LOW) {
        /* 完全无手指 */
        finger_state = 0;
        finger_confirm_cnt = 0;
        heart = -999;
        spo2 = -999;
        heart0 = -999;
        hr_history[0] = hr_history[1] = hr_history[2] = -999;
        hr_idx = 0;
        printf("无手指 ir_avg=%lu\r\n", ir_avg);
    }
    else if (ir_avg < IR_SIGNAL_THRESHOLD_HIGH) {
        /* 信号弱：手指刚放上或接触不良 */
        if (finger_state == 0) {
            finger_state = 1;
            heart = -999;
            spo2 = -999;
            printf("预热中 ir_avg=%lu\r\n", ir_avg);
        }
    }
    else {
        /* 信号强 */
        finger_confirm_cnt++;
        if (finger_confirm_cnt >= FINGER_CONFIRM_FRAMES || finger_state == 2) {
            finger_state = 2;
            if (finger_confirm_cnt > FINGER_CONFIRM_FRAMES) {
                finger_confirm_cnt = FINGER_CONFIRM_FRAMES;
            }

            /* 调用 STM32 移植的 Maxim 官方算法 */
            maxim_heart_rate_and_oxygen_saturation(s2, 500, s1,
                                                   &spo2, &XY, &heart, &XL);

            if (heart >= 30 && heart <= 220) {
                /* 存入历史 */
                hr_history[hr_idx] = heart;
                hr_idx = (hr_idx + 1) % 3;

                if (hr_history[0] >= 30 && hr_history[1] >= 30 && hr_history[2] >= 30) {
                    /* 3 帧都有效：中值滤波 + 跳变保护 */
                    int hr_median = median_3(hr_history[0], hr_history[1], hr_history[2]);

                    if (heart0 >= 30) {
                        int diff = hr_median - heart0;
                        if (diff < 0) diff = -diff;

                        if (diff > 30) {
                            printf("跳变保护: %d -> %d, 保持 %d\r\n",
                                   heart0, hr_median, heart0);
                            heart = heart0;
                        } else {
                            heart = hr_median;
                            heart0 = hr_median;
                        }
                    } else {
                        heart = hr_median;
                        heart0 = hr_median;
                    }
                } else {
                    /* 预热期：直接输出，快速收敛 */
                    heart0 = heart;
                    printf("预热输出: %d BPM (缓存 %d/3)\r\n", heart, hr_idx);
                }
            } else {
                /* 算法返回无效 */
                if (finger_state == 2 && heart0 >= 30) {
                    heart = heart0;
                    printf("算法无效，保持旧值 %d\r\n", heart0);
                } else {
                    heart = -999;
                    printf("算法无效，无旧值\r\n");
                }
            }
        } else {
            heart = -999;
            spo2 = -999;
            printf("确认中 %d/3 ir_avg=%lu\r\n", finger_confirm_cnt, ir_avg);
        }
    }

    printf("心率%3d/min; 血氧%2d\r\n", heart, spo2);
    data_clean();
}

/* ============================================================================
 * 以下 STM32 移植的 Maxim 官方算法实现
 * ============================================================================ */

void maxim_heart_rate_and_oxygen_saturation(uint32_t *pun_ir_buffer,
                                            int32_t n_ir_buffer_length,
                                            uint32_t *pun_red_buffer,
                                            int32_t *pn_spo2,
                                            int8_t *pch_spo2_valid,
                                            int32_t *pn_heart_rate,
                                            int8_t *pch_hr_valid)
{
    uint32_t un_ir_mean, un_only_once;
    int32_t k, n_i_ratio_count;
    int32_t i, s, m, n_exact_ir_valley_locs_count, n_middle_idx;
    int32_t n_th1, n_npks, n_c_min;
    int32_t an_ir_valley_locs[15];
    int32_t an_exact_ir_valley_locs[15];
    int32_t an_dx_peak_locs[15];
    int32_t n_peak_interval_sum;

    int32_t n_y_ac, n_x_ac;
    int32_t n_spo2_calc;
    int32_t n_y_dc_max, n_x_dc_max;
    int32_t n_y_dc_max_idx = 0;
    int32_t n_x_dc_max_idx = 0;
    int32_t an_ratio[5], n_ratio_average;
    int32_t n_nume, n_denom;

    /* 1. 去直流 */
    un_ir_mean = 0;
    for (k = 0; k < n_ir_buffer_length; k++) {
        un_ir_mean += pun_ir_buffer[k];
    }
    un_ir_mean = un_ir_mean / n_ir_buffer_length;
    for (k = 0; k < n_ir_buffer_length; k++) {
        an_x[k] = (int32_t)pun_ir_buffer[k] - (int32_t)un_ir_mean;
    }

    /* 2. 4点滑动平均（官方固定值，DO NOT CHANGE） */
    for (k = 0; k < BUFFER_SIZE - MA4_SIZE; k++) {
        n_denom = (an_x[k] + an_x[k + 1] + an_x[k + 2] + an_x[k + 3]);
        an_x[k] = n_denom / (int32_t)4;
    }

    /* 3. 差分 */
    for (k = 0; k < BUFFER_SIZE - MA4_SIZE - 1; k++) {
        an_dx[k] = (an_x[k + 1] - an_x[k]);
    }

    /* 4. 2点移动平均 */
    for (k = 0; k < BUFFER_SIZE - MA4_SIZE - 2; k++) {
        an_dx[k] = (an_dx[k] + an_dx[k + 1]) / 2;
    }

    /* 5. Hamming窗 + 翻转波形 */
    for (i = 0; i < BUFFER_SIZE - HAMMING_SIZE - MA4_SIZE - 2; i++) {
        s = 0;
        for (k = i; k < i + HAMMING_SIZE; k++) {
            s -= an_dx[k] * auw_hamm[k - i];
        }
        an_dx[i] = s / (int32_t)1146;
    }

    /* 6. 自适应阈值 */
    n_th1 = 0;
    for (k = 0; k < BUFFER_SIZE - HAMMING_SIZE; k++) {
        n_th1 += ((an_dx[k] > 0) ? an_dx[k] : ((int32_t)0 - an_dx[k]));
    }
    n_th1 = n_th1 / (BUFFER_SIZE - HAMMING_SIZE);

    /* 7. 峰值检测（找波谷）—— 官方参数：间距=8，最大峰值=5 */
    maxim_find_peaks(an_dx_peak_locs, &n_npks, an_dx,
                     BUFFER_SIZE - HAMMING_SIZE, n_th1, 8, 5);

    /* 8. 心率计算 */
    n_peak_interval_sum = 0;
    if (n_npks >= 2) {
        for (k = 1; k < n_npks; k++) {
            n_peak_interval_sum += (an_dx_peak_locs[k] - an_dx_peak_locs[k - 1]);
        }
        n_peak_interval_sum = n_peak_interval_sum / (n_npks - 1);
        *pn_heart_rate = (int32_t)(6000 / n_peak_interval_sum);
        *pch_hr_valid = 1;
    } else {
        *pn_heart_rate = -999;
        *pch_hr_valid = 0;
    }

    for (k = 0; k < n_npks; k++) {
        an_ir_valley_locs[k] = an_dx_peak_locs[k] + HAMMING_SIZE / 2;
    }

    /* 9. 还原原始 Red/IR 用于血氧 */
    for (k = 0; k < n_ir_buffer_length; k++) {
        an_x[k] = (int32_t)pun_ir_buffer[k];
        an_y[k] = (int32_t)pun_red_buffer[k];
    }

    for (k = 0; k < BUFFER_SIZE - MA4_SIZE; k++) {
        an_x[k] = (an_x[k] + an_x[k + 1] + an_x[k + 2] + an_x[k + 3]) / (int32_t)4;
        an_y[k] = (an_y[k] + an_y[k + 1] + an_y[k + 2] + an_y[k + 3]) / (int32_t)4;
    }

    /* 10. 精确波谷定位 */
    n_exact_ir_valley_locs_count = 0;
    for (k = 0; k < n_npks; k++) {
        un_only_once = 1;
        m = an_ir_valley_locs[k];
        n_c_min = 16777216;
        if (m + 5 < BUFFER_SIZE - HAMMING_SIZE && m - 5 > 0) {
            for (i = m - 5; i < m + 5; i++) {
                if (an_x[i] < n_c_min) {
                    if (un_only_once > 0) {
                        un_only_once = 0;
                    }
                    n_c_min = an_x[i];
                    an_exact_ir_valley_locs[k] = i;
                }
            }
            if (un_only_once == 0) {
                n_exact_ir_valley_locs_count++;
            }
        }
    }

    if (n_exact_ir_valley_locs_count < 2) {
        *pn_spo2 = -999;
        *pch_spo2_valid = 0;
        return;
    }

    /* 11. 4点MA再次平滑（血氧专用） */
    for (k = 0; k < BUFFER_SIZE - MA4_SIZE; k++) {
        an_x[k] = (an_x[k] + an_x[k + 1] + an_x[k + 2] + an_x[k + 3]) / (int32_t)4;
        an_y[k] = (an_y[k] + an_y[k + 1] + an_y[k + 2] + an_y[k + 3]) / (int32_t)4;
    }

    /* 12. 血氧计算（AC/DC 比值查表法） */
    n_ratio_average = 0;
    n_i_ratio_count = 0;
    for (k = 0; k < 5; k++) an_ratio[k] = 0;

    for (k = 0; k < n_exact_ir_valley_locs_count; k++) {
        if (an_exact_ir_valley_locs[k] > BUFFER_SIZE) {
            *pn_spo2 = -999;
            *pch_spo2_valid = 0;
            return;
        }
    }

    for (k = 0; k < n_exact_ir_valley_locs_count - 1; k++) {
        n_y_dc_max = -16777216;
        n_x_dc_max = -16777216;
        if (an_exact_ir_valley_locs[k + 1] - an_exact_ir_valley_locs[k] > 10) {
            for (i = an_exact_ir_valley_locs[k]; i < an_exact_ir_valley_locs[k + 1]; i++) {
                if (an_x[i] > n_x_dc_max) {
                    n_x_dc_max = an_x[i];
                    n_x_dc_max_idx = i;
                }
                if (an_y[i] > n_y_dc_max) {
                    n_y_dc_max = an_y[i];
                    n_y_dc_max_idx = i;
                }
            }

            n_y_ac = (an_y[an_exact_ir_valley_locs[k + 1]] - an_y[an_exact_ir_valley_locs[k]])
                   * (n_y_dc_max_idx - an_exact_ir_valley_locs[k]);
            n_y_ac = an_y[an_exact_ir_valley_locs[k]] + n_y_ac /
                     (an_exact_ir_valley_locs[k + 1] - an_exact_ir_valley_locs[k]);
            n_y_ac = an_y[n_y_dc_max_idx] - n_y_ac;

            n_x_ac = (an_x[an_exact_ir_valley_locs[k + 1]] - an_x[an_exact_ir_valley_locs[k]])
                   * (n_x_dc_max_idx - an_exact_ir_valley_locs[k]);
            n_x_ac = an_x[an_exact_ir_valley_locs[k]] + n_x_ac /
                     (an_exact_ir_valley_locs[k + 1] - an_exact_ir_valley_locs[k]);
            n_x_ac = an_x[n_y_dc_max_idx] - n_x_ac;

            n_nume = (n_y_ac * n_x_dc_max) >> 7;
            n_denom = (n_x_ac * n_y_dc_max) >> 7;
            if (n_denom > 0 && n_i_ratio_count < 5 && n_nume != 0) {
                an_ratio[n_i_ratio_count] = (n_nume * 20) / n_denom;
                n_i_ratio_count++;
            }
        }
    }

    maxim_sort_ascend(an_ratio, n_i_ratio_count);
    n_middle_idx = n_i_ratio_count / 2;

    if (n_middle_idx > 1) {
        n_ratio_average = (an_ratio[n_middle_idx - 1] + an_ratio[n_middle_idx]) / 2;
    } else {
        n_ratio_average = an_ratio[n_middle_idx];
    }

    if (n_ratio_average > 2 && n_ratio_average < 184) {
        n_spo2_calc = uch_spo2_table[n_ratio_average];
        *pn_spo2 = n_spo2_calc;
        *pch_spo2_valid = 1;
    } else {
        *pn_spo2 = -999;
        *pch_spo2_valid = 0;
    }
}

/* ========== 峰值检测辅助函数（STM32移植） ========== */
void maxim_sort_ascend(int32_t *pn_x, int32_t n_size)
{
    int32_t i, j, n_temp;
    for (i = 1; i < n_size; i++) {
        n_temp = pn_x[i];
        for (j = i; j > 0 && n_temp < pn_x[j - 1]; j--)
            pn_x[j] = pn_x[j - 1];
        pn_x[j] = n_temp;
    }
}

void maxim_find_peaks(int32_t *pn_locs, int32_t *pn_npks, int32_t *pn_x,
                      int32_t n_size, int32_t n_min_height,
                      int32_t n_min_distance, int32_t n_max_num)
{
    maxim_peaks_above_min_height(pn_locs, pn_npks, pn_x, n_size, n_min_height);
    maxim_remove_close_peaks(pn_locs, pn_npks, pn_x, n_min_distance);
    *pn_npks = min(*pn_npks, n_max_num);
}

void maxim_peaks_above_min_height(int32_t *pn_locs, int32_t *pn_npks,
                                  int32_t *pn_x, int32_t n_size,
                                  int32_t n_min_height)
{
    int32_t i = 1, n_width;
    *pn_npks = 0;

    while (i < n_size - 1) {
        if (pn_x[i] > n_min_height && pn_x[i] > pn_x[i - 1]) {
            n_width = 1;
            while (i + n_width < n_size && pn_x[i] == pn_x[i + n_width])
                n_width++;
            if (pn_x[i] > pn_x[i + n_width] && (*pn_npks) < 15) {
                pn_locs[(*pn_npks)++] = i;
                i += n_width + 1;
            } else {
                i += n_width;
            }
        } else {
            i++;
        }
    }
}

void maxim_remove_close_peaks(int32_t *pn_locs, int32_t *pn_npks,
                              int32_t *pn_x, int32_t n_min_distance)
{
    int32_t i, j, n_old_npks, n_dist;

    maxim_sort_indices_descend(pn_x, pn_locs, *pn_npks);

    for (i = -1; i < *pn_npks; i++) {
        n_old_npks = *pn_npks;
        *pn_npks = i + 1;
        for (j = i + 1; j < n_old_npks; j++) {
            n_dist = pn_locs[j] - (i == -1 ? -1 : pn_locs[i]);
            if (n_dist > n_min_distance || n_dist < -n_min_distance)
                pn_locs[(*pn_npks)++] = pn_locs[j];
        }
    }
    maxim_sort_ascend(pn_locs, *pn_npks);
}

void maxim_sort_indices_descend(int32_t *pn_x, int32_t *pn_indx, int32_t n_size)
{
    int32_t i, j, n_temp;
    for (i = 1; i < n_size; i++) {
        n_temp = pn_indx[i];
        for (j = i; j > 0 && pn_x[n_temp] > pn_x[pn_indx[j - 1]]; j--)
            pn_indx[j] = pn_indx[j - 1];
        pn_indx[j] = n_temp;
    }
}