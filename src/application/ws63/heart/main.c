/*
 * main.c - MAX30102 数据采集（修复版 - 使用忙等待替代定时器中断）
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <math.h>

/* ========== SDK 头文件 ========== */
#include "pinctrl.h"
#include "i2c.h"
#include "gpio.h"
#include "hal_gpio.h"
#include "osal_debug.h"
#include "soc_osal.h"
#include "app_init.h"
#include "errcode.h"
#include "cmsis_os2.h"
#include "watchdog.h"

/* 如果有tcxo计数器头文件，取消注释 */
// #include "tcxo.h"  

/* ========== 宏定义 ========== */
#define CONFIG_I2C_SCL_MASTER_PIN   16
#define CONFIG_I2C_SDA_MASTER_PIN   15
#define CONFIG_I2C_MASTER_PIN_MODE  2
#define I2C_MASTER_ADDR             0x0
#define I2C_SET_BANDRATE            400000

#define MAX30102_I2C_BUS            1
#define MAX30102_ADDR               0x57

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

#define SAMPLE_RATE_HZ              100
#define SAMPLE_TIME_S               10
#define TOTAL_SAMPLES               (SAMPLE_RATE_HZ * SAMPLE_TIME_S)
#define SAMPLE_PERIOD_US            10000  /* 10ms = 100Hz */

#define SSD1306_WIDTH               128
#define SSD1306_HEIGHT              64
#define SSD1306_BUFFER_SIZE         (SSD1306_WIDTH * SSD1306_HEIGHT / 8)
#define I2C_SLAVE2_ADDR             0x3C
#define SSD1306_CTRL_CMD            0x00
#define SSD1306_CTRL_DATA           0x40
#define SSD1306_MASK_CONT           (0x1 << 7)

#define BSP_LED                     7

/* ========== 全局变量 ========== */
uint32_t ir_buffer[TOTAL_SAMPLES];
uint32_t red_buffer[TOTAL_SAMPLES];
volatile uint16_t sample_index = 0;

uint32_t fifo_red = 0;
uint32_t fifo_ir = 0;
uint8_t ach_i2c_data[6] = {0};

static uint8_t SSD1306_Buffer[SSD1306_BUFFER_SIZE];

typedef enum { Black = 0, White = 1 } SSD1306_COLOR;

typedef struct {
    uint16_t CurrentX;
    uint16_t CurrentY;
    uint8_t Initialized;
    uint8_t DisplayOn;
} SSD1306_t;

static SSD1306_t SSD1306;

typedef struct {
    const uint8_t width;
    uint8_t height;
    const uint16_t *data;
} FontDef;

static const unsigned short Font7x10[] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x3800, 0x4400, 0x4400, 0x5400, 0x4400, 0x4400, 0x4400, 0x3800, 0x0000, 0x0000,
    0x1000, 0x3000, 0x5000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x0000, 0x0000,
    0x3800, 0x4400, 0x0400, 0x0800, 0x1000, 0x2000, 0x7C00, 0x0000, 0x0000, 0x0000,
    0x3800, 0x4400, 0x0400, 0x1800, 0x0400, 0x4400, 0x3800, 0x0000, 0x0000, 0x0000,
    0x0800, 0x1800, 0x2800, 0x2800, 0x4800, 0x7C00, 0x0800, 0x0800, 0x0000, 0x0000,
    0x7C00, 0x4000, 0x7800, 0x0400, 0x0400, 0x4400, 0x3800, 0x0000, 0x0000, 0x0000,
    0x3800, 0x4400, 0x4000, 0x7800, 0x4400, 0x4400, 0x3800, 0x0000, 0x0000, 0x0000,
    0x7C00, 0x0400, 0x0800, 0x1000, 0x2000, 0x2000, 0x2000, 0x0000, 0x0000, 0x0000,
    0x3800, 0x4400, 0x4400, 0x3800, 0x4400, 0x4400, 0x3800, 0x0000, 0x0000, 0x0000,
    0x3800, 0x4400, 0x4400, 0x3C00, 0x0400, 0x4400, 0x3800, 0x0000, 0x0000, 0x0000,
};

#define FONT_OFFSET_0   1
static FontDef Font_7x10 = {7, 10, Font7x10};

static const uint8_t fonts_heart[][32] = {
    {0x02,0x00,0x01,0x00,0x00,0x80,0x00,0xC0,0x08,0x80,0x08,0x00,0x28,0x08,0x28,0x04,
     0x28,0x02,0x48,0x02,0x88,0x02,0x08,0x00,0x08,0x10,0x08,0x10,0x07,0xF0,0x00,0x00},
    {0x02,0x00,0x01,0x08,0x7F,0xFC,0x01,0x00,0x42,0x44,0x27,0x88,0x11,0x10,0x22,0x48,
     0x4F,0xE4,0x01,0x20,0x01,0x04,0xFF,0xFE,0x01,0x00,0x01,0x00,0x01,0x00,0x01,0x00}
};

static const uint8_t fonts_spo2[][32] = {
    {0x00,0x00,0x01,0x00,0x01,0x00,0x02,0x08,0x3F,0xFC,0x24,0x48,0x24,0x48,0x24,0x48,
     0x24,0x48,0x24,0x48,0x24,0x48,0x24,0x48,0x24,0x48,0x24,0x48,0xFF,0xFE,0x00,0x00},
    {0x10,0x00,0x1F,0xFC,0x20,0x00,0x2F,0xF8,0x40,0x00,0xBF,0xF8,0x08,0x88,0x05,0x08,
     0x3F,0xE8,0x02,0x08,0x1F,0xC8,0x02,0x08,0x7F,0xFA,0x02,0x0A,0x02,0x04,0x02,0x00}
};

/* ========== 函数声明 ========== */
static void delay_ms(uint32_t ms);
// static void delay_us(uint32_t us);
static uint32_t get_micros(void);
static void app_i2c_init_pin(void);

static uint32_t ssd1306_SendData(uint8_t *buffer, uint32_t size);
static void ssd1306_WriteCommand(uint8_t byte);
static void ssd1306_Init(void);
static void ssd1306_Fill(SSD1306_COLOR color);
static void ssd1306_UpdateScreen(void);
static void ssd1306_SetCursor(uint8_t x, uint8_t y);
static char ssd1306_DrawChar(char ch, FontDef Font, SSD1306_COLOR color);
static char ssd1306_DrawString(char *str, FontDef Font, SSD1306_COLOR color);
static void ssd1306_DrawRegion(uint8_t x, uint8_t y, uint8_t w, const uint8_t *data, uint32_t size);

static uint8_t max30102_write_reg(uint8_t addr, uint8_t data);
static uint8_t max30102_read_reg(uint8_t addr);
static void max30102_read_fifo(void);
static void max30102_clear_fifo(void);
static void Max30102_reset(void);
static void MAX30102_Config(void);

static void display_init(void);
static void display_values(uint32_t ir_val, uint32_t red_val, uint16_t progress);
static void display_result(uint16_t hr, uint8_t valid);
static uint16_t calculate_heart_rate(uint32_t *ir_data, uint16_t count);

/* ========== 基础函数 ========== */
static void delay_ms(uint32_t ms) { osal_mdelay(ms); }

/* 使用简单循环做us延时（需要根据CPU频率校准） */
// static void delay_us(uint32_t us)
// {
//     /* 假设CPU 48MHz，大约48个周期1us */
//     /* 实际可能需要调整 */
//     volatile uint32_t count = us * 48 / 5;  /* 粗略估计 */
//     while (count--) __asm volatile("nop");
// }

/* 获取微秒时间戳 - 使用osal_tick或tcxo */
static uint32_t get_micros(void)
{
    /* 方法1：如果有tcxo，使用 uapi_tcxo_get_count() */
    /* return uapi_tcxo_get_count(); */
    
    /* 方法2：使用osal_get_jiffies，通常是ms级 */
    /* 这里先用ms * 1000作为近似 */
    return osal_get_jiffies() * 1000;
}

