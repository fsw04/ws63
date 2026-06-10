/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Licensed under the Apache License, Version 2.0.
 */

#include "m1_pn532.h"

#include <stdio.h>
#include <string.h>
#include "common_def.h"
#include "gpio.h"
#include "pinctrl.h"
#include "soc_osal.h"
#include "tcxo.h"

#define PN532_FRAME_MAX_LEN           512
#define PN532_PREAMBLE                0x00
#define PN532_START_CODE              0xFF
#define PN532_POSTAMBLE               0x00
#define PN532_HOST_TO_PN532           0xD4
#define PN532_CMD_SAM_CONFIGURATION   0x14
#define PN532_CMD_IN_AUTO_POLL        0x60
#define PN532_CMD_IN_DATA_EXCHANGE    0x40
#define PN532_CMD_IN_RELEASE          0x52
#define PN532_RSP_SAM_CONFIGURATION   0x15
#define PN532_RSP_IN_AUTO_POLL        0x61
#define PN532_RSP_IN_DATA_EXCHANGE    0x41
#define PN532_STATUS_OK               0x00
#define PN532_ACK_LEN                 6
#define PN532_I2C_ADDRESS             0x24
#define PN532_I2C_WRITE_ADDRESS       (PN532_I2C_ADDRESS << 1)
#define PN532_I2C_READ_ADDRESS        ((PN532_I2C_ADDRESS << 1) | 0x01)
#define PN532_I2C_READY               0x01
#define PN532_I2C_CLOCK_STRETCH_US    5000
#define PN532_I2C_WAKEUP_DELAY_MS     500
#define PN532_ACK_WAIT_TIME_MS        10
#define PN532_I2C_ADDR_RETRY_MS       20
#define PN532_RESPONSE_TIMEOUT_MS     2000
#define PN532_AUTO_POLL_TIMEOUT_MS    8000
#define PN532_AUTO_POLL_NR            5
#define PN532_AUTO_POLL_PERIOD        0x0A
#define MIFARE_AUTH_KEY_A             0x60
#define MIFARE_AUTH_KEY_B             0x61
#define MIFARE_READ                   0x30
#define MIFARE_WRITE                  0xA0
#define MIFARE_DEFAULT_KEY_LEN        6

static const uint8_t g_mifare_default_key[MIFARE_DEFAULT_KEY_LEN] = {
    0x88, 0x88, 0x88, 0x88, 0x88, 0x88
};

typedef struct {
    uint8_t data[256];
    uint8_t len;
} pn532_resp_t;

static uint8_t m1_calc_dcs(const uint8_t *data, uint8_t len)
{
    uint16_t sum = 0;
    uint8_t i;

    for (i = 0; i < len; i++) {
        sum = (uint16_t)(sum + data[i]);
    }
    return (uint8_t)((0x100U - (sum & 0xFFU)) & 0xFFU);
}

static void soft_i2c_delay(void)
{
    uapi_tcxo_delay_us(CONFIG_M1_PN532_SOFT_I2C_DELAY_US);
}

static void soft_i2c_scl_release(void)
{
    uint64_t start;

    (void)uapi_gpio_set_dir(CONFIG_M1_PN532_SOFT_I2C_SCL_PIN, GPIO_DIRECTION_INPUT);
    start = uapi_tcxo_get_us();
    while (uapi_gpio_get_val(CONFIG_M1_PN532_SOFT_I2C_SCL_PIN) == GPIO_LEVEL_LOW) {
        if ((uint32_t)(uapi_tcxo_get_us() - start) >= PN532_I2C_CLOCK_STRETCH_US) {
            break;
        }
    }
}

static void soft_i2c_scl_low(void)
{
    (void)uapi_gpio_set_val(CONFIG_M1_PN532_SOFT_I2C_SCL_PIN, GPIO_LEVEL_LOW);
    (void)uapi_gpio_set_dir(CONFIG_M1_PN532_SOFT_I2C_SCL_PIN, GPIO_DIRECTION_OUTPUT);
}

static void soft_i2c_sda_release(void)
{
    (void)uapi_gpio_set_dir(CONFIG_M1_PN532_SOFT_I2C_SDA_PIN, GPIO_DIRECTION_INPUT);
}

