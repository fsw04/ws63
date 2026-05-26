#include "ft6336.h"
#include "lcd.h"
#include "pinctrl.h"
#include "gpio.h"
#include "i2c.h"
#include "soc_osal.h"

#define FT6336_I2C_BUS          I2C_BUS_0
#define FT6336_I2C_BAUDRATE     400000
#define FT6336_READ_LEN         16

static bool g_ft6336_initialized = false;

static errcode_t ft6336_i2c_write(uint8_t reg, uint8_t value)
{
    uint8_t send_buf[2] = {reg, value};
    i2c_data_t data = {
        .send_buf = send_buf,
        .send_len = 2,
        .receive_buf = NULL,
        .receive_len = 0,
    };
    return uapi_i2c_master_write(FT6336_I2C_BUS, FT6336_I2C_ADDR, &data);
}

static errcode_t ft6336_i2c_read(uint8_t reg, uint8_t *buf, uint16_t len)
{
    uint8_t reg_buf = reg;
    i2c_data_t data = {
        .send_buf = &reg_buf,
        .send_len = 1,
        .receive_buf = buf,
        .receive_len = len,
    };
    return uapi_i2c_master_writeread(FT6336_I2C_BUS, FT6336_I2C_ADDR, &data);
}

static void ft6336_gpio_init(void)
{
    uapi_pin_set_mode(CONFIG_TOUCH_SDA_PIN, PIN_MODE_2);
    uapi_pin_set_mode(CONFIG_TOUCH_SCL_PIN, PIN_MODE_2);

    uapi_pin_set_mode(CONFIG_TOUCH_RST_PIN, CONFIG_GPIO_PIN_MODE);
    uapi_gpio_set_dir(CONFIG_TOUCH_RST_PIN, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(CONFIG_TOUCH_RST_PIN, GPIO_LEVEL_HIGH);

    uapi_pin_set_mode(CONFIG_TOUCH_INT_PIN, CONFIG_GPIO_PIN_MODE);
    uapi_gpio_set_dir(CONFIG_TOUCH_INT_PIN, GPIO_DIRECTION_INPUT);
    uapi_pin_set_pull(CONFIG_TOUCH_INT_PIN, PIN_PULL_TYPE_UP);
}

static void ft6336_reset(void)
{
    uapi_gpio_set_val(CONFIG_TOUCH_RST_PIN, GPIO_LEVEL_LOW);
    osal_mdelay(10);
    uapi_gpio_set_val(CONFIG_TOUCH_RST_PIN, GPIO_LEVEL_HIGH);
    osal_mdelay(50);
}

void ft6336_init(void)
{
    if (g_ft6336_initialized) {
        return;
    }

    ft6336_gpio_init();

    errcode_t ret = uapi_i2c_master_init(FT6336_I2C_BUS, FT6336_I2C_BAUDRATE, 0);
    if (ret != ERRCODE_SUCC) {
        osal_printk("FT6336 I2C init failed: 0x%x\r\n", ret);
        return;
    }

    ft6336_reset();

    osal_mdelay(100);

    uint16_t chip_id = ft6336_get_chip_id();
    osal_printk("FT6336 chip id: 0x%04x\r\n", chip_id);

    uint8_t fw_ver = ft6336_get_firmware_version();
    osal_printk("FT6336 firmware version: 0x%02x\r\n", fw_ver);

    ft6336_i2c_write(FT6336_REG_MODE, 0x00);
    ft6336_i2c_write(FT6336_REG_TH_GROUP, 0x16);
    ft6336_i2c_write(FT6336_REG_PERIOD_ACTIVE, 0x0E);
    ft6336_i2c_write(FT6336_REG_PERIOD_MONITOR, 0x0A);

    g_ft6336_initialized = true;
    osal_printk("FT6336 init success\r\n");
}

bool ft6336_read_touch(ft6336_touch_data_t *touch_data)
{
    if (touch_data == NULL || !g_ft6336_initialized) {
        return false;
    }

    uint8_t buf[FT6336_READ_LEN] = {0};
    errcode_t ret = ft6336_i2c_read(0x00, buf, FT6336_READ_LEN);
    if (ret != ERRCODE_SUCC) {
        return false;
    }

    touch_data->gesture = buf[1];
    touch_data->touch_count = buf[2] & 0x0F;

    if (touch_data->touch_count == 0 || touch_data->touch_count > FT6336_MAX_TOUCH_POINTS) {
        touch_data->touch_count = 0;
        return false;
    }

    for (uint8_t i = 0; i < touch_data->touch_count; i++) {
        uint8_t base = 3 + i * 6;
        touch_data->points[i].x = (uint16_t)((buf[base] & 0x0F) << 8) | buf[base + 1];
        touch_data->points[i].y = (uint16_t)((buf[base + 2] & 0x0F) << 8) | buf[base + 3];
        touch_data->points[i].event = (buf[base + 2] & 0xC0) >> 6;
        touch_data->points[i].weight = buf[base + 4];
        touch_data->points[i].area = (buf[base + 5] >> 4) & 0x0F;
    }

    return true;
}

uint16_t ft6336_get_chip_id(void)
{
    uint8_t buf[2] = {0};
    ft6336_i2c_read(FT6336_REG_CHIP_ID_H, buf, 2);
    return (uint16_t)(buf[0] << 8) | buf[1];
}

uint8_t ft6336_get_firmware_version(void)
{
    uint8_t ver = 0;
    ft6336_i2c_read(FT6336_REG_FIRMWARE_VERS, &ver, 1);
    return ver;
}
