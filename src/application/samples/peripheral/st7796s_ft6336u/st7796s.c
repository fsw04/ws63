#include "st7796s.h"

#include "lcd_bus.h"
#include "soc_osal.h"

#define ST7796S_SLPOUT 0x11
#define ST7796S_DISPON 0x29
#define ST7796S_CASET 0x2A
#define ST7796S_RASET 0x2B
#define ST7796S_RAMWR 0x2C
#define ST7796S_MADCTL 0x36
#define ST7796S_COLMOD 0x3A

#define ST7796S_RGB565 0x55
#define ST7796S_HIGH_BYTE_SHIFT 8
#define ST7796S_LOW_BYTE_MASK 0xFF
#define ST7796S_FILL_CHUNK_PIXELS 256

static errcode_t st7796s_write_cmd_data(uint8_t cmd, const uint8_t *data, uint32_t len)
{
    errcode_t ret = lcd_bus_send_cmd(cmd);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    return lcd_bus_send_data_array(data, len);
}

static errcode_t st7796s_write_cmd_byte(uint8_t cmd, uint8_t data)
{
    errcode_t ret = lcd_bus_send_cmd(cmd);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    return lcd_bus_send_data(data);
}

errcode_t st7796s_init(void)
{
    static const uint8_t porch[] = {0x01};
    static const uint8_t display_function[] = {0xC6};
    static const uint8_t display_output[] = {0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33};
    static const uint8_t positive_gamma[] = {0xF0, 0x09, 0x0B, 0x06, 0x04, 0x15, 0x2F,
                                             0x54, 0x42, 0x3C, 0x17, 0x14, 0x18, 0x1B};
    static const uint8_t negative_gamma[] = {0xF0, 0x09, 0x0B, 0x06, 0x04, 0x03, 0x2D,
                                             0x43, 0x42, 0x3B, 0x16, 0x14, 0x17, 0x1B};
    errcode_t ret;

    ret = lcd_bus_init();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    lcd_bus_reset();
    osal_msleep(120);

    ret = lcd_bus_send_cmd(ST7796S_SLPOUT);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    osal_msleep(120);

    ret = st7796s_write_cmd_byte(0xF0, 0xC3);
    ret |= st7796s_write_cmd_byte(0xF0, 0x96);
    ret |= st7796s_write_cmd_byte(ST7796S_MADCTL, CONFIG_ST7796S_MADCTL);
    ret |= st7796s_write_cmd_byte(ST7796S_COLMOD, ST7796S_RGB565);
    ret |= st7796s_write_cmd_data(0xB4, porch, sizeof(porch));
    ret |= st7796s_write_cmd_data(0xB7, display_function, sizeof(display_function));
    ret |= st7796s_write_cmd_data(0xE8, display_output, sizeof(display_output));
    ret |= st7796s_write_cmd_byte(0xC1, 0x06);
    ret |= st7796s_write_cmd_byte(0xC2, 0xA7);
    ret |= st7796s_write_cmd_byte(0xC5, 0x18);
    ret |= st7796s_write_cmd_data(0xE0, positive_gamma, sizeof(positive_gamma));
    ret |= st7796s_write_cmd_data(0xE1, negative_gamma, sizeof(negative_gamma));
    ret |= st7796s_write_cmd_byte(0xF0, 0x3C);
    ret |= st7796s_write_cmd_byte(0xF0, 0x69);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    osal_msleep(120);
    ret = lcd_bus_send_cmd(ST7796S_DISPON);
    if (ret == ERRCODE_SUCC) {
        lcd_bus_backlight_set(1);
    }

    return ret;
}

errcode_t st7796s_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t col[] = {
        x0 >> ST7796S_HIGH_BYTE_SHIFT,
        x0 & ST7796S_LOW_BYTE_MASK,
        x1 >> ST7796S_HIGH_BYTE_SHIFT,
        x1 & ST7796S_LOW_BYTE_MASK,
    };
    uint8_t row[] = {
        y0 >> ST7796S_HIGH_BYTE_SHIFT,
        y0 & ST7796S_LOW_BYTE_MASK,
        y1 >> ST7796S_HIGH_BYTE_SHIFT,
        y1 & ST7796S_LOW_BYTE_MASK,
    };

    errcode_t ret = st7796s_write_cmd_data(ST7796S_CASET, col, sizeof(col));
    ret |= st7796s_write_cmd_data(ST7796S_RASET, row, sizeof(row));
    return ret;
}

errcode_t st7796s_write_pixels(const uint8_t *data, uint32_t len)
{
    errcode_t ret = lcd_bus_send_cmd(ST7796S_RAMWR);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    return lcd_bus_send_data_array(data, len);
}

errcode_t st7796s_fill_color(uint16_t color)
{
    uint8_t line[ST7796S_FILL_CHUNK_PIXELS * 2];
    uint32_t pixels_left = ST7796S_WIDTH * ST7796S_HEIGHT;
    uint32_t chunk_pixels;
    errcode_t ret;

    for (uint32_t i = 0; i < ST7796S_FILL_CHUNK_PIXELS; i++) {
        line[(i * 2)] = color >> ST7796S_HIGH_BYTE_SHIFT;
        line[(i * 2) + 1] = color & ST7796S_LOW_BYTE_MASK;
    }

    ret = st7796s_set_window(0, 0, ST7796S_WIDTH - 1, ST7796S_HEIGHT - 1);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = lcd_bus_send_cmd(ST7796S_RAMWR);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    while (pixels_left > 0) {
        chunk_pixels = (pixels_left > ST7796S_FILL_CHUNK_PIXELS) ? ST7796S_FILL_CHUNK_PIXELS : pixels_left;
        ret = lcd_bus_send_data_array(line, chunk_pixels * 2);
        if (ret != ERRCODE_SUCC) {
            return ret;
        }
        pixels_left -= chunk_pixels;
    }

    return ERRCODE_SUCC;
}