/* ========== I2C & OLED ========== */
static void app_i2c_init_pin(void)
{
    uapi_pin_set_mode(CONFIG_I2C_SCL_MASTER_PIN, CONFIG_I2C_MASTER_PIN_MODE);
    uapi_pin_set_mode(CONFIG_I2C_SDA_MASTER_PIN, CONFIG_I2C_MASTER_PIN_MODE);
}

static uint32_t ssd1306_SendData(uint8_t *buffer, uint32_t size)
{
    i2c_data_t data = {0};
    data.send_buf = buffer;
    data.send_len = size;
    uint32_t retval = uapi_i2c_master_write(MAX30102_I2C_BUS, I2C_SLAVE2_ADDR, &data);
    if (retval != 0) printf("I2C Write failed: %0X!\n", retval);
    return retval;
}

static uint32_t ssd1306_WiteByte(uint8_t regAddr, uint8_t byte)
{
    uint8_t buffer[] = {regAddr, byte};
    return ssd1306_SendData(buffer, sizeof(buffer));
}

static void ssd1306_WriteCommand(uint8_t byte) { ssd1306_WiteByte(SSD1306_CTRL_CMD, byte); }

static void ssd1306_Init(void)
{
    delay_ms(1);
    ssd1306_WriteCommand(0xAE);
    ssd1306_WriteCommand(0x20); ssd1306_WriteCommand(0x00);
    ssd1306_WriteCommand(0xB0);
    ssd1306_WriteCommand(0xC8);
    ssd1306_WriteCommand(0x00);
    ssd1306_WriteCommand(0x10);
    ssd1306_WriteCommand(0x40);
    ssd1306_WriteCommand(0x81); ssd1306_WriteCommand(0xFF);
    ssd1306_WriteCommand(0xA1);
    ssd1306_WriteCommand(0xA6);
    ssd1306_WriteCommand(0xA8); ssd1306_WriteCommand(0x3F);
    ssd1306_WriteCommand(0xA4);
    ssd1306_WriteCommand(0xD3); ssd1306_WriteCommand(0x00);
    ssd1306_WriteCommand(0xD5); ssd1306_WriteCommand(0xF0);
    ssd1306_WriteCommand(0xD9); ssd1306_WriteCommand(0x11);
    ssd1306_WriteCommand(0xDA); ssd1306_WriteCommand(0x12);
    ssd1306_WriteCommand(0xDB); ssd1306_WriteCommand(0x30);
    ssd1306_WriteCommand(0x8D); ssd1306_WriteCommand(0x14);
    ssd1306_WriteCommand(0xAF);
    ssd1306_Fill(Black);
    ssd1306_UpdateScreen();
    SSD1306.CurrentX = 0; SSD1306.CurrentY = 0;
    SSD1306.Initialized = 1; SSD1306.DisplayOn = 1;
}

static void ssd1306_Fill(SSD1306_COLOR color)
{
    memset(SSD1306_Buffer, (color == Black) ? 0x00 : 0xFF, SSD1306_BUFFER_SIZE);
}

static void ssd1306_UpdateScreen(void)
{
    uint8_t cmd[] = {0x21, 0x00, 0x7F, 0x22, 0x00, 0x07};
    uint32_t count = 0;
    uint8_t data[sizeof(cmd) * 2 + SSD1306_BUFFER_SIZE + 1] = {0};
    for (uint32_t i = 0; i < sizeof(cmd) / sizeof(cmd[0]); i++) {
        data[count++] = SSD1306_CTRL_CMD | SSD1306_MASK_CONT;
        data[count++] = cmd[i];
    }
    data[count++] = SSD1306_CTRL_DATA;
    memcpy(&data[count], SSD1306_Buffer, SSD1306_BUFFER_SIZE);
    count += SSD1306_BUFFER_SIZE;
    ssd1306_SendData(data, count);
}

static void ssd1306_SetCursor(uint8_t x, uint8_t y) { SSD1306.CurrentX = x; SSD1306.CurrentY = y; }

