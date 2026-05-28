#ifndef __MAX30102_H__
#define __MAX30102_H__

#include <stdint.h>
#include <stdbool.h>

#define MAX30102_I2C_ADDR           0x57
#define MAX30102_I2C_BUS            0

/* 寄存器定义 */
#define MAX30102_REG_INT_STATUS1    0x00
#define MAX30102_REG_INT_STATUS2    0x01
#define MAX30102_REG_INT_ENABLE1    0x02
#define MAX30102_REG_INT_ENABLE2    0x03
#define MAX30102_REG_FIFO_WR_PTR    0x04
#define MAX30102_REG_OVF_COUNTER    0x05
#define MAX30102_REG_FIFO_RD_PTR    0x06
#define MAX30102_REG_FIFO_DATA      0x07
#define MAX30102_REG_FIFO_CONFIG    0x08
#define MAX30102_REG_MODE_CONFIG    0x09
#define MAX30102_REG_SPO2_CONFIG    0x0A
#define MAX30102_REG_LED1_PA        0x0C
#define MAX30102_REG_LED2_PA        0x0D
#define MAX30102_REG_MULTI_LED      0x11
#define MAX30102_REG_TEMP_INT       0x1F
#define MAX30102_REG_TEMP_FRAC      0x20
#define MAX30102_REG_TEMP_CONFIG    0x21
#define MAX30102_REG_PROX_INT_THRESH 0x30
#define MAX30102_REG_REV_ID         0xFE
#define MAX30102_REG_PART_ID        0xFF

#define MAX30102_MODE_HRONLY        0x02
#define MAX30102_MODE_SPO2          0x03

#define MAX30102_SAMPRATE_50HZ      0x00
#define MAX30102_SAMPRATE_100HZ     0x01
#define MAX30102_SAMPRATE_200HZ     0x02
#define MAX30102_SAMPRATE_400HZ     0x03
#define MAX30102_SAMPRATE_800HZ     0x04
#define MAX30102_SAMPRATE_1000HZ    0x05
#define MAX30102_SAMPRATE_1600HZ    0x06
#define MAX30102_SAMPRATE_3200HZ    0x07

#define MAX30102_PULSE_WIDTH_69US   0x00
#define MAX30102_PULSE_WIDTH_118US  0x01
#define MAX30102_PULSE_WIDTH_215US  0x02
#define MAX30102_PULSE_WIDTH_411US  0x03

#define MAX30102_ADC_RANGE_2048     0x00
#define MAX30102_ADC_RANGE_4096     0x01
#define MAX30102_ADC_RANGE_8192     0x02
#define MAX30102_ADC_RANGE_16384    0x03

#define MAX30102_LED_CURRENT_0MA    0x00
#define MAX30102_LED_CURRENT_3_1MA  0x0F
#define MAX30102_LED_CURRENT_6_2MA  0x1F
#define MAX30102_LED_CURRENT_12_5MA 0x3F
#define MAX30102_LED_CURRENT_25MA   0x7F
#define MAX30102_LED_CURRENT_50MA   0xFF

/* ========== 算法相关常量（从 STM32 移植） ========== */
#define MAX30102_FS                 100
#define MAX30102_BUFFER_SIZE        (MAX30102_FS * 5)
#define MAX30102_MA4_SIZE           4
#define MAX30102_HAMMING_SIZE       5

#ifndef min
#define min(x, y) ((x) < (y) ? (x) : (y))
#endif

/* FIFO 数据结构 */
typedef struct {
    uint32_t red;
    uint32_t ir;
} max30102_fifo_t;

/* 算法输出结果 */
typedef struct {
    int32_t heart_rate;
    int8_t  hr_valid;
    int32_t spo2;
    int8_t  spo2_valid;
} max30102_result_t;

/* 中断回调函数类型 */
typedef void (*max30102_int_callback_t)(void);

/* 初始化 MAX30102 */
int max30102_init(void);

/* 写寄存器 */
int max30102_write_reg(uint8_t reg, uint8_t value);

/* 读寄存器 */
int max30102_read_reg(uint8_t reg, uint8_t *value);

/* 读多个寄存器 */
int max30102_read_regs(uint8_t reg, uint8_t *buf, uint8_t len);

/* 读取 FIFO 数据 */
int max30102_fifo_read(max30102_fifo_t *data, uint8_t *sample_count);

/* 读取温度值 (摄氏度) */
float max30102_read_temperature(void);

/* 启动温度转换 */
int max30102_start_temp_conversion(void);

/* 检查是否有新数据 */
bool max30102_check_new_data(void);

/* 复位 MAX30102 */
int max30102_reset(void);

/* 进入休眠模式 */
int max30102_shutdown(void);

/* 唤醒 */
int max30102_wakeup(void);

/* 设置 LED 电流 */
int max30102_set_led_current(uint8_t red_current, uint8_t ir_current);

/* 设置采样率和脉冲宽度 */
int max30102_set_sampling(uint8_t sample_rate, uint8_t pulse_width, uint8_t adc_range);

/* 初始化中断 (使用 INT 引脚时调用) */
int max30102_int_init(max30102_int_callback_t callback);

/* 清除中断标志 */
void max30102_int_clear(void);

/* 启动 MAX30102 采集任务 */
void max30102_task_start(void);

/* ========== 心率/血氧算法接口（从 STM32 移植） ========== */
void maxim_heart_rate_and_oxygen_saturation(uint32_t *pun_ir_buffer, int32_t n_ir_buffer_length,
                                            uint32_t *pun_red_buffer, int32_t *pn_spo2,
                                            int8_t *pch_spo2_valid, int32_t *pn_heart_rate,
                                            int8_t *pch_hr_valid);

#endif /* __MAX30102_H__ */