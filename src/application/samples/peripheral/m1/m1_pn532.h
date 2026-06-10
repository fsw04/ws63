/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Licensed under the Apache License, Version 2.0.
 */

#ifndef __M1_PN532_H__
#define __M1_PN532_H__

#include <stdint.h>
#include <stdbool.h>

#define M1_BLOCK_SIZE                 16
#define M1_UID_MAX_LEN                10
#define M1_NAME_MAX_UTF8_BYTES        16
#define M1_ID_DIGIT_COUNT             18
#define M1_ID_BCD_BYTES               9

#define M1_NAME_START_BLOCK           4
#define M1_NAME_BLOCK_COUNT           1
#define M1_ID_BLOCK                   5

typedef enum {
    M1_OK = 0,
    M1_ERR_PARAM = -1,
    M1_ERR_TIMEOUT = -2,
    M1_ERR_FRAME = -3,
    M1_ERR_PN532_STATUS = -4,
    M1_ERR_NO_TARGET = -5,
    M1_ERR_VERIFY = -6
} m1_error_t;

typedef struct {
    uint8_t uid[M1_UID_MAX_LEN];
    uint8_t uid_len;
    uint8_t target_number;
} m1_card_t;

typedef struct {
    uint8_t name_utf8[M1_NAME_MAX_UTF8_BYTES + 1];
    uint8_t name_len;
    char id_number[M1_ID_DIGIT_COUNT + 1];
} m1_person_info_t;

int32_t m1_pn532_soft_i2c_pin_init(void);
m1_error_t m1_pn532_i2c_probe(void);
m1_error_t m1_pn532_sam_config(void);
m1_error_t m1_pn532_release_target(uint8_t target_number);
m1_error_t m1_pn532_poll_card(m1_card_t *card);
m1_error_t m1_pn532_auth_key_a(const m1_card_t *card, uint8_t block);
m1_error_t m1_pn532_auth_key_b(const m1_card_t *card, uint8_t block);
m1_error_t m1_pn532_read_block(uint8_t block, uint8_t data[M1_BLOCK_SIZE]);
m1_error_t m1_pn532_write_block(uint8_t block, const uint8_t data[M1_BLOCK_SIZE]);

m1_error_t m1_pack_name_blocks(const uint8_t *name_utf8, uint8_t name_len,
                               uint8_t blocks[M1_NAME_BLOCK_COUNT][M1_BLOCK_SIZE]);
m1_error_t m1_unpack_name_blocks(const uint8_t blocks[M1_NAME_BLOCK_COUNT][M1_BLOCK_SIZE],
                                 uint8_t *name_utf8, uint8_t *name_len);
m1_error_t m1_pack_id_block(const char *id_number, uint8_t block[M1_BLOCK_SIZE]);
m1_error_t m1_unpack_id_block(const uint8_t block[M1_BLOCK_SIZE], char *id_number);
bool m1_block_is_blank(const uint8_t block[M1_BLOCK_SIZE]);

#endif /* __M1_PN532_H__ */
