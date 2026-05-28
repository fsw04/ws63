#include "../inc/blood.h"
#include "../inc/MAX30102.h"
#include "pinctrl.h"
#include "osal_debug.h"
#include "cmsis_os2.h"

int heart;
int32_t spo2;
int32_t heart0;
int32_t heart1;

/* 外部变量：uint32_t 匹配 18bit FIFO 解析 */
extern uint32_t fifo_red;
extern uint32_t fifo_ir;

uint16_t g_fft_index = 0;

uint32_t s1[500];
uint32_t s2[500];
uint32_t s12[500];
uint32_t s22[500];

int8_t XL;
int8_t XY;

#define CORRECTED_VALUE 47
#define IR_SIGNAL_THRESHOLD  1000   /* 最新100点红外均值阈值 */

/* ========== 输出端滤波缓存 ========== */
static int hr_history[3] = {-999, -999, -999};
static int hr_idx = 0;
static int finger_state = 0;  /* 0=无手指, 1=预热中, 2=稳定 */

/* ========== 3帧取中值 ========== */
static int median_3(int a, int b, int c)
{
    int t;
    if (a > b) { t = a; a = b; b = t; }
    if (b > c) { t = b; b = c; c = t; }
    if (a > b) { t = a; a = b; b = t; }
    return b;
}

void blood_data_update(void)
{
    printf("512 ================\r\n");
    g_fft_index = 0;
    while (g_fft_index < 500) {
        while (max30102_Read(REG_INTR_STATUS_1) & 0x40) {
            max30102_read_fifo();
            if (g_fft_index < 500) {
                s1[g_fft_index] = fifo_red;   /* s1: 红光 */
                s2[g_fft_index] = fifo_ir;    /* s2: 红外 */
                g_fft_index++;
            }
        }
    }
    printf("500 ok ================\r\n");
}

void blood_data(void)
{
    printf("jin ru2 ================\r\n");
    g_fft_index = 400;
    while (g_fft_index < 500) {
        while (max30102_Read(REG_INTR_STATUS_1) & 0x40) {
            max30102_read_fifo();
            if (g_fft_index < 500) {
                s1[g_fft_index] = fifo_red;
                s2[g_fft_index] = fifo_ir;
                g_fft_index++;
            }
        }
    }
    data_huan();
    printf("jin ru21 ================\r\n");
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
    int i;
    int s = 0;
    for (i = 100; i < 500; i++) {
        s12[s] = s1[i];
        s22[s] = s2[i];
        s++;
    }
}

void blood_Loop(void)
{
    printf("jin ru ================\r\n");
    blood_data();

    /* ========== 检测最新100个点（判断当前手指状态）========== */
    uint32_t ir_new_sum = 0;
    int i;
    for (i = 400; i < 500; i++) {
        ir_new_sum += s2[i];
    }

    if (ir_new_sum / 100 < IR_SIGNAL_THRESHOLD) {
        /* 无手指：清空所有状态 */
        finger_state = 0;
        heart = -999;
        spo2 = -999;
        heart0 = -999;
        hr_history[0] = hr_history[1] = hr_history[2] = -999;
        hr_idx = 0;
        printf("无信号/手指未放置\r\n");
    } else {
        /* 有手指：调用算法 */
        maxim_heart_rate_and_oxygen_saturation(s2, 500, s1,
                                               &spo2, &XY, &heart, &XL);

        if (heart >= 30 && heart <= 220) {
            /* 存入历史 */
            hr_history[hr_idx] = heart;
            hr_idx = (hr_idx + 1) % 3;

            if (hr_history[0] >= 30 && hr_history[1] >= 30 && hr_history[2] >= 30) {
                /* ========== 3帧都有效：中值滤波 + 跳变保护 ========== */
                int hr_median = median_3(hr_history[0], hr_history[1], hr_history[2]);

                if (heart0 >= 30) {
                    int diff = hr_median - heart0;
                    if (diff < 0) diff = -diff;

                    if (diff > 30) {
                        /* 跳变过大，丢弃，保持旧值 */
                        heart = heart0;
                    } else {
                        /* 正常更新 */
                        heart = hr_median;
                        heart0 = hr_median;
                    }
                } else {
                    /* 第一组有效值 */
                    heart = hr_median;
                    heart0 = hr_median;
                }
                finger_state = 2;  /* 标记稳定 */
            } else {
                /* ========== 缓存未满3帧：预热期 ========== */
                if (heart0 >= 30) {
                    int diff = heart - heart0;
                    if (diff < 0) diff = -diff;

                    if (diff > 30) {
                        heart = heart0;  /* 预热期也做跳变保护 */
                    } else {
                        heart0 = heart;
                    }
                } else {
                    heart0 = heart;  /* 第一帧 */
                }
            }
        } else {
            /* 算法返回无效 */
            if (finger_state == 2) {
                heart = heart0;  /* 稳定期用旧值兜底 */
            } else {
                heart = -999;
            }
        }
    }

    printf("心率%3d/min; 血氧%2d\r\n", heart, spo2);
    data_clean();
}