static void soft_i2c_sda_low(void)
{
    (void)uapi_gpio_set_val(CONFIG_M1_PN532_SOFT_I2C_SDA_PIN, GPIO_LEVEL_LOW);
    (void)uapi_gpio_set_dir(CONFIG_M1_PN532_SOFT_I2C_SDA_PIN, GPIO_DIRECTION_OUTPUT);
}

static uint8_t soft_i2c_sda_read(void)
{
    return (uint8_t)uapi_gpio_get_val(CONFIG_M1_PN532_SOFT_I2C_SDA_PIN);
}

static void soft_i2c_start(void)
{
    soft_i2c_sda_release();
    soft_i2c_scl_release();
    soft_i2c_delay();
    soft_i2c_sda_low();
    soft_i2c_delay();
    soft_i2c_scl_low();
    soft_i2c_delay();
}

static void soft_i2c_stop(void)
{
    soft_i2c_sda_low();
    soft_i2c_delay();
    soft_i2c_scl_release();
    soft_i2c_delay();
    soft_i2c_sda_release();
    soft_i2c_delay();
}

static void soft_i2c_bus_recover(void)
{
    uint8_t i;

    soft_i2c_sda_release();
    for (i = 0; i < 9; i++) {
        soft_i2c_scl_release();
        soft_i2c_delay();
        soft_i2c_scl_low();
        soft_i2c_delay();
    }
    soft_i2c_stop();
}

static bool soft_i2c_write_byte(uint8_t data)
{
    uint8_t i;
    bool ack;

    for (i = 0; i < 8; i++) {
        if ((data & 0x80) != 0) {
            soft_i2c_sda_release();
        } else {
            soft_i2c_sda_low();
        }
        soft_i2c_delay();
        soft_i2c_scl_release();
        soft_i2c_delay();
        soft_i2c_scl_low();
        data <<= 1;
    }

    soft_i2c_sda_release();
    soft_i2c_delay();
    soft_i2c_scl_release();
    soft_i2c_delay();
    ack = (soft_i2c_sda_read() == GPIO_LEVEL_LOW);
    soft_i2c_scl_low();
    soft_i2c_delay();
    return ack;
}

static uint8_t soft_i2c_read_byte(bool ack)
{
    uint8_t i;
    uint8_t data = 0;

    soft_i2c_sda_release();
    for (i = 0; i < 8; i++) {
        data <<= 1;
        soft_i2c_delay();
        soft_i2c_scl_release();
        soft_i2c_delay();
        if (soft_i2c_sda_read() == GPIO_LEVEL_HIGH) {
            data |= 0x01;
        }
        soft_i2c_scl_low();
    }

    if (ack) {
        soft_i2c_sda_low();
    } else {
        soft_i2c_sda_release();
    }
    soft_i2c_delay();
    soft_i2c_scl_release();
    soft_i2c_delay();
    soft_i2c_scl_low();
    soft_i2c_sda_release();
    soft_i2c_delay();
    return data;
}

static m1_error_t soft_i2c_write(const uint8_t *data, uint16_t len)
{
    uint16_t i;
    uint32_t elapsed = 0;

    if (data == NULL || len == 0) {
        return M1_ERR_PARAM;
    }

    do {
        soft_i2c_start();
        if (soft_i2c_write_byte(PN532_I2C_WRITE_ADDRESS)) {
            break;
        }
        soft_i2c_stop();
        soft_i2c_bus_recover();
        osal_msleep(1);
        elapsed++;
    } while (elapsed < PN532_I2C_ADDR_RETRY_MS);

    if (elapsed >= PN532_I2C_ADDR_RETRY_MS) {
        printf("[M1] PN532 I2C write addr NACK\n");
        return M1_ERR_TIMEOUT;
    }

    for (i = 0; i < len; i++) {
        if (!soft_i2c_write_byte(data[i])) {
            soft_i2c_stop();
            printf("[M1] PN532 I2C write byte NACK, index=%u, data=0x%02X\n", i, data[i]);
            return M1_ERR_TIMEOUT;
        }
    }
    soft_i2c_stop();
    return M1_OK;
}

