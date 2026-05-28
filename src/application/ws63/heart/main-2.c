/*
 * main.c - MAX30102 定时器中断 + FIFO 数据采集
 * 功能：定时器10ms中断 → 设置标志 → 主循环读取FIFO → 10秒采集1000点 → 计算心率
 * 定时器：Timer1（Timer0用于系统时钟，Timer1和Timer2给业务使用）
 * Timer1提供6个软件定时器
 * FIFO模式：读取并处理FIFO数据，自动处理指针
 * 平台：HiSilicon WS63
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
#include "timer.h"          /* 硬件定时器 */

/* ========== 宏定义 ========== */

/* I2C 配置 */
#define CONFIG_I2C_SCL_MASTER_PIN   16
#define CONFIG_I2C_SDA_MASTER_PIN   15
#define CONFIG_I2C_MASTER_PIN_MODE  2
#define I2C_MASTER_ADDR             0x0
#define I2C_SET_BANDRATE            400000

/* MAX30102 I2C 配置 */
#define MAX30102_I2C_BUS            1
#define MAX30102_ADDR               0x57

/* MAX30102 寄存器地址定义 */
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

/* 定时器配置 - 使用Timer1 */
#define TIMER_INDEX                 TIMER_INDEX_1
#define TIMER_IRQ                   1
#define TIMER_IRQ_PRIO              3
#define TIMER_PERIOD_US             10000

/* 采样配置 */
#define SAMPLE_RATE_HZ              100
#define SAMPLE_TIME_S               10
#define TOTAL_SAMPLES               (SAMPLE_RATE_HZ * SAMPLE_TIME_S)

/* FIFO 配置 */
#define FIFO_DEPTH                  32

/* OLED (SSD1306) 配置 */
#define SSD1306_WIDTH               128
#define SSD1306_HEIGHT              64
#define SSD1306_BUFFER_SIZE         (SSD1306_WIDTH * SSD1306_HEIGHT / 8)
#define I2C_SLAVE2_ADDR             0x3C
#define SSD1306_CTRL_CMD            0x00
#define SSD1306_CTRL_DATA           0x40
#define SSD1306_MASK_CONT           (0x1 << 7)

/* LED 引脚 */
#define BSP_LED                     7

/* ========== 全局变量 ========== */

/* 定时器句柄 */
static timer_handle_t g_timer_handle = 0;

/* 定时器标志 */
volatile uint8_t timer_flag = 0;
volatile uint32_t timer_count = 0;

/* 数据采集缓冲区（10秒 = 1000点） */
uint32_t ir_buffer[TOTAL_SAMPLES];
uint32_t red_buffer[TOTAL_SAMPLES];
volatile uint16_t sample_index = 0;
volatile uint8_t collection_done = 0;

/* 当前FIFO数据 */
uint32_t fifo_red = 0;
uint32_t fifo_ir = 0;
uint8_t ach_i2c_data[6] = {0};

/* OLED 缓冲区 */
static uint8_t SSD1306_Buffer[SSD1306_BUFFER_SIZE];

typedef enum {
    Black = 0,
    White = 1
} SSD1306_COLOR;

typedef struct {
    uint16_t CurrentX;
    uint16_t CurrentY;
    uint8_t Inverted;
    uint8_t Initialized;
    uint8_t DisplayOn;
} SSD1306_t;

static SSD1306_t SSD1306;

/* 字体定义 */
typedef struct {
    const uint8_t width;
    uint8_t height;
    const uint16_t *data;
} FontDef;

/* 7x10 字体 */
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

/* 中文字体 16x16 */
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

static void timer_callback(uintptr_t param);
static void timer_init(void);
static void timer_deinit(void);

static void display_init(void);
static void display_values(uint32_t ir_val, uint32_t red_val, uint16_t progress);
static void display_result(uint16_t hr, uint8_t valid);

static uint16_t calculate_heart_rate(uint32_t *ir_data, uint16_t count);

/* ========== 基础函数 ========== */

static void delay_ms(uint32_t ms)
{
    osal_mdelay(ms);
}

/* ========== 定时器中断 ========== */

/* 定时器回调函数 - 10ms中断 */
static void timer_callback(uintptr_t param)
{
    (void)param;
    timer_flag = 1;
    timer_count++;

    /* 重新启动定时器以实现周期性触发（单次模式需要手动重启） */
    uapi_timer_start(g_timer_handle, TIMER_PERIOD_US, timer_callback, 0);
}

/* 定时器初始化 - 10ms周期，100Hz */
// static void timer_init(void)
// {
//     errcode_t ret;

//     /* 1. 定时器软件初始化 */
//     uapi_timer_init();