/* ========== 以下 Maxim 算法（已改16点MA） ========== */
const uint16_t auw_hamm[31] = {41, 276, 512, 276, 41};

const uint8_t uch_spo2_table[184] = {
    95, 95, 95, 96, 96, 96, 97, 97, 97, 97, 97, 98, 98, 98, 98, 98, 99, 99, 99, 99,
    99, 99, 99, 99, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
    100, 100, 100, 100, 99, 99, 99, 99, 99, 99, 99, 99, 98, 98, 98, 98, 98, 98, 97, 97,
    97, 97, 96, 96, 96, 96, 95, 95, 95, 94, 94, 94, 93, 93, 93, 92, 92, 92, 91, 91,
    90, 90, 89, 89, 89, 88, 88, 87, 87, 86, 86, 85, 85, 84, 84, 83, 82, 82, 81, 81,
    80, 80, 79, 78, 78, 77, 76, 76, 75, 74, 74, 73, 72, 72, 71, 70, 69, 69, 68, 67,
    66, 66, 65, 64, 63, 62, 62, 61, 60, 59, 58, 57, 56, 56, 55, 54, 53, 52, 51, 50,
    49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 31, 30, 29,
    28, 27, 26, 25, 23, 22, 21, 20, 19, 17, 16, 15, 14, 12, 11, 10, 9, 7, 6, 5,
    3, 2, 1
};