static char ssd1306_DrawChar(char ch, FontDef Font, SSD1306_COLOR color)
{
    uint32_t i, b, j;
    (void)color;
    if (ch < '0' || ch > '9') return 0;
    if (SSD1306_WIDTH < (SSD1306.CurrentX + Font.width) || 
        SSD1306_HEIGHT < (SSD1306.CurrentY + Font.height)) return 0;
    uint16_t idx = (ch - '0' + FONT_OFFSET_0);
    for (i = 0; i < Font.height; i++) {
        b = Font.data[idx * Font.height + i];
        for (j = 0; j < Font.width; j++) {
            if ((b << j) & 0x8000) {
                uint32_t c = 8;
                SSD1306_Buffer[(SSD1306.CurrentX + j) + ((SSD1306.CurrentY + i) / c) * SSD1306_WIDTH] |= 1 << ((SSD1306.CurrentY + i) % c);
            }
        }
    }
    SSD1306.CurrentX += Font.width;
    return ch;
}

static char ssd1306_DrawString(char *str, FontDef Font, SSD1306_COLOR color)
{
    (void)color;
    while (*str) {
        if (ssd1306_DrawChar(*str, Font, White) != *str) return *str;
        str++;
    }
    return *str;
}

static void ssd1306_DrawRegion(uint8_t x, uint8_t y, uint8_t w, const uint8_t *data, uint32_t size)
{
    uint32_t stride = w;
    uint8_t h = 16, width = w;
    if (x + w > SSD1306_WIDTH || y + h > SSD1306_HEIGHT || w * h == 0) return;
    width = (width <= SSD1306_WIDTH ? width : SSD1306_WIDTH);
    h = (h <= SSD1306_HEIGHT ? h : SSD1306_HEIGHT);
    stride = (stride == 0 ? w : stride);
    for (uint8_t i = 0; i < h; i++) {
        uint32_t base = i * stride / 8;
        for (uint8_t j = 0; j < width; j++) {
            uint32_t idx = base + (j / 8);
            uint8_t byte = idx < size ? data[idx] : 0;
            uint8_t bit = byte & (0x80 >> (j % 8));
            if (bit) {
                uint32_t c = 8;
                SSD1306_Buffer[(x + j) + ((y + i) / c) * SSD1306_WIDTH] |= 1 << ((y + i) % c);
            }
        }
    }
}

/* ========== MAX30102 驱动 ========== */
static uint8_t max30102_write_reg(uint8_t addr, uint8_t data)
{
    uint8_t cmd[2] = {addr, data};
    i2c_data_t i2c_data = {0};
    i2c_data.send_buf = cmd;
    i2c_data.send_len = sizeof(cmd);
    uint32_t ret = uapi_i2c_master_write(MAX30102_I2C_BUS, MAX30102_ADDR, &i2c_data);
    if (ret != ERRCODE_SUCC) { printf("MAX30102 write err: 0x%x\r\n", ret); return 1; }
    return 0;
}

static uint8_t max30102_read_reg(uint8_t addr)
{
    i2c_data_t i2c_data = {0};
    uint8_t rx_buf[1] = {0};
    i2c_data.send_buf = &addr;
    i2c_data.send_len = 1;
    i2c_data.receive_buf = rx_buf;
    i2c_data.receive_len = 1;
    uint32_t ret = uapi_i2c_master_writeread(MAX30102_I2C_BUS, MAX30102_ADDR, &i2c_data);
    if (ret != ERRCODE_SUCC) { printf("MAX30102 read err: 0x%x\r\n", ret); return 0; }
    return rx_buf[0];
}

static void max30102_read_fifo(void)
{
    uint8_t reg = REG_FIFO_DATA;
    i2c_data_t i2c_data = {0};
    i2c_data.send_buf = &reg;
    i2c_data.send_len = 1;
    i2c_data.receive_buf = ach_i2c_data;
    i2c_data.receive_len = 6;
    uint32_t ret = uapi_i2c_master_writeread(MAX30102_I2C_BUS, MAX30102_ADDR, &i2c_data);
    if (ret != ERRCODE_SUCC) {
        printf("MAX30102 FIFO read err: 0x%x\r\n", ret);
        return;
    }
    fifo_red = ((uint32_t)ach_i2c_data[0] << 16) | ((uint32_t)ach_i2c_data[1] << 8) | (uint32_t)ach_i2c_data[2];
    fifo_red &= 0x03FFFF;
    fifo_ir = ((uint32_t)ach_i2c_data[3] << 16) | ((uint32_t)ach_i2c_data[4] << 8) | (uint32_t)ach_i2c_data[5];
    fifo_ir &= 0x03FFFF;
}

