#include "ft6336u.h"

#include "gpio.h"
#include "i2c.h"
#include "pinctrl.h"
#include "soc_osal.h"

#define FT6336U_GPIO_PIN_MODE 0
#define FT6336U_I2C_RESET_LOW_MS 20
#define FT6336U_I2C_BAUDRATE 100000
#define FT6336U_I2C_HSCODE 0
#define FT6336U_I2C_RESET_READY_MS 500
#define FT6336U_I2C_GPIO_SETTLE_MS 2
#define FT6336U_REG_TD_STATUS 0x02
#define FT6336U_REG_TOUCH1_XH 0x03
#define FT6336U_REG_CHIP_ID 0xA3
#define FT6336U_REG_VENDOR_ID 0xA8
#define FT6336U_POINT_BYTES 6
#define FT6336U_POINT_COORD_BYTES 4
#define FT6336U_EVENT_DOWN 0
#define FT6336U_EVENT_CONTACT 2
#define FT6336U_READ_FAIL_LOG_INTERVAL 100
#define FT6336U_SCAN_ADDR_START 0x08
#define FT6336U_SCAN_ADDR_END 0x77

static uint32_t g_ft6336u_read_fail_count = 0;

static void ft6336u_log_i2c_idle_level(void)
{
    uapi_pin_set_mode(CONFIG_FT6336U_I2C_SDA_PIN, FT6336U_GPIO_PIN_MODE);
    uapi_pin_set_mode(CONFIG_FT6336U_I2C_SCL_PIN, FT6336U_GPIO_PIN_MODE);
    uapi_pin_set_pull(CONFIG_FT6336U_I2C_SDA_PIN, PIN_PULL_TYPE_UP);
    uapi_pin_set_pull(CONFIG_FT6336U_I2C_SCL_PIN, PIN_PULL_TYPE_UP);
    uapi_gpio_set_dir(CONFIG_FT6336U_I2C_SDA_PIN, GPIO_DIRECTION_INPUT);
    uapi_gpio_set_dir(CONFIG_FT6336U_I2C_SCL_PIN, GPIO_DIRECTION_INPUT);
    osal_msleep(FT6336U_I2C_GPIO_SETTLE_MS);

    osal_printk("ft6336u i2c idle level before init: sda=%u scl=%u\r\n",
                uapi_gpio_get_val(CONFIG_FT6336U_I2C_SDA_PIN),
                uapi_gpio_get_val(CONFIG_FT6336U_I2C_SCL_PIN));
}

static errcode_t ft6336u_read_reg(uint8_t reg, uint8_t *buf, uint32_t len)
{
    i2c_data_t data = {
        .send_buf = &reg,
        .send_len = 1,
        .receive_buf = buf,
        .receive_len = len,
    };

    return uapi_i2c_master_writeread(CONFIG_FT6336U_I2C_BUS_ID, FT6336U_I2C_ADDR, &data);
}

static void ft6336u_scan_bus(void)
{
    uint8_t reg = 0;
    i2c_data_t data = {
        .send_buf = &reg,
        .send_len = 1,
    };
    uint8_t found = 0;

    osal_printk("ft6336u i2c scan start bus=%d sda=%d scl=%d\r\n",
                CONFIG_FT6336U_I2C_BUS_ID, CONFIG_FT6336U_I2C_SDA_PIN, CONFIG_FT6336U_I2C_SCL_PIN);
    for (uint16_t addr = FT6336U_SCAN_ADDR_START; addr <= FT6336U_SCAN_ADDR_END; addr++) {
        if (uapi_i2c_master_write(CONFIG_FT6336U_I2C_BUS_ID, addr, &data) == ERRCODE_SUCC) {
            osal_printk("ft6336u i2c addr found: 0x%x\r\n", addr);
            found++;
        }
    }
    if (found == 0) {
        osal_printk("ft6336u i2c scan found no devices\r\n");
    }
}

