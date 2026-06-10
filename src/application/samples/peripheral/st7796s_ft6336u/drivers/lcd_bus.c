#include "lcd_bus.h"

#include "gpio.h"
#include "pinctrl.h"
#include "soc_osal.h"
#include "spi.h"

#if defined(CONFIG_SPI_SUPPORT_DMA) && (CONFIG_SPI_SUPPORT_DMA == 1)
#include "dma.h"
#endif

#define LCD_BUS_SPI_CLK_POLARITY 0
#define LCD_BUS_SPI_CLK_PHASE 0
#define LCD_BUS_SPI_SLAVE_NUM 1
#define LCD_BUS_GPIO_PIN_MODE 0
#define LCD_BUS_WRITE_TIMEOUT 0xFFFFFFFF

static errcode_t lcd_bus_write_byte(uint8_t data)
{
    spi_xfer_data_t spi_data = {
        .tx_buff = &data,
        .tx_bytes = 1,
    };

    return uapi_spi_master_write(CONFIG_ST7796S_SPI_BUS_ID, &spi_data, LCD_BUS_WRITE_TIMEOUT);
}

errcode_t lcd_bus_init(void)
{
    osal_printk("st7796s bus init: spi%d sck=%d mosi=%d cs=%d dc=%d rst=%d bl=%d\r\n",
                CONFIG_ST7796S_SPI_BUS_ID, CONFIG_ST7796S_SPI_CLK_PIN, CONFIG_ST7796S_SPI_MOSI_PIN,
                CONFIG_ST7796S_SPI_CS_PIN, CONFIG_ST7796S_LCD_DC_PIN, CONFIG_ST7796S_LCD_RESET_PIN,
                CONFIG_ST7796S_LCD_BL_PIN);

    uapi_pin_init();
    uapi_gpio_init();

    uapi_pin_set_mode(CONFIG_ST7796S_SPI_MISO_PIN, CONFIG_ST7796S_SPI_PIN_MODE);
    uapi_pin_set_mode(CONFIG_ST7796S_SPI_MOSI_PIN, CONFIG_ST7796S_SPI_PIN_MODE);
    uapi_pin_set_mode(CONFIG_ST7796S_SPI_CLK_PIN, CONFIG_ST7796S_SPI_PIN_MODE);
    uapi_pin_set_mode(CONFIG_ST7796S_SPI_CS_PIN, CONFIG_ST7796S_SPI_PIN_MODE);

    uapi_pin_set_mode(CONFIG_ST7796S_LCD_DC_PIN, LCD_BUS_GPIO_PIN_MODE);
    uapi_gpio_set_dir(CONFIG_ST7796S_LCD_DC_PIN, GPIO_DIRECTION_OUTPUT);
    uapi_pin_set_mode(CONFIG_ST7796S_LCD_RESET_PIN, LCD_BUS_GPIO_PIN_MODE);
    uapi_gpio_set_dir(CONFIG_ST7796S_LCD_RESET_PIN, GPIO_DIRECTION_OUTPUT);

#if defined(CONFIG_ST7796S_LCD_BL_ENABLE) && (CONFIG_ST7796S_LCD_BL_ENABLE == 1)
    uapi_pin_set_mode(CONFIG_ST7796S_LCD_BL_PIN, LCD_BUS_GPIO_PIN_MODE);
    uapi_gpio_set_dir(CONFIG_ST7796S_LCD_BL_PIN, GPIO_DIRECTION_OUTPUT);
    lcd_bus_backlight_set(0);
#endif

    spi_attr_t config = {0};
    spi_extra_attr_t ext_config = {0};

    config.is_slave = false;
    config.slave_num = LCD_BUS_SPI_SLAVE_NUM;
    config.bus_clk = SPI_CLK_FREQ;
    config.freq_mhz = CONFIG_ST7796S_SPI_FREQUENCY_MHZ;
    config.clk_polarity = LCD_BUS_SPI_CLK_POLARITY;
    config.clk_phase = LCD_BUS_SPI_CLK_PHASE;
    config.frame_format = SPI_CFG_FRAME_FORMAT_MOTOROLA_SPI;
    config.spi_frame_format = HAL_SPI_FRAME_FORMAT_STANDARD;
    config.frame_size = HAL_SPI_FRAME_SIZE_8;
    config.tmod = HAL_SPI_TRANS_MODE_TX;
    config.sste = 0;

    errcode_t ret = uapi_spi_init(CONFIG_ST7796S_SPI_BUS_ID, &config, &ext_config);
    if (ret != ERRCODE_SUCC) {
        osal_printk("st7796s spi init fail 0x%x\r\n", ret);
        return ret;
    }
    osal_printk("st7796s spi init ok\r\n");

#if defined(CONFIG_SPI_SUPPORT_DMA) && (CONFIG_SPI_SUPPORT_DMA == 1)
    uapi_dma_init();
    uapi_dma_open();
#endif

    return ERRCODE_SUCC;
}

void lcd_bus_reset(void)
{
    uapi_gpio_set_val(CONFIG_ST7796S_LCD_RESET_PIN, GPIO_LEVEL_HIGH);
    osal_msleep(1);
    uapi_gpio_set_val(CONFIG_ST7796S_LCD_RESET_PIN, GPIO_LEVEL_LOW);
    osal_msleep(10);
    uapi_gpio_set_val(CONFIG_ST7796S_LCD_RESET_PIN, GPIO_LEVEL_HIGH);
    osal_msleep(120);
}

void lcd_bus_backlight_set(uint8_t enabled)
{
#if defined(CONFIG_ST7796S_LCD_BL_ENABLE) && (CONFIG_ST7796S_LCD_BL_ENABLE == 1)
    uapi_gpio_set_val(CONFIG_ST7796S_LCD_BL_PIN, enabled ? GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW);
#else
    (void)enabled;
#endif
}

errcode_t lcd_bus_send_cmd(uint8_t cmd)
{
    uapi_gpio_set_val(CONFIG_ST7796S_LCD_DC_PIN, GPIO_LEVEL_LOW);
    return lcd_bus_write_byte(cmd);
}

errcode_t lcd_bus_send_data(uint8_t data)
{
    uapi_gpio_set_val(CONFIG_ST7796S_LCD_DC_PIN, GPIO_LEVEL_HIGH);
    return lcd_bus_write_byte(data);
}

errcode_t lcd_bus_send_data_array(const uint8_t *data, uint32_t len)
{
    if ((data == NULL) || (len == 0)) {
        return ERRCODE_SUCC;
    }

    spi_xfer_data_t spi_data = {
        .tx_buff = (uint8_t *)data,
        .tx_bytes = len,
    };

    uapi_gpio_set_val(CONFIG_ST7796S_LCD_DC_PIN, GPIO_LEVEL_HIGH);
    return uapi_spi_master_write(CONFIG_ST7796S_SPI_BUS_ID, &spi_data, LCD_BUS_WRITE_TIMEOUT);
}
