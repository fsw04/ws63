#ifndef _MAX30102_H
#define _MAX30102_H

#include <stdint.h>
#include <stdbool.h>

/* I2C 配置 (WS63平台) */
#define MAX30102_ADDR           0x57
#define MAX30102_I2C_BUS        1

/* 寄存器地址 */
#define REG_INTR_STATUS_1       0x00
#define REG_INTR_STATUS_2       0x01
#define REG_INTR_ENABLE_1       0x02
#define REG_INTR_ENABLE_2       0x03
#define REG_FIFO_WR_PTR         0x04
#define REG_OVF_COUNTER         0x05
#define REG_FIFO_RD_PTR         0x06
#define REG_FIFO_DATA           0x07
#define REG_FIFO_CONFIG         0x08
#define REG_MODE_CONFIG         0x09
#define REG_SPO2_CONFIG         0x0A
#define REG_LED1_PA             0x0C
#define REG_LED2_PA             0x0D
#define REG_PILOT_PA            0x10
#define REG_MULTI_LED_CTRL1     0x11
#define REG_MULTI_LED_CTRL2     0x12
#define REG_TEMP_INTR           0x1F
#define REG_TEMP_FRAC           0x20
#define REG_TEMP_CONFIG         0x21
#define REG_PROX_INT_THRESH     0x30
#define REG_REV_ID              0xFE
#define REG_PART_ID             0xFF

/* ========== STM32算法移植: 常量和查表 ========== */
#define FS              100
#define BUFFER_SIZE     (FS * 5)    /* 500点 = 5秒 */
#define MA4_SIZE        4           /* DO NOT CHANGE */
#define HAMMING_SIZE    5           /* DO NOT CHANGE */
#define HR_FIFO_SIZE    7

#ifndef min
#define min(x, y) ((x) < (y) ? (x) : (y))
#endif

/* 汉明窗系数 = long16(512 * hamming(5)') */
static const uint16_t auw_hamm[31] = { 41, 276, 512, 276, 41 };

/* uch_spo2_table 通过公式计算：-45.060*ratioAverage*ratioAverage + 30.354*ratioAverage + 94.845 */
static const uint8_t uch_spo2_table[184] = {
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

/* 函数声明 */
uint8_t max30102_write_reg(uint8_t addr, uint8_t data);
uint8_t max30102_read_reg(uint8_t addr);
void max30102_read_fifo(void);
void MAX30102_Config(void);
void Max30102_reset(void);

/* 外部变量 */
extern uint32_t fifo_red;
extern uint32_t fifo_ir;

/* ========== STM32算法移植: 算法函数声明 ========== */
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