static void max30102_clear_fifo(void)
{
    max30102_write_reg(REG_FIFO_WR_PTR, 0x00);
    max30102_write_reg(REG_OVF_COUNTER, 0x00);
    max30102_write_reg(REG_FIFO_RD_PTR, 0x00);
}

static void Max30102_reset(void) { max30102_write_reg(REG_MODE_CONFIG, 0x40); }

static void MAX30102_Config(void)
{
    max30102_write_reg(REG_INTR_ENABLE_1, 0x40);
    max30102_write_reg(REG_INTR_ENABLE_2, 0x00);
    max30102_clear_fifo();
    max30102_write_reg(REG_FIFO_CONFIG, 0x0F);  /* 平均=1, 不循环, 几乎满=17 */
    max30102_write_reg(REG_MODE_CONFIG, 0x03);  /* SpO2模式 */
    max30102_write_reg(REG_SPO2_CONFIG, 0x27);  /* 100Hz, 18bit */
    max30102_write_reg(REG_LED1_PA, 0x1F);
    max30102_write_reg(REG_LED2_PA, 0x1F);
    max30102_write_reg(REG_PILOT_PA, 0x7F);
}

/* ========== 显示函数 ========== */
static void display_chinese(void)
{
    const uint32_t W = 16;
    for (size_t i = 0; i < 2; i++) ssd1306_DrawRegion(i * W, 3, W, fonts_heart[i], 32);
    for (size_t j = 0; j < 2; j++) ssd1306_DrawRegion(j * W, 35, W, fonts_spo2[j], 32);
}

static void display_values(uint32_t ir_val, uint32_t red_val, uint16_t progress)
{
    static char ir_line[32], red_line[32], prog_line[32];
    ssd1306_Fill(Black);
    display_chinese();
    ssd1306_SetCursor(32, 8);
    sprintf(ir_line, ": %lu", ir_val);
    ssd1306_DrawString(ir_line, Font_7x10, White);
    ssd1306_SetCursor(32, 25);
    sprintf(red_line, ": %lu", red_val);
    ssd1306_DrawString(red_line, Font_7x10, White);
    ssd1306_SetCursor(0, 45);
    sprintf(prog_line, "%d/%d", progress, TOTAL_SAMPLES);
    ssd1306_DrawString(prog_line, Font_7x10, White);
    ssd1306_UpdateScreen();
}

static void display_result(uint16_t hr, uint8_t valid)
{
    static char result[32];
    ssd1306_Fill(Black);
    ssd1306_SetCursor(0, 0);
    ssd1306_DrawString("HR Result:", Font_7x10, White);
    ssd1306_SetCursor(0, 20);
    if (valid) sprintf(result, "%d BPM", hr);
    else sprintf(result, "Invalid");
    ssd1306_DrawString(result, Font_7x10, White);
    ssd1306_SetCursor(0, 40);
    ssd1306_DrawString("Done!", Font_7x10, White);
    ssd1306_UpdateScreen();
}

static void display_init(void)
{
    ssd1306_Init();
    ssd1306_Fill(Black);
    ssd1306_SetCursor(0, 0);
}