static m1_error_t soft_i2c_read(uint8_t *data, uint16_t len)
{
    uint16_t i;

    if (data == NULL || len == 0) {
        return M1_ERR_PARAM;
    }

    soft_i2c_start();
    if (!soft_i2c_write_byte(PN532_I2C_READ_ADDRESS)) {
        soft_i2c_stop();
        return M1_ERR_TIMEOUT;
    }
    for (i = 0; i < len; i++) {
        data[i] = soft_i2c_read_byte(i + 1U < len);
    }
    soft_i2c_stop();
    return M1_OK;
}

int32_t m1_pn532_soft_i2c_pin_init(void)
{
    uapi_gpio_init();
    uapi_tcxo_init();

    uapi_pin_set_mode(CONFIG_M1_PN532_SOFT_I2C_SCL_PIN, PIN_MODE_0);
    uapi_pin_set_mode(CONFIG_M1_PN532_SOFT_I2C_SDA_PIN, PIN_MODE_0);
    uapi_pin_set_pull(CONFIG_M1_PN532_SOFT_I2C_SCL_PIN, PIN_PULL_TYPE_UP);
    uapi_pin_set_pull(CONFIG_M1_PN532_SOFT_I2C_SDA_PIN, PIN_PULL_TYPE_UP);
#if defined(CONFIG_PINCTRL_SUPPORT_IE)
    uapi_pin_set_ie(CONFIG_M1_PN532_SOFT_I2C_SCL_PIN, PIN_IE_1);
    uapi_pin_set_ie(CONFIG_M1_PN532_SOFT_I2C_SDA_PIN, PIN_IE_1);
#endif

    if (uapi_gpio_set_dir(CONFIG_M1_PN532_SOFT_I2C_SCL_PIN, GPIO_DIRECTION_INPUT) != ERRCODE_SUCC) {
        return M1_ERR_TIMEOUT;
    }
    if (uapi_gpio_set_dir(CONFIG_M1_PN532_SOFT_I2C_SDA_PIN, GPIO_DIRECTION_INPUT) != ERRCODE_SUCC) {
        return M1_ERR_TIMEOUT;
    }
    return M1_OK;
}

m1_error_t m1_pn532_i2c_probe(void)
{
    bool write_ack;
    bool read_ack;

    soft_i2c_bus_recover();

    soft_i2c_start();
    write_ack = soft_i2c_write_byte(PN532_I2C_WRITE_ADDRESS);
    soft_i2c_stop();

    soft_i2c_start();
    read_ack = soft_i2c_write_byte(PN532_I2C_READ_ADDRESS);
    soft_i2c_stop();

    printf("[M1] PN532 I2C probe write=%d, read=%d\n", write_ack ? 1 : 0, read_ack ? 1 : 0);

    return write_ack ? M1_OK : M1_ERR_TIMEOUT;
}

static m1_error_t pn532_read_ack_frame(void)
{
    static const uint8_t ack_frame[PN532_ACK_LEN] = {0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00};
    uint8_t raw[PN532_ACK_LEN + 1];
    uint32_t elapsed = 0;
    m1_error_t ret;

    while (elapsed < PN532_ACK_WAIT_TIME_MS) {
        ret = soft_i2c_read(raw, sizeof(raw));
        if (ret == M1_OK && raw[0] == PN532_I2C_READY) {
            if (memcmp(&raw[1], ack_frame, PN532_ACK_LEN) == 0) {
                return M1_OK;
            }
            printf("[M1] PN532 I2C bad ACK: %02X %02X %02X %02X %02X %02X %02X\n",
                   raw[0], raw[1], raw[2], raw[3], raw[4], raw[5], raw[6]);
            return M1_ERR_FRAME;
        }
        osal_msleep(1);
        elapsed++;
    }

    printf("[M1] PN532 I2C ACK timeout\n");
    return M1_ERR_TIMEOUT;
}

static m1_error_t pn532_send_nack_frame(void)
{
    static const uint8_t nack_frame[PN532_ACK_LEN] = {0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00};

    return soft_i2c_write(nack_frame, sizeof(nack_frame));
}

