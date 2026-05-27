#include "display_port.h"
#include "pinctrl.h"
#include "spi.h"
#include "soc_osal.h"
#include "app_init.h"
#include "gpio.h"
#include "timer.h"
#include "chip_core_irq.h"

#define SPI_FREQUENCY 32
#define SPI_CLK_POLARITY 0
#define SPI_CLK_PHASE 0
#define SPI_SLAVE_NUM 1

#define GPIO_PINMODE 0
#define SPI_PINMODE 3

#define HIGH_BYTE_SHIFT 8
#define LOW_BYTE_MASK 0xFF

#define ST7789_NOP 0x00
#define ST7789_SWRESET 0x01
#define ST7789_RDDID 0x04
#define ST7789_RDDST 0x09
#define ST7789_SLPIN 0x10
#define ST7789_SLPOUT 0x11
#define ST7789_PTLON 0x12
#define ST7789_NORON 0x13
#define ST7789_INVOFF 0x20
#define ST7789_INVON 0x21
#define ST7789_DISPOFF 0x28
#define ST7789_DISPON 0x29
#define ST7789_CASET 0x2A
#define ST7789_RASET 0x2B
#define ST7789_RAMWR 0x2C
#define ST7789_RAMRD 0x2E
#define ST7789_PTLAR 0x30
#define ST7789_COLMOD 0x3A
#define ST7789_MADCTL 0x36

#define ST7789_WIDTH 320
#define ST7789_HEIGHT 480

#define TIMER_INDEX 1
#define TIMER_PRIORITY 1
#define TICK_US 1000

static timer_handle_t timer_handle = NULL;
static uint8_t disp_buf[DISP_HOR_RES * DISP_BUF_ROWS * BYTE_PER_PIXEL];

static void spi_init(void)
{
    uapi_pin_set_mode(CONFIG_SPI_DI_MASTER_PIN, SPI_PINMODE);
    uapi_pin_set_mode(CONFIG_SPI_DO_MASTER_PIN, SPI_PINMODE);
    uapi_pin_set_mode(CONFIG_SPI_CLK_MASTER_PIN, SPI_PINMODE);
    uapi_pin_set_mode(CONFIG_SPI_CS_MASTER_PIN, SPI_PINMODE);

    uapi_pin_set_mode(CONFIG_SPI_RESET_MASTER_PIN, GPIO_PINMODE);
    uapi_gpio_set_dir(CONFIG_SPI_RESET_MASTER_PIN, GPIO_DIRECTION_OUTPUT);
    uapi_pin_set_mode(CONFIG_SPI_DC_MASTER_PIN, GPIO_PINMODE);
    uapi_gpio_set_dir(CONFIG_SPI_DC_MASTER_PIN, GPIO_DIRECTION_OUTPUT);

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

    int ret = uapi_spi_init(CONFIG_SPI_MASTER_BUS_ID, &config, &ext_config);
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
    uapi_gpio_set_val(CONFIG_SPI_DC_MASTER_PIN, GPIO_LEVEL_LOW);
    uapi_spi_master_write(CONFIG_SPI_MASTER_BUS_ID, &spi_data, 0xFFFFFFFF);
}

static void send_data(uint8_t data)
{
    uint8_t d = data;
    spi_xfer_data_t spi_data = {
        .tx_buff = &d,
        .tx_bytes = 1,
    };
    uapi_gpio_set_val(CONFIG_SPI_DC_MASTER_PIN, GPIO_LEVEL_HIGH);
    uapi_spi_master_write(CONFIG_SPI_MASTER_BUS_ID, &spi_data, 0xFFFFFFFF);
}

static void send_data_array(uint8_t *data, uint32_t len)
{
    spi_xfer_data_t spi_data = {
        .tx_buff = data,
        .tx_bytes = len,
    };
    uapi_gpio_set_val(CONFIG_SPI_DC_MASTER_PIN, GPIO_LEVEL_HIGH);
    uapi_spi_master_write(CONFIG_SPI_MASTER_BUS_ID, &spi_data, 0xFFFFFFFF);
}

static void st7789_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    {
        uint8_t data[] = {x0 >> HIGH_BYTE_SHIFT, x0 & LOW_BYTE_MASK, x1 >> HIGH_BYTE_SHIFT, x1 & LOW_BYTE_MASK};
        send_cmd(ST7789_CASET);
        send_data_array(data, sizeof(data));
    }

    {
        uint8_t data[] = {y0 >> HIGH_BYTE_SHIFT, y0 & LOW_BYTE_MASK, y1 >> HIGH_BYTE_SHIFT, y1 & LOW_BYTE_MASK};
        send_cmd(ST7789_RASET);
        send_data_array(data, sizeof(data));
    }
}

static void st7789_write_data(uint8_t *data, uint32_t len)
{
    send_cmd(ST7789_RAMWR);
    send_data_array(data, len);
}

static void st7789_init(void)
{
    spi_init();
    osal_printk("spi init\r\n");

    uapi_gpio_set_val(CONFIG_SPI_RESET_MASTER_PIN, GPIO_LEVEL_LOW);
    osal_msleep(100);
    uapi_gpio_set_val(CONFIG_SPI_RESET_MASTER_PIN, GPIO_LEVEL_HIGH);
    osal_msleep(100);

    send_cmd(ST7789_SLPOUT);

    send_cmd(ST7789_COLMOD);
    send_data(0x55);

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
    send_data(0xD0);
    send_data(0x08);
    send_data(0x0E);
    send_data(0x09);
    send_data(0x09);
    send_data(0x05);
    send_data(0x31);
    send_data(0x33);
    send_data(0x48);
    send_data(0x17);
    send_data(0x14);
    send_data(0x15);
    send_data(0x31);
    send_data(0x34);

    send_cmd(0xE1);
    send_data(0xD0);
    send_data(0x08);
    send_data(0x0E);
    send_data(0x09);
    send_data(0x09);
    send_data(0x15);
    send_data(0x31);
    send_data(0x33);
    send_data(0x48);
    send_data(0x17);
    send_data(0x14);
    send_data(0x15);
    send_data(0x31);
    send_data(0x34);

    send_cmd(ST7789_INVON);

    send_cmd(ST7789_MADCTL);
    send_data(0x00);

    send_cmd(ST7789_DISPON);
}

static void disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    st7789_set_window(area->x1, area->y1, area->x2, area->y2);
    st7789_write_data(px_map, (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1) * BYTE_PER_PIXEL);
    lv_display_flush_ready(disp);
}

static void timer_callback(uintptr_t data)
{
    unused(data);
    lv_tick_inc(1);
    uapi_timer_start(timer_handle, TICK_US, timer_callback, 0);
}

void display_port_init(void)
{
    st7789_init();

    lv_init();

    lv_display_t *disp = lv_display_create(DISP_HOR_RES, DISP_VER_RES);
    lv_display_set_flush_cb(disp, disp_flush);
    lv_display_set_buffers(disp, disp_buf, NULL, sizeof(disp_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);

    uapi_timer_init();
    uapi_timer_adapter(TIMER_INDEX, TIMER_1_IRQN, TIMER_PRIORITY);
    uapi_timer_create(TIMER_INDEX, &timer_handle);
    uapi_timer_start(timer_handle, TICK_US, timer_callback, 0);
}