//     /* 2. 设置Timer1硬件初始化，配置中断号和优先级 */
//     ret = uapi_timer_adapter(TIMER_INDEX, TIMER_IRQ, TIMER_IRQ_PRIO);
//     if (ret != ERRCODE_SUCC) {
//         printf("Timer adapter failed: 0x%x\r\n", ret);
//         return;
//     }

//     /* 3. 创建Timer1软件定时器控制句柄 */
//     ret = uapi_timer_create(TIMER_INDEX, &g_timer_handle);
//     if (ret != ERRCODE_SUCC) {
//         printf("Timer create failed: 0x%x\r\n", ret);
//         return;
//     }

//     /* 4. 启动定时器：传入句柄、时间、回调函数、回调参数 */
//     ret = uapi_timer_start(g_timer_handle, TIMER_PERIOD_US, timer_callback, 0);
//     if (ret != ERRCODE_SUCC) {
//         printf("Timer start failed: 0x%x\r\n", ret);
//         return;
//     }

//     printf("Timer init done: %dus period (100Hz)\r\n", TIMER_PERIOD_US);
// }

/* 定时器初始化 - 只负责 create + start */
static void timer_init(void)
{
    errcode_t ret;

    /* 创建Timer1软件定时器控制句柄 */
    ret = uapi_timer_create(TIMER_INDEX, &g_timer_handle);
    if (ret != ERRCODE_SUCC) {
        printf("Timer create failed: 0x%x\r\n", ret);
        return;
    }

    /* 启动定时器 */
    ret = uapi_timer_start(g_timer_handle, TIMER_PERIOD_US, timer_callback, 0);
    if (ret != ERRCODE_SUCC) {
        printf("Timer start failed: 0x%x\r\n", ret);
        return;
    }

    printf("Timer started, handle=%u\r\n", (unsigned)g_timer_handle);
    printf("Timer init done: %dus period (100Hz)\r\n", TIMER_PERIOD_US);
}

// static void timer_deinit(void)
// {
//     uapi_timer_stop(g_timer_handle);
//     uapi_timer_delete(g_timer_handle);
//     // uapi_timer_deinit();
//     g_timer_handle = 0;
// }

static void timer_deinit(void)
{
    if (g_timer_handle != 0) {
        uapi_timer_stop(g_timer_handle);
        uapi_timer_delete(g_timer_handle);
        g_timer_handle = 0;
    }
    /* 不调用 uapi_timer_deinit()，保持定时器子系统和中断向量存活 */
}

/* ========== I2C 初始化 ========== */
static void app_i2c_init_pin(void)
{
    uapi_pin_set_mode(CONFIG_I2C_SCL_MASTER_PIN, CONFIG_I2C_MASTER_PIN_MODE);
    uapi_pin_set_mode(CONFIG_I2C_SDA_MASTER_PIN, CONFIG_I2C_MASTER_PIN_MODE);
}

/* ========== OLED (SSD1306) 驱动 ========== */

static uint32_t ssd1306_SendData(uint8_t *buffer, uint32_t size)
{
    i2c_data_t data = {0};
    data.send_buf = buffer;
    data.send_len = size;
    uint32_t retval = uapi_i2c_master_write(MAX30102_I2C_BUS, I2C_SLAVE2_ADDR, &data);
    if (retval != 0) {
        printf("I2C Write failed: %0X!\n", retval);
        return retval;
    }
    return 0;
}

static uint32_t ssd1306_WiteByte(uint8_t regAddr, uint8_t byte)
{
    uint8_t buffer[] = {regAddr, byte};
    return ssd1306_SendData(buffer, sizeof(buffer));
}

static void ssd1306_WriteCommand(uint8_t byte)
{
    ssd1306_WiteByte(SSD1306_CTRL_CMD, byte);
}

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

    SSD1306.CurrentX = 0;
    SSD1306.CurrentY = 0;
    SSD1306.Initialized = 1;
    SSD1306.DisplayOn = 1;
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

static void ssd1306_SetCursor(uint8_t x, uint8_t y)
{
    SSD1306.CurrentX = x;
    SSD1306.CurrentY = y;
}

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
    uint8_t h = 16;
    uint8_t width = w;

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
    if (ret != ERRCODE_SUCC) {
        printf("MAX30102 write err: 0x%x\r\n", ret);
        return 1;
    }
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
    if (ret != ERRCODE_SUCC) {
        printf("MAX30102 read err: 0x%x\r\n", ret);
        return 0;
    }
    return rx_buf[0];
}

/* 读取FIFO数据 - 标准6字节 = 1个样本（3字节Red + 3字节IR） */
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

    /* 组合18位数据 - 大端序 */
    fifo_red = ((uint32_t)ach_i2c_data[0] << 16)
             | ((uint32_t)ach_i2c_data[1] << 8)
             | (uint32_t)ach_i2c_data[2];
    fifo_red &= 0x03FFFF;

    fifo_ir = ((uint32_t)ach_i2c_data[3] << 16)
            | ((uint32_t)ach_i2c_data[4] << 8)
            | (uint32_t)ach_i2c_data[5];
    fifo_ir &= 0x03FFFF;
}

/* 清空FIFO */
static void max30102_clear_fifo(void)
{
    max30102_write_reg(REG_FIFO_WR_PTR, 0x00);
    max30102_write_reg(REG_OVF_COUNTER, 0x00);
    max30102_write_reg(REG_FIFO_RD_PTR, 0x00);
}

static void Max30102_reset(void)
{
    max30102_write_reg(REG_MODE_CONFIG, 0x40);
}

static void MAX30102_Config(void)
{
    /* 中断使能：只开启PPG_RDY（新样本就绪） */
    max30102_write_reg(REG_INTR_ENABLE_1, 0x40);
    max30102_write_reg(REG_INTR_ENABLE_2, 0x00);

    /* 清空FIFO */
    max30102_clear_fifo();

    /* FIFO配置：样本平均=1, FIFO满时不循环, 几乎满阈值=0x0F(17) */
    max30102_write_reg(REG_FIFO_CONFIG, 0x0F);

    /* 模式：SpO2（同时采集Red和IR） */
    max30102_write_reg(REG_MODE_CONFIG, 0x03);

    /* SpO2配置：100Hz, 18bit */
    max30102_write_reg(REG_SPO2_CONFIG, 0x27);

    /* LED电流 */
    max30102_write_reg(REG_LED1_PA, 0x1F);
    max30102_write_reg(REG_LED2_PA, 0x1F);
    max30102_write_reg(REG_PILOT_PA, 0x7F);
}

/* ========== 显示函数 ========== */

static void display_chinese(void)
{
    const uint32_t W = 16;
    for (size_t i = 0; i < 2; i++) {
        ssd1306_DrawRegion(i * W, 3, W, fonts_heart[i], 32);
    }
    for (size_t j = 0; j < 2; j++) {
        ssd1306_DrawRegion(j * W, 35, W, fonts_spo2[j], 32);
    }
}