static m1_error_t pn532_pack_frame(uint8_t cmd, const uint8_t *payload, uint8_t payload_len,
                                   bool with_wakeup, uint8_t *frame, uint16_t *frame_len)
{
    uint16_t offset = 0;
    uint8_t len;
    uint8_t sum_data[256];
    uint8_t i;

    if (frame == NULL || frame_len == NULL) {
        return M1_ERR_PARAM;
    }
    if (payload_len > 250) {
        return M1_ERR_PARAM;
    }

    unused(with_wakeup);

    len = (uint8_t)(payload_len + 2);
    frame[offset++] = PN532_PREAMBLE;
    frame[offset++] = PN532_PREAMBLE;
    frame[offset++] = PN532_START_CODE;
    frame[offset++] = len;
    frame[offset++] = (uint8_t)((0x100U - len) & 0xFFU);
    frame[offset++] = PN532_HOST_TO_PN532;
    frame[offset++] = cmd;

    sum_data[0] = PN532_HOST_TO_PN532;
    sum_data[1] = cmd;
    for (i = 0; i < payload_len; i++) {
        frame[offset++] = payload[i];
        sum_data[i + 2] = payload[i];
    }
    frame[offset++] = m1_calc_dcs(sum_data, len);
    frame[offset++] = PN532_POSTAMBLE;
    *frame_len = offset;

    return M1_OK;
}

static m1_error_t pn532_send_command(uint8_t cmd, const uint8_t *payload, uint8_t payload_len,
                                     bool with_wakeup)
{
    uint8_t frame[PN532_FRAME_MAX_LEN];
    uint16_t frame_len = 0;
    m1_error_t ret;

    if (pn532_pack_frame(cmd, payload, payload_len, with_wakeup, frame, &frame_len) != M1_OK) {
        return M1_ERR_FRAME;
    }

    if (with_wakeup) {
        osal_msleep(PN532_I2C_WAKEUP_DELAY_MS);
    }

    soft_i2c_bus_recover();
    ret = soft_i2c_write(frame, frame_len);
    if (ret != M1_OK) {
        printf("[M1] PN532 I2C send cmd 0x%02X failed, ret=%d\n", cmd, ret);
        return ret;
    }

    ret = pn532_read_ack_frame();
    if (ret != M1_OK) {
        printf("[M1] PN532 I2C cmd 0x%02X no ACK, ret=%d\n", cmd, ret);
    }
    return ret;
}

static m1_error_t pn532_unpack_frame(const uint8_t *raw, uint16_t raw_len, pn532_resp_t *resp)
{
    uint16_t pos;
    uint16_t data_start;
    uint16_t post_pos;
    uint8_t len;
    uint8_t i;
    uint16_t sum;

    if (raw == NULL || resp == NULL || raw_len < 8) {
        return M1_ERR_PARAM;
    }

    for (pos = 0; pos + 5 < raw_len; pos++) {
        if (raw[pos] == PN532_PREAMBLE && raw[pos + 1] == PN532_PREAMBLE &&
            raw[pos + 2] == PN532_START_CODE) {
            break;
        }
    }
    if (pos + 5 >= raw_len) {
        return M1_ERR_FRAME;
    }

    len = raw[pos + 3];
    if (((uint8_t)(len + raw[pos + 4])) != 0x00) {
        return M1_ERR_FRAME;
    }
    data_start = (uint16_t)(pos + 5);
    post_pos = (uint16_t)(data_start + len + 1);
    if (post_pos >= raw_len) {
        return M1_ERR_FRAME;
    }

    sum = 0;
    for (i = 0; i < len; i++) {
        sum = (uint16_t)(sum + raw[data_start + i]);
    }
    sum = (uint16_t)(sum + raw[data_start + len]);
    if ((sum & 0xFFU) != 0) {
        return M1_ERR_FRAME;
    }
    if (raw[post_pos] != PN532_POSTAMBLE || len < 2 || raw[data_start] != 0xD5) {
        return M1_ERR_FRAME;
    }

    resp->len = (uint8_t)(len - 1);
    memcpy(resp->data, &raw[data_start + 1], resp->len);
    return M1_OK;
}

