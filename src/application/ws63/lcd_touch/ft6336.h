#ifndef _FT6336_H_
#define _FT6336_H_

#include <stdint.h>
#include <stdbool.h>

#define FT6336_I2C_ADDR          0x38

#define FT6336_REG_MODE          0x00
#define FT6336_REG_GESTURE       0x01
#define FT6336_REG_STATUS        0x02
#define FT6336_REG_TOUCH1_XH     0x03
#define FT6336_REG_TOUCH1_XL     0x04
#define FT6336_REG_TOUCH1_YH     0x05
#define FT6336_REG_TOUCH1_YL     0x06
#define FT6336_REG_TOUCH_WEIGHT  0x07
#define FT6336_REG_TOUCH_AREA    0x08
#define FT6336_REG_TOUCH2_XH     0x09
#define FT6336_REG_TOUCH2_XL     0x0A
#define FT6336_REG_TOUCH2_YH     0x0B
#define FT6336_REG_TOUCH2_YL     0x0C
#define FT6336_REG_TH_GROUP      0x80
#define FT6336_REG_TH_DIFF       0x85
#define FT6336_REG_CTRL          0x86
#define FT6336_REG_TIME_ENTER_MONITOR 0x87
#define FT6336_REG_PERIOD_ACTIVE  0x88
#define FT6336_REG_PERIOD_MONITOR 0x89
#define FT6336_REG_RADIAN_VALUE  0x91
#define FT6336_REG_OFFSET_LEFT_RIGHT 0x92
#define FT6336_REG_OFFSET_UP_DOWN 0x93
#define FT6336_REG_DISTANCE_LEFT_RIGHT 0x94
#define FT6336_REG_DISTANCE_UP_DOWN 0x95
#define FT6336_REG_LIB_VER_H     0xA1
#define FT6336_REG_LIB_VER_L     0xA2
#define FT6336_REG_CHIP_ID_H     0xA3
#define FT6336_REG_CHIP_ID_L     0xA4
#define FT6336_REG_FIRMWARE_VERS 0xA6
#define FT6336_REG_VENDOR_ID     0xA8

#define FT6336_MAX_TOUCH_POINTS  2

typedef struct {
    uint16_t x;
    uint16_t y;
    uint8_t  weight;
    uint8_t  area;
    uint8_t  event;
} ft6336_touch_point_t;

typedef struct {
    uint8_t touch_count;
    uint8_t gesture;
    ft6336_touch_point_t points[FT6336_MAX_TOUCH_POINTS];
} ft6336_touch_data_t;

void ft6336_init(void);
bool ft6336_read_touch(ft6336_touch_data_t *touch_data);
uint16_t ft6336_get_chip_id(void);
uint8_t ft6336_get_firmware_version(void);

#endif