/* 采集过程中显示：实时值 + 进度 */
static void display_values(uint32_t ir_val, uint32_t red_val, uint16_t progress)
{
    static char ir_line[32] = {0};
    static char red_line[32] = {0};
    static char prog_line[32] = {0};

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

/* 显示最终结果 */
static void display_result(uint16_t hr, uint8_t valid)
{
    static char result[32] = {0};

    ssd1306_Fill(Black);

    ssd1306_SetCursor(0, 0);
    ssd1306_DrawString("HR Result:", Font_7x10, White);

    ssd1306_SetCursor(0, 20);
    if (valid) {
        sprintf(result, "%d BPM", hr);
    } else {
        sprintf(result, "Invalid");
    }
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

/* ========== 心率计算（简化版 - 基于峰值检测） ========== */

static uint16_t calculate_heart_rate(uint32_t *ir_data, uint16_t count)
{
    #define PEAK_THRESHOLD      500
    #define MIN_PEAK_DISTANCE   30

    uint16_t peak_indices[50] = {0};
    uint16_t peak_count = 0;

    for (uint16_t i = 2; i < count - 2; i++) {
        if (ir_data[i] > PEAK_THRESHOLD &&
            ir_data[i] > ir_data[i-1] && ir_data[i] > ir_data[i-2] &&
            ir_data[i] > ir_data[i+1] && ir_data[i] > ir_data[i+2]) {

            if (peak_count == 0 || (i - peak_indices[peak_count-1]) > MIN_PEAK_DISTANCE) {
                peak_indices[peak_count++] = i;
                if (peak_count >= 50) break;
            }
        }
    }

    if (peak_count < 2) {
        printf("Too few peaks found: %d\r\n", peak_count);
        return 0;
    }

    uint32_t total_interval = 0;
    for (uint16_t i = 1; i < peak_count; i++) {
        total_interval += (peak_indices[i] - peak_indices[i-1]);
    }
    uint16_t avg_interval = total_interval / (peak_count - 1);

    uint16_t hr = (uint16_t)(6000 / avg_interval);

    printf("Peaks found: %d, avg interval: %d, HR: %d BPM\r\n", 
           peak_count, avg_interval, hr);

    if (hr < 30 || hr > 250) {
        return 0;
    }

    return hr;
}

/* ========== 主任务 ========== */

static int MAX30102_task(void)
{
    delay_ms(5000);

    printf("\r\n========== MAX30102 Timer + FIFO Collection ==========\r\n");
    printf("Sample Rate: %d Hz\r\n", SAMPLE_RATE_HZ);
    printf("Sample Time: %d s\r\n", SAMPLE_TIME_S);
    printf("Total Samples: %d\r\n", TOTAL_SAMPLES);
    printf("========================================\r\n");

    /* 1. I2C 初始化 */
    app_i2c_init_pin();
    errcode_t ret = uapi_i2c_master_init(MAX30102_I2C_BUS, I2C_SET_BANDRATE, I2C_MASTER_ADDR);
    if (ret != 0) {
        printf("I2C init failed, ret = 0x%x\r\n", ret);
        return -1;
    }
    printf("I2C init done\r\n");

    /* 2. OLED 初始化 */
    display_init();
    printf("OLED init done\r\n");

    /* 3. MAX30102 初始化 */
    Max30102_reset();
    delay_ms(10);

    uint8_t mode_reg = max30102_read_reg(REG_MODE_CONFIG);
    printf("MAX30102 mode reg: 0x%02X\r\n", mode_reg);

    MAX30102_Config();
    printf("MAX30102 config done\r\n");

    /* 4. LED 初始化 */
    uapi_pin_set_mode(BSP_LED, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(BSP_LED, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(BSP_LED, GPIO_LEVEL_LOW);

    /* ===== 定时器子系统：只初始化一次，不在循环内重复 adapter ===== */
    uapi_timer_init();
    ret = uapi_timer_adapter(TIMER_INDEX, TIMER_IRQ, TIMER_IRQ_PRIO);
    if (ret != ERRCODE_SUCC) {
        printf("Timer adapter failed: 0x%x\r\n", ret);
        return -1;
    }
    printf("Timer adapter done once\r\n");

    /* 主循环：多次采集 */
    while (1) {
        /* ===== 阶段1：10秒数据采集 ===== */
        printf("\r\n--- Starting 10s data collection ---\r\n");

        sample_index = 0;
        collection_done = 0;
        timer_count = 0;
        memset(ir_buffer, 0, sizeof(ir_buffer));
        memset(red_buffer, 0, sizeof(red_buffer));

        /* 清空FIFO */
        max30102_clear_fifo();
        delay_ms(10);

        /* 启动定时器 */
        timer_init();

        /* 采集循环 */
        while (!collection_done) {
            if (timer_flag) {
                timer_flag = 0;

                uint8_t wr_ptr = max30102_read_reg(REG_FIFO_WR_PTR);
                uint8_t rd_ptr = max30102_read_reg(REG_FIFO_RD_PTR);
                uint8_t num_samples = (wr_ptr - rd_ptr) & 0x1F;  // FIFO 中待读样本数
                printf("FIFO: wr=%d, rd=%d, cnt=%d\r\n", wr_ptr, rd_ptr, num_samples);

                /* 读取FIFO */
                max30102_read_fifo();

                /* 存入缓冲区 */
                if (sample_index < TOTAL_SAMPLES) {
                    ir_buffer[sample_index] = fifo_ir;
                    red_buffer[sample_index] = fifo_red;
                    sample_index++;
                }

                /* 更新显示 */
                if (sample_index % 10 == 0) {
                    display_values(fifo_ir, fifo_red, sample_index);
                }

                /* LED指示 */
                if (fifo_ir > 10000) {
                    uapi_gpio_set_val(BSP_LED, GPIO_LEVEL_HIGH);
                } else {
                    uapi_gpio_set_val(BSP_LED, GPIO_LEVEL_LOW);
                }

                /* 检查是否完成 */
                if (sample_index >= TOTAL_SAMPLES) {
                    collection_done = 1;
                }
            }
            delay_ms(1);
        }

        /* 停止定时器 */
        timer_deinit();
        printf("Collection done! Samples: %d, Timer count: %lu\r\n", sample_index, timer_count);

        /* ===== 阶段2：数据处理 ===== */
        printf("\r\n--- Processing data ---\r\n");

        printf("First 20 samples (IR):\r\n");
        for (uint16_t i = 0; i < 20 && i < TOTAL_SAMPLES; i++) {
            printf("%lu ", ir_buffer[i]);
            if ((i+1) % 10 == 0) printf("\r\n");
        }

        printf("IR waveform sample:\r\n");
        for (uint16_t i = 0; i < 200; i++) {
            printf("%lu ", ir_buffer[i]);
            if ((i+1) % 20 == 0) printf("\r\n");
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
            display_result(0, 0);  // 显示 Invalid
            continue;  // 跳过本次心率计算
        }

        uint16_t hr = calculate_heart_rate((uint32_t *)ir_buffer, TOTAL_SAMPLES);
        uint8_t hr_valid = (hr > 0) ? 1 : 0;

        display_result(hr, hr_valid);

        printf("Heart Rate: %d BPM (%s)\r\n", hr, hr_valid ? "Valid" : "Invalid");

        /* ===== 阶段3：等待下一次采集 ===== */
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