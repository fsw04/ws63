#include "display_port.h"
#include "pinctrl.h"
#include "spi.h"
#include "soc_osal.h"
#include "app_init.h"
#include "gpio.h"
#include "timer.h"
#include "chip_core_irq.h"

#define SPI_CLK_POLARITY 0
#define SPI_CLK_PHASE 0
#define SPI_SLAVE_NUM 1

#define HIGH_BYTE_SHIFT 8
#define LOW_BYTE_MASK 0xFF

#define ST7796_CASET 0x2A
#define ST7796_RASET 0x2B
#define ST7796_RAMWR 0x2C

#define TIMER_INDEX 1
#define TIMER_PRIORITY 1
#define TICK_US 1000

static timer_handle_t timer_handle = NULL;
static uint8_t disp_buf[DISP_HOR_RES * DISP_BUF_ROWS * BYTE_PER_PIXEL];

static void spi_init(void)
{
    uapi_pin_set_mode(LCD_MOSI_PIN, SPI_PIN_MODE);
    uapi_pin_set_mode(LCD_SCK_PIN, SPI_PIN_MODE);
    uapi_pin_set_mode(LCD_MISO_PIN, SPI_PIN_MODE);

    uapi_pin_set_mode(LCD_CS_PIN, GPIO_PIN_MODE);
    uapi_gpio_set_dir(LCD_CS_PIN, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(LCD_CS_PIN, GPIO_LEVEL_HIGH);

    uapi_pin_set_mode(LCD_RESET_PIN, GPIO_PIN_MODE);
    uapi_gpio_set_dir(LCD_RESET_PIN, GPIO_DIRECTION_OUTPUT);

    uapi_pin_set_mode(LCD_DC_PIN, GPIO_PIN_MODE);
    uapi_gpio_set_dir(LCD_DC_PIN, GPIO_DIRECTION_OUTPUT);

    uapi_pin_set_mode(LCD_BL_PIN, GPIO_PIN_MODE);
    uapi_gpio_set_dir(LCD_BL_PIN, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(LCD_BL_PIN, GPIO_LEVEL_HIGH);

    spi_attr_t config = {0};
    spi_extra_attr_t ext_config = {0};

    config.is_slave = false;
    config.slave_num = SPI_SLAVE_NUM;
    config.bus_clk = SPI_CLK_FREQ;
    config.freq_mhz = SPI_FREQUENCY;
    config.clk_polarity = SPI_CLK_POLARITY;
    config.clk_phase = SPI_CLK_PHASE;
    config.frame_format = SPI_CFG_FRAME_FORMAT_MOTOROLA_SPI;
    config.spi_frame_format = HAL_SPI_FRAME_FORMAT_STANDARD;
    config.frame_size = HAL_SPI_FRAME_SIZE_8;
    config.tmod = HAL_SPI_TRANS_MODE_TX;
    config.sste = 0;

    int ret = uapi_spi_init(LCD_SPI_BUS, &config, &ext_config);
    if (ret != 0) {
        osal_printk("spi init fail %0x\r\n", ret);
    }
    uapi_dma_init();
    uapi_dma_open();
}

static void send_cmd(uint8_t cmd)
{
    uint8_t d = cmd;
    spi_xfer_data_t spi_data = {
        .tx_buff = &d,
        .tx_bytes = 1,
    };
    uapi_gpio_set_val(LCD_CS_PIN, GPIO_LEVEL_LOW);
    uapi_gpio_set_val(LCD_DC_PIN, GPIO_LEVEL_LOW);
    uapi_spi_master_write(LCD_SPI_BUS, &spi_data, 0xFFFFFFFF);
    uapi_gpio_set_val(LCD_CS_PIN, GPIO_LEVEL_HIGH);
}

static void send_data(uint8_t data)
{
    uint8_t d = data;
    spi_xfer_data_t spi_data = {
        .tx_buff = &d,
        .tx_bytes = 1,
    };
    uapi_gpio_set_val(LCD_CS_PIN, GPIO_LEVEL_LOW);
    uapi_gpio_set_val(LCD_DC_PIN, GPIO_LEVEL_HIGH);
    uapi_spi_master_write(LCD_SPI_BUS, &spi_data, 0xFFFFFFFF);
    uapi_gpio_set_val(LCD_CS_PIN, GPIO_LEVEL_HIGH);
}

static void send_data_array(uint8_t *data, uint32_t len)
{
    spi_xfer_data_t spi_data = {
        .tx_buff = data,
        .tx_bytes = len,
    };
    uapi_gpio_set_val(LCD_CS_PIN, GPIO_LEVEL_LOW);
    uapi_gpio_set_val(LCD_DC_PIN, GPIO_LEVEL_HIGH);
    uapi_spi_master_write(LCD_SPI_BUS, &spi_data, 0xFFFFFFFF);
    uapi_gpio_set_val(LCD_CS_PIN, GPIO_LEVEL_HIGH);
}

static void st7796_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    {
        uint8_t data[] = {x0 >> HIGH_BYTE_SHIFT, x0 & LOW_BYTE_MASK, x1 >> HIGH_BYTE_SHIFT, x1 & LOW_BYTE_MASK};
        send_cmd(ST7796_CASET);
        send_data_array(data, sizeof(data));
    }

    {
        uint8_t data[] = {y0 >> HIGH_BYTE_SHIFT, y0 & LOW_BYTE_MASK, y1 >> HIGH_BYTE_SHIFT, y1 & LOW_BYTE_MASK};
        send_cmd(ST7796_RASET);
        send_data_array(data, sizeof(data));
    }
}

static void st7796_write_data(uint8_t *data, uint32_t len)
{
    send_cmd(ST7796_RAMWR);
    send_data_array(data, len);
}

static void st7796_init(void)
{
    spi_init();

    uapi_gpio_set_val(LCD_RESET_PIN, GPIO_LEVEL_LOW);
    osal_msleep(120);
    uapi_gpio_set_val(LCD_RESET_PIN, GPIO_LEVEL_HIGH);
    osal_msleep(120);

    send_cmd(0x01);
    osal_msleep(120);

    send_cmd(0x11);
    osal_msleep(20);

    send_cmd(0x3A);
    send_data(0x55);

    send_cmd(0x36);
    send_data(0x00);

    send_cmd(0xB2);
    send_data(0x0C);
    send_data(0x0C);
    send_data(0x00);
    send_data(0x33);
    send_data(0x33);

    send_cmd(0xB7);
    send_data(0x35);

    send_cmd(0xBB);
    send_data(0x32);

    send_cmd(0xC0);
    send_data(0x2C);

    send_cmd(0xC2);
    send_data(0x01);

    send_cmd(0xC3);
    send_data(0x19);

    send_cmd(0xC4);
    send_data(0x20);

    send_cmd(0xC6);
    send_data(0x0F);

    send_cmd(0xD0);
    send_data(0xA4);
    send_data(0xA1);

    send_cmd(0xE0);
    send_data(0xD0); send_data(0x08); send_data(0x0E); send_data(0x09);
    send_data(0x09); send_data(0x05); send_data(0x31); send_data(0x33);
    send_data(0x48); send_data(0x17); send_data(0x14); send_data(0x15);
    send_data(0x31); send_data(0x34);

    send_cmd(0xE1);
    send_data(0xD0); send_data(0x08); send_data(0x0E); send_data(0x09);
    send_data(0x09); send_data(0x15); send_data(0x31); send_data(0x33);
    send_data(0x48); send_data(0x17); send_data(0x14); send_data(0x15);
    send_data(0x31); send_data(0x34);

    send_cmd(0x21);

    send_cmd(0x29);
    osal_msleep(20);
}

static void disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    st7796_set_window(area->x1, area->y1, area->x2, area->y2);
    st7796_write_data(px_map, (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1) * BYTE_PER_PIXEL);
    lv_display_flush_ready(disp);
}

static void timer_callback(uintptr_t data)
{
    unused(data);
    lv_tick_inc(1);
    uapi_timer_start(timer_handle, TICK_US, timer_callback, 0);
}

void display_set_backlight(bool on)
{
    uapi_gpio_set_val(LCD_BL_PIN, on ? GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW);
}

void display_port_init(void)
{
    st7796_init();
    display_set_backlight(true);

    lv_init();

    lv_display_t *disp = lv_display_create(DISP_HOR_RES, DISP_VER_RES);
    lv_display_set_flush_cb(disp, disp_flush);
    lv_display_set_buffers(disp, disp_buf, NULL, sizeof(disp_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);

    uapi_timer_init();
    uapi_timer_adapter(TIMER_INDEX, TIMER_1_IRQN, TIMER_PRIORITY);
    uapi_timer_create(TIMER_INDEX, &timer_handle);
    uapi_timer_start(timer_handle, TICK_US, timer_callback, 0);
}
