#ifndef FT6336U_H
#define FT6336U_H

#include <stdint.h>
#include <stdbool.h>

#define FT6336U_I2C_ADDR    0x38
#define FT6336U_I2C_BUS    1

#define FT6336U_TOUCH_SDA_PIN   15
#define FT6336U_TOUCH_SCL_PIN   16
#define FT6336U_TOUCH_RST_PIN   13
#define FT6336U_TOUCH_INT_PIN   2

#define FT6336U_SDA_PIN_MODE    2
#define FT6336U_SCL_PIN_MODE    2

#define FT6336U_MAX_TOUCHES     2

typedef struct {
    uint8_t count;
    uint16_t x[FT6336U_MAX_TOUCHES];
    uint16_t y[FT6336U_MAX_TOUCHES];
    uint8_t event[FT6336U_MAX_TOUCHES];
} ft6336u_touch_t;

void ft6336u_init(void);
bool ft6336u_read_touch(ft6336u_touch_t *touch);

#endif