static m1_error_t pn532_recv_response(pn532_resp_t *resp, uint32_t timeout_ms)
{
    uint8_t raw[PN532_FRAME_MAX_LEN];
    uint32_t elapsed = 0;
    uint8_t status;
    uint8_t len;
    uint16_t total_len;
    m1_error_t ret;

    if (resp == NULL) {
        return M1_ERR_PARAM;
    }

    while (elapsed < timeout_ms) {
        ret = soft_i2c_read(&status, 1);
        if (ret == M1_OK && status == PN532_I2C_READY) {
            break;
        }
        osal_msleep(5);
        elapsed += 5;
    }
    if (elapsed >= timeout_ms) {
        printf("[M1] PN532 I2C response ready timeout\n");
        return M1_ERR_TIMEOUT;
    }

    ret = soft_i2c_read(raw, 6);
    if (ret != M1_OK) {
        printf("[M1] PN532 I2C response header read failed, ret=%d\n", ret);
        return ret;
    }
    if (raw[0] != PN532_I2C_READY) {
        return M1_ERR_FRAME;
    }
    if (raw[1] != 0x00 || raw[2] != 0x00 || raw[3] != 0xFF) {
        printf("[M1] PN532 I2C bad response header: %02X %02X %02X %02X %02X %02X\n",
               raw[0], raw[1], raw[2], raw[3], raw[4], raw[5]);
        return M1_ERR_FRAME;
    }
    len = raw[4];
    if (((uint8_t)(len + raw[5])) != 0x00) {
        return M1_ERR_FRAME;
    }
    ret = pn532_send_nack_frame();
    if (ret != M1_OK) {
        printf("[M1] PN532 I2C send NACK failed, ret=%d\n", ret);
        return ret;
    }
    total_len = (uint16_t)(6U + len + 2U);
    if (total_len + 1U > sizeof(raw)) {
        return M1_ERR_FRAME;
    }
    ret = soft_i2c_read(raw, (uint16_t)(total_len + 1U));
    if (ret != M1_OK) {
        printf("[M1] PN532 I2C response read failed, ret=%d\n", ret);
        return ret;
    }
    if (raw[0] != PN532_I2C_READY) {
        return M1_ERR_FRAME;
    }

    ret = pn532_unpack_frame(&raw[1], total_len, resp);
    if (ret != M1_OK) {
        printf("[M1] PN532 I2C unpack failed, ret=%d, raw=%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
               ret, raw[0], raw[1], raw[2], raw[3], raw[4], raw[5], raw[6], raw[7], raw[8], raw[9]);
    }
    return ret;
}

m1_error_t m1_pn532_sam_config(void)
{
    m1_error_t ret;
    pn532_resp_t resp;
    uint8_t normal_mode = 0x01;

    ret = pn532_send_command(PN532_CMD_SAM_CONFIGURATION, &normal_mode, sizeof(normal_mode), true);
    if (ret != M1_OK) {
        return ret;
    }
    ret = pn532_recv_response(&resp, PN532_RESPONSE_TIMEOUT_MS);
    if (ret != M1_OK) {
        return ret;
    }
    if (resp.len < 1 || resp.data[0] != PN532_RSP_SAM_CONFIGURATION) {
        return M1_ERR_PN532_STATUS;
    }

    return M1_OK;
}

m1_error_t m1_pn532_release_target(uint8_t target_number)
{
    m1_error_t ret;
    pn532_resp_t resp;
    uint8_t payload = target_number;

    ret = pn532_send_command(PN532_CMD_IN_RELEASE, &payload, sizeof(payload), false);
    if (ret != M1_OK) {
        return ret;
    }
    ret = pn532_recv_response(&resp, PN532_RESPONSE_TIMEOUT_MS);
    if (ret != M1_OK) {
        return ret;
    }
    if (resp.len < 1 || resp.data[0] != (PN532_CMD_IN_RELEASE + 1)) {
        return M1_ERR_PN532_STATUS;
    }

    return M1_OK;
}

