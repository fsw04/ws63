# ST7796S + FT6336U sample

This sample drives a 320x480 ST7796S SPI LCD and an FT6336U I2C touch panel with LVGL 9.

## Default wiring

LCD SPI0:

- GPIO_07: SPI0_SCK
- GPIO_09: SPI0_MOSI
- GPIO_10: SPI0_CS0_N
- GPIO_11: SPI0_MISO, optional
- GPIO_06: LCD DC/RS
- GPIO_03: LCD RESET
- GPIO_08: LCD BL/LEDA

Touch I2C1:

- GPIO_15: I2C1_SDA
- GPIO_16: I2C1_SCL
- GPIO_12: FT6336U RST
- GPIO_13: FT6336U INT, optional and unused by default

All logic pins are 3.3 V. Connect the backlight cathode through a current-limiting path to ground as required by the module.

## Enable

Enable these Kconfig options:

- `SAMPLE_ENABLE`
- `ENABLE_PERIPHERAL_SAMPLE`
- `SAMPLE_SUPPORT_ST7796S_FT6336U`

The sample selects GPIO, pinctrl, SPI master, I2C master, and timer driver support. The `ws63-liteos-app` target already includes the `lvgl` component in `ram_component`.

## First run

On boot the LCD fills red, green, blue, white, then black before LVGL starts. LVGL then shows a label, slider, and button. Touch coordinates are printed as `points,x,y,event` while pressed.

If colors look byte-swapped, toggle `ST7796S_LVGL_RGB565_SWAPPED`. If orientation is wrong, adjust `ST7796S_MADCTL` and update touch coordinate mapping in `touch_read()`.
