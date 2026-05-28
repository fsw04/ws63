#ifndef DISPLAY_PORT_H
#define DISPLAY_PORT_H

#include "lvgl.h"

#define DISP_HOR_RES 320
#define DISP_VER_RES 480
#define DISP_BUF_ROWS 20
#define BYTE_PER_PIXEL 2

#define LCD_CS_PIN      8
#define LCD_RESET_PIN   14
#define LCD_DC_PIN      1
#define LCD_MOSI_PIN    9
#define LCD_SCK_PIN     7
#define LCD_MISO_PIN    11
#define LCD_BL_PIN      12
#define LCD_SPI_BUS     0

#define SPI_FREQUENCY   10
#define SPI_PIN_MODE    3
#define GPIO_PIN_MODE   0

void display_port_init(void);
void display_set_backlight(bool on);

#endif