m1_error_t m1_pn532_poll_card(m1_card_t *card)
{
    m1_error_t ret;
    pn532_resp_t resp;
    uint8_t payload[] = {PN532_AUTO_POLL_NR, PN532_AUTO_POLL_PERIOD, 0x00};

    if (card == NULL) {
        return M1_ERR_PARAM;
    }

    ret = pn532_send_command(PN532_CMD_IN_AUTO_POLL, payload, sizeof(payload), false);
    if (ret != M1_OK) {
        return ret;
    }
    ret = pn532_recv_response(&resp, PN532_AUTO_POLL_TIMEOUT_MS);
    if (ret != M1_OK) {
        return ret;
    }
    if (resp.len < 2 || resp.data[0] != PN532_RSP_IN_AUTO_POLL || resp.data[1] == 0) {
        return M1_ERR_NO_TARGET;
    }

    card->target_number = resp.data[4];
    card->uid_len = resp.data[8];
    if (card->uid_len > M1_UID_MAX_LEN || resp.len < 9 + card->uid_len) {
        return M1_ERR_FRAME;
    }
    memcpy(card->uid, &resp.data[9], card->uid_len);
    return M1_OK;
}

static m1_error_t m1_pn532_auth(const m1_card_t *card, uint8_t block, uint8_t key_type)
{
    uint8_t payload[1 + 1 + 1 + MIFARE_DEFAULT_KEY_LEN + M1_UID_MAX_LEN];
    uint8_t len = 0;
    pn532_resp_t resp;
    m1_error_t ret;

    if (card == NULL || card->uid_len == 0 || card->uid_len > M1_UID_MAX_LEN) {
        return M1_ERR_PARAM;
    }

    payload[len++] = card->target_number;
    payload[len++] = key_type;
    payload[len++] = block;
    memcpy(&payload[len], g_mifare_default_key, MIFARE_DEFAULT_KEY_LEN);
    len = (uint8_t)(len + MIFARE_DEFAULT_KEY_LEN);
    memcpy(&payload[len], card->uid, card->uid_len);
    len = (uint8_t)(len + card->uid_len);

    ret = pn532_send_command(PN532_CMD_IN_DATA_EXCHANGE, payload, len, false);
    if (ret != M1_OK) {
        return ret;
    }
    ret = pn532_recv_response(&resp, PN532_RESPONSE_TIMEOUT_MS);
    if (ret != M1_OK) {
        return ret;
    }
    if (resp.len < 2 || resp.data[0] != PN532_RSP_IN_DATA_EXCHANGE ||
        resp.data[1] != PN532_STATUS_OK) {
        return M1_ERR_VERIFY;
    }

    return M1_OK;
}

m1_error_t m1_pn532_auth_key_a(const m1_card_t *card, uint8_t block)
{
    return m1_pn532_auth(card, block, MIFARE_AUTH_KEY_A);
}

m1_error_t m1_pn532_auth_key_b(const m1_card_t *card, uint8_t block)
{
    return m1_pn532_auth(card, block, MIFARE_AUTH_KEY_B);
}

m1_error_t m1_pn532_read_block(uint8_t block, uint8_t data[M1_BLOCK_SIZE])
{
    uint8_t payload[] = {0x01, MIFARE_READ, block};
    pn532_resp_t resp;
    m1_error_t ret;

    if (data == NULL) {
        return M1_ERR_PARAM;
    }

    ret = pn532_send_command(PN532_CMD_IN_DATA_EXCHANGE, payload, sizeof(payload), false);
    if (ret != M1_OK) {
        return ret;
    }
    ret = pn532_recv_response(&resp, PN532_RESPONSE_TIMEOUT_MS);
    if (ret != M1_OK) {
        return ret;
    }
    if (resp.len < 2 + M1_BLOCK_SIZE || resp.data[0] != PN532_RSP_IN_DATA_EXCHANGE ||
        resp.data[1] != PN532_STATUS_OK) {
        return M1_ERR_PN532_STATUS;
    }

    memcpy(data, &resp.data[2], M1_BLOCK_SIZE);
    return M1_OK;
}