errcode_t ft6336u_init(void)
{
    uint8_t chip_id = 0;
    uint8_t vendor_id = 0;

    ft6336u_log_i2c_idle_level();

    uapi_pin_set_mode(CONFIG_FT6336U_I2C_SDA_PIN, CONFIG_FT6336U_I2C_PIN_MODE);
    uapi_pin_set_mode(CONFIG_FT6336U_I2C_SCL_PIN, CONFIG_FT6336U_I2C_PIN_MODE);
    uapi_pin_set_pull(CONFIG_FT6336U_I2C_SDA_PIN, PIN_PULL_TYPE_UP);
    uapi_pin_set_pull(CONFIG_FT6336U_I2C_SCL_PIN, PIN_PULL_TYPE_UP);

    uapi_pin_set_mode(CONFIG_FT6336U_RESET_PIN, FT6336U_GPIO_PIN_MODE);
    uapi_gpio_set_dir(CONFIG_FT6336U_RESET_PIN, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(CONFIG_FT6336U_RESET_PIN, GPIO_LEVEL_LOW);
    osal_msleep(FT6336U_I2C_RESET_LOW_MS);
    uapi_gpio_set_val(CONFIG_FT6336U_RESET_PIN, GPIO_LEVEL_HIGH);
    osal_msleep(FT6336U_I2C_RESET_READY_MS);

#if defined(CONFIG_FT6336U_USE_INT_PIN) && (CONFIG_FT6336U_USE_INT_PIN == 1)
    uapi_pin_set_mode(CONFIG_FT6336U_INT_PIN, FT6336U_GPIO_PIN_MODE);
    uapi_gpio_set_dir(CONFIG_FT6336U_INT_PIN, GPIO_DIRECTION_INPUT);
    uapi_pin_set_pull(CONFIG_FT6336U_INT_PIN, PIN_PULL_TYPE_UP);
#endif

    errcode_t ret = uapi_i2c_master_init(CONFIG_FT6336U_I2C_BUS_ID, FT6336U_I2C_BAUDRATE, FT6336U_I2C_HSCODE);
    if (ret != ERRCODE_SUCC) {
        osal_printk("ft6336u i2c init fail 0x%x\r\n", ret);
        return ret;
    }

    ret = ft6336u_read_reg(FT6336U_REG_CHIP_ID, &chip_id, 1);
    if (ret == ERRCODE_SUCC) {
        (void)ft6336u_read_reg(FT6336U_REG_VENDOR_ID, &vendor_id, 1);
        osal_printk("ft6336u probe ok addr=0x38 chip=0x%x vendor=0x%x\r\n", chip_id, vendor_id);
        return ERRCODE_SUCC;
    }

    osal_printk("ft6336u probe addr=0x38 fail 0x%x, check SDA/SCL/RST/3V3/GND\r\n", ret);
    ft6336u_scan_bus();
    return ret;
}

uint8_t ft6336u_read_points(ft6336u_point_t *points, uint8_t max_points)
{
    uint8_t status = 0;
    uint8_t raw[FT6336U_MAX_POINTS * FT6336U_POINT_BYTES] = {0};
    uint8_t touch_count;
    uint8_t accepted = 0;

    if ((points == NULL) || (max_points == 0)) {
        return 0;
    }

    errcode_t ret = ft6336u_read_reg(FT6336U_REG_TD_STATUS, &status, 1);
    if (ret != ERRCODE_SUCC) {
        g_ft6336u_read_fail_count++;
        if ((g_ft6336u_read_fail_count % FT6336U_READ_FAIL_LOG_INTERVAL) == 1) {
            osal_printk("ft6336u read status fail 0x%x count=%u\r\n", ret, g_ft6336u_read_fail_count);
        }
        return 0;
    }

    touch_count = status & 0x0F;
    if ((touch_count == 0) || (touch_count > FT6336U_MAX_POINTS)) {
        return 0;
    }

    if (touch_count > max_points) {
        touch_count = max_points;
    }

    if (ft6336u_read_reg(FT6336U_REG_TOUCH1_XH, raw, touch_count * FT6336U_POINT_BYTES) != ERRCODE_SUCC) {
        return 0;
    }

    for (uint8_t i = 0; i < touch_count; i++) {
        const uint8_t *p = &raw[i * FT6336U_POINT_BYTES];
        uint8_t event = p[0] >> 6;
        uint16_t x = ((uint16_t)(p[0] & 0x0F) << 8) | p[1];
        uint16_t y = ((uint16_t)(p[2] & 0x0F) << 8) | p[3];

        if ((event != FT6336U_EVENT_DOWN) && (event != FT6336U_EVENT_CONTACT)) {
            osal_printk("ft6336u ignore point event=%u x=%u y=%u\r\n", event, x, y);
            continue;
        }

        points[accepted].event = event;
        points[accepted].id = p[2] >> 4;
        points[accepted].x = x;
        points[accepted].y = y;
        accepted++;
    }

    return accepted;
}
