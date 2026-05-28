#include "ft6336u.h"
#include "pinctrl.h"
#include "gpio.h"
#include "i2c.h"
#include "soc_osal.h"
#include "platform_core.h"
#include "errcode.h"

#define FT6336U_REG_TD_STATUS       0x02
#define FT6336U_REG_TOUCH1          0x03
#define FT6336U_REG_CHIP_ID         0xA8

#define FT6336U_I2C_BAUDRATE        400000

#define FT6336U_RST_LOW_DELAY_MS    20
#define FT6336U_RST_HIGH_DELAY_MS   100

#define FT6336U_TOUCH_DATA_LEN      6

#define GPIO_PINMODE                0

static errcode_t ft6336u_write_reg(uint8_t reg, const uint8_t *buf, uint16_t len)
{
    uint8_t send_buf[32];
    send_buf[0] = reg;
    for (uint16_t i = 0; i < len && (i + 1) < sizeof(send_buf); i++) {
        send_buf[i + 1] = buf[i];
    }
    i2c_data_t data = {
        .send_buf = send_buf,
        .send_len = 1 + len,
        .receive_buf = NULL,
        .receive_len = 0,
    };
    return uapi_i2c_master_write(I2C_BUS_1, FT6336U_I2C_ADDR, &data);
}

static errcode_t ft6336u_read_reg(uint8_t reg, uint8_t *buf, uint16_t len)
{
    i2c_data_t data = {
        .send_buf = &reg,
        .send_len = 1,
        .receive_buf = buf,
        .receive_len = len,
    };
    return uapi_i2c_master_writeread(I2C_BUS_1, FT6336U_I2C_ADDR, &data);
}

void ft6336u_init(void)
{
    uapi_pin_set_mode(FT6336U_TOUCH_SDA_PIN, FT6336U_SDA_PIN_MODE);
    uapi_pin_set_mode(FT6336U_TOUCH_SCL_PIN, FT6336U_SCL_PIN_MODE);

    uapi_pin_set_mode(FT6336U_TOUCH_RST_PIN, GPIO_PINMODE);
    uapi_gpio_set_dir(FT6336U_TOUCH_RST_PIN, GPIO_DIRECTION_OUTPUT);

    uapi_pin_set_mode(FT6336U_TOUCH_INT_PIN, GPIO_PINMODE);
    uapi_gpio_set_dir(FT6336U_TOUCH_INT_PIN, GPIO_DIRECTION_INPUT);

    uapi_gpio_set_val(FT6336U_TOUCH_RST_PIN, GPIO_LEVEL_LOW);
    osal_msleep(FT6336U_RST_LOW_DELAY_MS);
    uapi_gpio_set_val(FT6336U_TOUCH_RST_PIN, GPIO_LEVEL_HIGH);
    osal_msleep(FT6336U_RST_HIGH_DELAY_MS);

    errcode_t ret = uapi_i2c_master_init(I2C_BUS_1, FT6336U_I2C_BAUDRATE, 0);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[FT6336U] I2C init fail: 0x%x\r\n", ret);
        return;
    }

    uint8_t chip_id = 0;
    ret = ft6336u_read_reg(FT6336U_REG_CHIP_ID, &chip_id, 1);
    if (ret == ERRCODE_SUCC) {
        osal_printk("[FT6336U] chip id: 0x%02X\r\n", chip_id);
    } else {
        osal_printk("[FT6336U] read chip id fail: 0x%x\r\n", ret);
    }
}

bool ft6336u_read_touch(ft6336u_touch_t *touch)
{
    uint8_t td_status = 0;
    errcode_t ret = ft6336u_read_reg(FT6336U_REG_TD_STATUS, &td_status, 1);
    if (ret != ERRCODE_SUCC) {
        touch->count = 0;
        return false;
    }

    if (td_status > FT6336U_MAX_TOUCHES) {
        td_status = 0;
    }

    touch->count = td_status;

    if (td_status == 0) {
        return false;
    }

    uint8_t buf[FT6336U_TOUCH_DATA_LEN * FT6336U_MAX_TOUCHES];
    ret = ft6336u_read_reg(FT6336U_REG_TOUCH1, buf, td_status * FT6336U_TOUCH_DATA_LEN);
    if (ret != ERRCODE_SUCC) {
        touch->count = 0;
        return false;
    }

    for (uint8_t i = 0; i < td_status; i++) {
        uint8_t offset = i * FT6336U_TOUCH_DATA_LEN;
        touch->event[i] = (buf[offset] >> 6) & 0x03;
        touch->x[i] = ((uint16_t)(buf[offset + 1] & 0x0F) << 8) | buf[offset + 2];
        touch->y[i] = ((uint16_t)(buf[offset + 3] & 0x0F) << 8) | buf[offset + 4];
    }

    return true;
}