m1_error_t m1_pn532_write_block(uint8_t block, const uint8_t data[M1_BLOCK_SIZE])
{
    uint8_t payload[1 + 1 + 1 + M1_BLOCK_SIZE];
    pn532_resp_t resp;
    m1_error_t ret;

    if (data == NULL) {
        return M1_ERR_PARAM;
    }

    payload[0] = 0x01;
    payload[1] = MIFARE_WRITE;
    payload[2] = block;
    memcpy(&payload[3], data, M1_BLOCK_SIZE);

    ret = pn532_send_command(PN532_CMD_IN_DATA_EXCHANGE, payload, sizeof(payload), false);
    if (ret != M1_OK) {
        return ret;
    }
    ret = pn532_recv_response(&resp, PN532_RESPONSE_TIMEOUT_MS);
    if (ret != M1_OK) {
        return ret;
    }
    if (resp.len < 2 || resp.data[0] != PN532_RSP_IN_DATA_EXCHANGE ||
        resp.data[1] != PN532_STATUS_OK) {
        return M1_ERR_PN532_STATUS;
    }

    return M1_OK;
}

m1_error_t m1_pack_name_blocks(const uint8_t *name_utf8, uint8_t name_len,
                               uint8_t blocks[M1_NAME_BLOCK_COUNT][M1_BLOCK_SIZE])
{
    if (name_utf8 == NULL || blocks == NULL || name_len > M1_NAME_MAX_UTF8_BYTES) {
        return M1_ERR_PARAM;
    }

    memset(blocks, 0xFF, M1_NAME_BLOCK_COUNT * M1_BLOCK_SIZE);
    memcpy(&blocks[0][0], name_utf8, name_len);
    return M1_OK;
}

m1_error_t m1_unpack_name_blocks(const uint8_t blocks[M1_NAME_BLOCK_COUNT][M1_BLOCK_SIZE],
                                 uint8_t *name_utf8, uint8_t *name_len)
{
    uint8_t i;
    uint8_t len = 0;
    uint8_t total = M1_NAME_BLOCK_COUNT * M1_BLOCK_SIZE;
    const uint8_t *raw = (const uint8_t *)blocks;

    if (blocks == NULL || name_utf8 == NULL || name_len == NULL) {
        return M1_ERR_PARAM;
    }

    for (i = 0; i < total; i++) {
        if (raw[i] == 0xFF) {
            break;
        }
        if (len < M1_NAME_MAX_UTF8_BYTES) {
            name_utf8[len++] = raw[i];
        }
    }
    name_utf8[len] = '\0';
    *name_len = len;

    return M1_OK;
}

m1_error_t m1_pack_id_block(const char *id_number, uint8_t block[M1_BLOCK_SIZE])
{
    uint8_t i;

    if (id_number == NULL || block == NULL) {
        return M1_ERR_PARAM;
    }

    memset(block, 0xFF, M1_BLOCK_SIZE);
    for (i = 0; i < M1_ID_DIGIT_COUNT; i += 2) {
        char high = id_number[i];
        char low = id_number[i + 1];
        if (high < '0' || high > '9' || low < '0' || low > '9') {
            return M1_ERR_PARAM;
        }
        block[i / 2] = (uint8_t)(((uint8_t)(high - '0') << 4) | (uint8_t)(low - '0'));
    }

    return M1_OK;
}

m1_error_t m1_unpack_id_block(const uint8_t block[M1_BLOCK_SIZE], char *id_number)
{
    uint8_t i;

    if (block == NULL || id_number == NULL) {
        return M1_ERR_PARAM;
    }

    for (i = 0; i < M1_ID_BCD_BYTES; i++) {
        uint8_t high = (uint8_t)((block[i] >> 4) & 0x0F);
        uint8_t low = (uint8_t)(block[i] & 0x0F);
        if (high > 9 || low > 9) {
            return M1_ERR_PARAM;
        }
        id_number[i * 2] = (char)('0' + high);
        id_number[i * 2 + 1] = (char)('0' + low);
    }
    id_number[M1_ID_DIGIT_COUNT] = '\0';

    return M1_OK;
}

bool m1_block_is_blank(const uint8_t block[M1_BLOCK_SIZE])
{
    uint8_t i;

    if (block == NULL) {
        return true;
    }
    for (i = 0; i < M1_BLOCK_SIZE; i++) {
        if (block[i] != 0x00 && block[i] != 0xFF) {
            return false;
        }
    }
    return true;
}
