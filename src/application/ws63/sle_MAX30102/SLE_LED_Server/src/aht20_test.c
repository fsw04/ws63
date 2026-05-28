/*
 * Copyright (c) 2024 HiSilicon Technologies CO., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "pinctrl.h"
#include "i2c.h"
#include "osal_debug.h"
#include "../inc/ssd1306_fonts.h"
#include "../inc/ssd1306.h"
#include "soc_osal.h"
#include "app_init.h"

#include "../inc/MAX30102.h"
#include "../inc/blood.h"
#include "watchdog.h"

#define CONFIG_I2C_SCL_MASTER_PIN 16
#define CONFIG_I2C_SDA_MASTER_PIN 15
#define CONFIG_I2C_MASTER_PIN_MODE 2
#define I2C_MASTER_ADDR 0x0
#define I2C_SET_BANDRATE 400000
#define I2C_TASK_STACK_SIZE 0x4000
#define I2C_TASK_PRIO 17

const unsigned char headSize[] = {64, 64};

int temp = 0;
int humi = 0;


void app_i2c_init_pin(void)
{
    uapi_pin_set_mode(CONFIG_I2C_SCL_MASTER_PIN, CONFIG_I2C_MASTER_PIN_MODE);
    uapi_pin_set_mode(CONFIG_I2C_SDA_MASTER_PIN, CONFIG_I2C_MASTER_PIN_MODE);
}


void TempHumChinese(void)
{
    const uint32_t W = 16;
    uint8_t fonts[][32] = {
        {/* -- ID:0,字符:"心",ASCII编码:CEC2,对应字:宽x高=16x16,画布:宽W=16 高H=16,共32字节 */
         0x02,0x00,0x01,0x00,0x00,0x80,0x00,0xC0,0x08,0x80,0x08,0x00,0x28,0x08,0x28,0x04,
         0x28,0x02,0x48,0x02,0x88,0x02,0x08,0x00,0x08,0x10,0x08,0x10,0x07,0xF0,0x00,0x00},
        {/* -- ID:0,字符:"率",ASCII编码:B6C8,对应字:宽x高=16x16,画布:宽W=16 高H=16,共32字节 */
         0x02,0x00,0x01,0x08,0x7F,0xFC,0x01,0x00,0x42,0x44,0x27,0x88,0x11,0x10,0x22,0x48,
         0x4F,0xE4,0x01,0x20,0x01,0x04,0xFF,0xFE,0x01,0x00,0x01,0x00,0x01,0x00,0x01,0x00}};
    uint8_t fonts2[][32] = {
        {/* -- ID:0,字符:"血",ASCII编码:CEC2,对应字:宽x高=16x16,画布:宽W=16 高H=16,共32字节 */
         0x00,0x00,0x01,0x00,0x01,0x00,0x02,0x08,0x3F,0xFC,0x24,0x48,0x24,0x48,0x24,0x48,
         0x24,0x48,0x24,0x48,0x24,0x48,0x24,0x48,0x24,0x48,0x24,0x48,0xFF,0xFE,0x00,0x00},
        {/* -- ID:0,字符:"氧",ASCII编码:B6C8,对应字:宽x高=16x16,画布:宽W=16 高H=16,共32字节 */
         0x10,0x00,0x1F,0xFC,0x20,0x00,0x2F,0xF8,0x40,0x00,0xBF,0xF8,0x08,0x88,0x05,0x08,
         0x3F,0xE8,0x02,0x08,0x1F,0xC8,0x02,0x08,0x7F,0xFA,0x02,0x0A,0x02,0x04,0x02,0x00}};      
    for (size_t i = 0; i < sizeof(fonts) / sizeof(fonts[0]); i++) {
        ssd1306_DrawRegion(i * W, 3, W, fonts[i], sizeof(fonts[0]));
    }
    for (size_t j = 0; j < sizeof(fonts2) / sizeof(fonts2[0]); j++) {
        ssd1306_DrawRegion(j * W, 35, W, fonts2[j], sizeof(fonts2[0]));
    }
}


void sever_xianshi(int x, int y)
{
    ssd1306_Fill(Black);
    TempHumChinese();

    static char templine[32] = {0};
    static char humiline[32] = {0};

    /* 心率显示：负数时显示 "---" */
    ssd1306_SetCursor(32, 8);
    if (x < 0) {
        sprintf(templine, ": ---");
    } else {
        sprintf(templine, ": %d", x);
    }
    ssd1306_DrawString(templine, Font_7x10, White);

    /* 血氧显示：负数时显示 "--" */
    ssd1306_SetCursor(32, 40);
    if (y < 0) {
        sprintf(humiline, ": ---");
    } else {
        sprintf(humiline, ": %d", y);
    }
    ssd1306_DrawString(humiline, Font_7x10, White);

    ssd1306_UpdateScreen();
}

void xianshiinit(void)
{
    printf("1111111111\r\n");
    ssd1306_Init();
    ssd1306_Fill(Black);
    ssd1306_SetCursor(0, 0);
    printf("2222222222\r\n");
}


void Aht20TestTask(void)
{
    uint32_t baudrate = I2C_SET_BANDRATE;
    uint32_t hscode = I2C_MASTER_ADDR;

    app_i2c_init_pin();
    errcode_t ret = uapi_i2c_master_init(1, baudrate, hscode);
    if (ret != 0) {
        printf("i2c init failed, ret = %0x\r\n", ret);
    }
    ssd1306_Init();
    ssd1306_Fill(Black);
    ssd1306_SetCursor(0, 0);

    printf("===============================\r\n");

    /* STM32移植: MAX30102初始化 */
    Max30102_reset();
    osal_mdelay(10);
    uint8_t X = max30102_read_reg(REG_MODE_CONFIG);
    if ((X & 0x40) == 0x40) {
        printf("MAX30102初始化完成0\r\n");
    }

    MAX30102_Config();
    printf("MAX30102初始化完成1\r\n");

    blood_data_update();
}