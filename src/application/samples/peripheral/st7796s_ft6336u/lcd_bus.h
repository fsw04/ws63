#ifndef LCD_BUS_H
#define LCD_BUS_H

#include <stdint.h>
#include "errcode.h"

#ifdef __cplusplus
extern "C" {
#endif

errcode_t lcd_bus_init(void);
void lcd_bus_reset(void);
void lcd_bus_backlight_set(uint8_t enabled);
errcode_t lcd_bus_send_cmd(uint8_t cmd);
errcode_t lcd_bus_send_data(uint8_t data);
errcode_t lcd_bus_send_data_array(const uint8_t *data, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif
