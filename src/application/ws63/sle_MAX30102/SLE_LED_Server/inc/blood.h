#ifndef _BLOOD_H
#define _BLOOD_H

#include <stdint.h>
#include "../inc/MAX30102.h"

/* 算法参数（与 STM32 移植版本一致） */
#define FS              100
#define BUFFER_SIZE     (FS * 5)    /* 500点 = 5秒 */
#define MA4_SIZE        4           /* DO NOT CHANGE */
#define HAMMING_SIZE    5           /* DO NOT CHANGE */
#define HR_FIFO_SIZE    7

#ifndef min
#define min(x, y) ((x) < (y) ? (x) : (y))
#endif

/* 手指检测与输出滤波 */
extern int heart;
extern int32_t spo2;
extern int32_t heart0;
extern int finger_state;    /* 0=无手指, 1=预热中, 2=稳定 */

/* 数据采集与主循环 */
void blood_data_update(void);
void blood_data(void);
void data_clean(void);
void data_huan(void);
void blood_Loop(void);

/* Maxim 官方算法函数（STM32移植） */
void maxim_heart_rate_and_oxygen_saturation(uint32_t *pun_ir_buffer,
                                            int32_t n_ir_buffer_length,
                                            uint32_t *pun_red_buffer,
                                            int32_t *pn_spo2,
                                            int8_t *pch_spo2_valid,
                                            int32_t *pn_heart_rate,
                                            int8_t *pch_hr_valid);

void maxim_find_peaks(int32_t *pn_locs, int32_t *pn_npks, int32_t *pn_x,
                      int32_t n_size, int32_t n_min_height,
                      int32_t n_min_distance, int32_t n_max_num);

void maxim_peaks_above_min_height(int32_t *pn_locs, int32_t *pn_npks,
                                  int32_t *pn_x, int32_t n_size,
                                  int32_t n_min_height);

void maxim_remove_close_peaks(int32_t *pn_locs, int32_t *pn_npks,
                              int32_t *pn_x, int32_t n_min_distance);

void maxim_sort_ascend(int32_t *pn_x, int32_t n_size);
void maxim_sort_indices_descend(int32_t *pn_x, int32_t *pn_indx, int32_t n_size);

#endif