static int32_t an_dx[BUFFER_SIZE - 4];  /* 原MA4_SIZE=4，现16，但BUFFER_SIZE-16足够 */
static int32_t an_x[BUFFER_SIZE];
static int32_t an_y[BUFFER_SIZE];

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

    /* 1. 去 DC */
    un_ir_mean = 0;
    for (k = 0; k < n_ir_buffer_length; k++) un_ir_mean += pun_ir_buffer[k];
    un_ir_mean = un_ir_mean / n_ir_buffer_length;
    for (k = 0; k < n_ir_buffer_length; k++) an_x[k] = pun_ir_buffer[k] - un_ir_mean;

    /* ========== 方案B：16点滑动平均（替代原4点MA）========== */
    for (k = 0; k < BUFFER_SIZE - 16; k++) {
        int32_t sum = 0;
        int m;
        for (m = 0; m < 16; m++) sum += an_x[k + m];
        an_x[k] = sum / 16;
    }

    /* 2. 差分（范围相应调整） */
    for (k = 0; k < BUFFER_SIZE - 16 - 1; k++)
        an_dx[k] = (an_x[k + 1] - an_x[k]);

    /* 3. 2点MA */
    for (k = 0; k < BUFFER_SIZE - 16 - 2; k++) {
        an_dx[k] = (an_dx[k] + an_dx[k + 1]) / 2;
    }

    /* 4. Hamming窗（范围调整） */
    for (i = 0; i < BUFFER_SIZE - HAMMING_SIZE - 16 - 2; i++) {
        s = 0;
        for (k = i; k < i + HAMMING_SIZE; k++) {
            s -= an_dx[k] * auw_hamm[k - i];
        }
        an_dx[i] = s / (int32_t)1146;
    }

    /* 5. 阈值计算（范围调整） */
    n_th1 = 0;
    for (k = 0; k < BUFFER_SIZE - HAMMING_SIZE - 16; k++) {
        n_th1 += ((an_dx[k] > 0) ? an_dx[k] : ((int32_t)0 - an_dx[k]));
    }
    n_th1 = n_th1 / (BUFFER_SIZE - HAMMING_SIZE - 16);

    /* 6. 峰值检测（范围调整） */
    maxim_find_peaks(an_dx_peak_locs, &n_npks, an_dx,
                     BUFFER_SIZE - HAMMING_SIZE - 16, n_th1, 8, 5);

    /* 7. 心率计算 */
    n_peak_interval_sum = 0;
    if (n_npks >= 2) {
        for (k = 1; k < n_npks; k++)
            n_peak_interval_sum += (an_dx_peak_locs[k] - an_dx_peak_locs[k - 1]);
        n_peak_interval_sum = n_peak_interval_sum / (n_npks - 1);
        *pn_heart_rate = (int32_t)(6000 / n_peak_interval_sum);
        *pch_hr_valid = 1;
    } else {
        *pn_heart_rate = -999;
        *pch_hr_valid = 0;
    }

    for (k = 0; k < n_npks; k++)
        an_ir_valley_locs[k] = an_dx_peak_locs[k] + HAMMING_SIZE / 2;

    /* 8. 还原原始 Red/IR 用于血氧（保留4点MA，血氧不需要太强平滑） */
    for (k = 0; k < n_ir_buffer_length; k++) {
        an_x[k] = pun_ir_buffer[k];
        an_y[k] = pun_red_buffer[k];
    }

    for (k = 0; k < BUFFER_SIZE - MA4_SIZE; k++) {
        an_x[k] = (an_x[k] + an_x[k + 1] + an_x[k + 2] + an_x[k + 3]) / (int32_t)4;
        an_y[k] = (an_y[k] + an_y[k + 1] + an_y[k + 2] + an_y[k + 3]) / (int32_t)4;
    }

    /* ========== 以下血氧计算完全不变 ========== */
    n_exact_ir_valley_locs_count = 0;
    for (k = 0; k < n_npks; k++) {
        un_only_once = 1;
        m = an_ir_valley_locs[k];
        n_c_min = 16777216;
        if (m + 5 < BUFFER_SIZE - HAMMING_SIZE && m - 5 > 0) {
            for (i = m - 5; i < m + 5; i++)
                if (an_x[i] < n_c_min) {
                    if (un_only_once > 0) {
                        un_only_once = 0;
                    }
                    n_c_min = an_x[i];
                    an_exact_ir_valley_locs[k] = i;
                }
            if (un_only_once == 0)
                n_exact_ir_valley_locs_count++;
        }
    }
    if (n_exact_ir_valley_locs_count < 2) {
        *pn_spo2 = -999;
        *pch_spo2_valid = 0;
        return;
    }

    for (k = 0; k < BUFFER_SIZE - MA4_SIZE; k++) {
        an_x[k] = (an_x[k] + an_x[k + 1] + an_x[k + 2] + an_x[k + 3]) / (int32_t)4;
        an_y[k] = (an_y[k] + an_y[k + 1] + an_y[k + 2] + an_y[k + 3]) / (int32_t)4;
    }

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
            n_y_ac = an_y[an_exact_ir_valley_locs[k]] + n_y_ac / (an_exact_ir_valley_locs[k + 1] - an_exact_ir_valley_locs[k]);
            n_y_ac = an_y[n_y_dc_max_idx] - n_y_ac;

            n_x_ac = (an_x[an_exact_ir_valley_locs[k + 1]] - an_x[an_exact_ir_valley_locs[k]])
                   * (n_x_dc_max_idx - an_exact_ir_valley_locs[k]);
            n_x_ac = an_x[an_exact_ir_valley_locs[k]] + n_x_ac / (an_exact_ir_valley_locs[k + 1] - an_exact_ir_valley_locs[k]);
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

    if (n_middle_idx > 1)
        n_ratio_average = (an_ratio[n_middle_idx - 1] + an_ratio[n_middle_idx]) / 2;
    else
        n_ratio_average = an_ratio[n_middle_idx];

    if (n_ratio_average > 2 && n_ratio_average < 184) {
        n_spo2_calc = uch_spo2_table[n_ratio_average];
        *pn_spo2 = n_spo2_calc;
        *pch_spo2_valid = 1;
    } else {
        *pn_spo2 = -999;
        *pch_spo2_valid = 0;
    }
}

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