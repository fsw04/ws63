#ifndef ST7796S_H
#define ST7796S_H

#include <stdint.h>
#include "errcode.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ST7796S_WIDTH 320
#define ST7796S_HEIGHT 480

errcode_t st7796s_init(void);
errcode_t st7796s_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
errcode_t st7796s_write_pixels(const uint8_t *data, uint32_t len);
errcode_t st7796s_fill_color(uint16_t color);

#ifdef __cplusplus
}
#endif

#endif