/* ========== 动态阈值心率计算 ========== */
static uint16_t calculate_heart_rate(uint32_t *ir_data, uint16_t count)
{
    #define DC_REMOVE_WINDOW    50
    #define MIN_PEAK_DISTANCE   30
    #define MAX_HR              220
    #define MIN_HR              40

    int32_t ac_buffer[TOTAL_SAMPLES];
    uint32_t dc_sum = 0;
    
    for (uint16_t i = 0; i < count; i++) {
        dc_sum += ir_data[i];
        if (i >= DC_REMOVE_WINDOW) dc_sum -= ir_data[i - DC_REMOVE_WINDOW];
        uint32_t dc = dc_sum / ((i < DC_REMOVE_WINDOW) ? (i + 1) : DC_REMOVE_WINDOW);
        ac_buffer[i] = (int32_t)ir_data[i] - (int32_t)dc;
    }

    int32_t ac_max = ac_buffer[0], ac_min = ac_buffer[0];
    for (uint16_t i = 1; i < count; i++) {
        if (ac_buffer[i] > ac_max) ac_max = ac_buffer[i];
        if (ac_buffer[i] < ac_min) ac_min = ac_buffer[i];
    }
    int32_t amplitude = ac_max - ac_min;
    if (amplitude < 500) {
        printf("Signal too weak: amplitude=%ld\r\n", amplitude);
        return 0;
    }

    int32_t threshold = ac_min + amplitude / 2;
    uint16_t peak_indices[50] = {0};
    uint16_t peak_count = 0;
    uint8_t was_above = 0;

    for (uint16_t i = 1; i < count; i++) {
        uint8_t is_above = (ac_buffer[i] > threshold);
        if (is_above && !was_above) {
            uint16_t peak_idx = i;
            int32_t peak_val = ac_buffer[i];
            uint16_t search_end = (i + 10 < count) ? i + 10 : count;
            for (uint16_t j = i; j < search_end; j++) {
                if (ac_buffer[j] > peak_val) {
                    peak_val = ac_buffer[j];
                    peak_idx = j;
                }
            }
            if (peak_count == 0 || (peak_idx - peak_indices[peak_count - 1]) > MIN_PEAK_DISTANCE) {
                peak_indices[peak_count++] = peak_idx;
                if (peak_count >= 50) break;
            }
        }
        was_above = is_above;
    }

    if (peak_count < 2) {
        printf("Too few peaks: %d\r\n", peak_count);
        return 0;
    }

    uint32_t total_interval = 0;
    uint16_t valid_peaks = 0;
    for (uint16_t i = 1; i < peak_count; i++) {
        uint16_t interval = peak_indices[i] - peak_indices[i - 1];
        if (interval >= 20 && interval <= 150) {
            total_interval += interval;
            valid_peaks++;
        }
    }

    if (valid_peaks < 1) {
        printf("No valid intervals\r\n");
        return 0;
    }

    uint16_t avg_interval = total_interval / valid_peaks;
    uint16_t hr = (uint16_t)(6000 / avg_interval);
    printf("Peaks: %d valid, avg interval: %d, amplitude: %ld, HR: %d BPM\r\n", 
           valid_peaks + 1, avg_interval, amplitude, hr);

    if (hr < MIN_HR || hr > MAX_HR) return 0;
    return hr;
}

/* ========== 主任务 ========== */
static int MAX30102_task(void)
{
    delay_ms(5000);

    printf("\r\n========== MAX30102 Polling Mode ==========\r\n");
    printf("Sample Rate: %d Hz\r\n", SAMPLE_RATE_HZ);
    printf("Sample Time: %d s\r\n", SAMPLE_TIME_S);
    printf("Total Samples: %d\r\n", TOTAL_SAMPLES);
    printf("========================================\r\n");

    app_i2c_init_pin();
    errcode_t ret = uapi_i2c_master_init(MAX30102_I2C_BUS, I2C_SET_BANDRATE, I2C_MASTER_ADDR);
    if (ret != 0) { printf("I2C init failed, ret = 0x%x\r\n", ret); return -1; }
    printf("I2C init done\r\n");

    display_init();
    printf("OLED init done\r\n");

    Max30102_reset();
    delay_ms(10);
    uint8_t mode_reg = max30102_read_reg(REG_MODE_CONFIG);
    printf("MAX30102 mode reg: 0x%02X\r\n", mode_reg);
    MAX30102_Config();
    printf("MAX30102 config done\r\n");

    uapi_pin_set_mode(BSP_LED, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(BSP_LED, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(BSP_LED, GPIO_LEVEL_LOW);

    while (1) {
        printf("\r\n--- Starting 10s data collection ---\r\n");

        sample_index = 0;
        memset(ir_buffer, 0, sizeof(ir_buffer));
        memset(red_buffer, 0, sizeof(red_buffer));

        max30102_clear_fifo();
        delay_ms(10);

        /* 使用忙等待循环替代定时器中断 */
        uint32_t next_sample_time = get_micros();
        uint32_t sample_count = 0;
        uint32_t loop_count = 0;

        while (sample_count < TOTAL_SAMPLES) {
            uint32_t now = get_micros();
            
            /* 检查是否到达采样时间 */
            if (now >= next_sample_time) {
                /* 读取FIFO中所有可用样本 */
                uint8_t wr_ptr = max30102_read_reg(REG_FIFO_WR_PTR);
                uint8_t rd_ptr = max30102_read_reg(REG_FIFO_RD_PTR);
                uint8_t num_samples = (wr_ptr - rd_ptr) & 0x1F;

                /* 批量读取 */
                for (uint8_t s = 0; s < num_samples && sample_count < TOTAL_SAMPLES; s++) {
                    max30102_read_fifo();
                    ir_buffer[sample_count] = fifo_ir;
                    red_buffer[sample_count] = fifo_red;
                    sample_count++;

                    if (fifo_ir > 10000) uapi_gpio_set_val(BSP_LED, GPIO_LEVEL_HIGH);
                    else uapi_gpio_set_val(BSP_LED, GPIO_LEVEL_LOW);
                }

                /* 更新显示 */
                if (sample_count % 10 == 0) {
                    display_values(fifo_ir, fifo_red, sample_count);
                }

                /* 设置下一个采样时间点 */
                next_sample_time += SAMPLE_PERIOD_US;
                
                /* 打印调试信息（每100个点打印一次） */
                if (sample_count % 100 == 0) {
                    printf("Sample %lu, FIFO cnt=%d, IR=%lu\r\n", 
                           sample_count, num_samples, fifo_ir);
                }
            }
            
            loop_count++;
            /* 短暂yield，避免完全占用CPU */
            if (loop_count % 1000 == 0) {
                osal_msleep(0);  /* 让出CPU */
            }
        }

        printf("Collection done! Samples: %d\r\n", sample_count);

        /* 数据处理 */
        printf("\r\n--- Processing data ---\r\n");
        printf("First 20 samples (IR):\r\n");
        for (uint16_t i = 0; i < 20 && i < TOTAL_SAMPLES; i++) {
            printf("%lu ", ir_buffer[i]);
            if ((i + 1) % 10 == 0) printf("\r\n");
        }

        uint32_t ir_max = ir_buffer[0], ir_min = ir_buffer[0];
        for (uint16_t i = 1; i < TOTAL_SAMPLES; i++) {
            if (ir_buffer[i] > ir_max) ir_max = ir_buffer[i];
            if (ir_buffer[i] < ir_min) ir_min = ir_buffer[i];
        }
        uint32_t ir_diff = ir_max - ir_min;
        printf("IR max=%lu, min=%lu, diff=%lu\r\n", ir_max, ir_min, ir_diff);

        if (ir_diff < 2000) {
            printf("Signal too weak, please check finger placement\r\n");
            display_result(0, 0);
            delay_ms(5000);
            continue;
        }

        uint16_t hr = calculate_heart_rate((uint32_t *)ir_buffer, TOTAL_SAMPLES);
        uint8_t hr_valid = (hr > 0) ? 1 : 0;
        display_result(hr, hr_valid);
        printf("Heart Rate: %d BPM (%s)\r\n", hr, hr_valid ? "Valid" : "Invalid");

        printf("\r\nWaiting 5s for next collection...\r\n");
        delay_ms(5000);
    }
    return 0;
}

/* ========== 系统入口 ========== */
#define TASK_PRIO       24
#define STACK_SIZE      0x2000

static void system_entry(void)
{
    uapi_watchdog_disable();
    osal_task *task_handle = NULL;
    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)MAX30102_task, 0, "MAX30102_task", STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, TASK_PRIO);
        osal_kfree(task_handle);
    }
    osal_kthread_unlock();
}

app_run(system_entry);