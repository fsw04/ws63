/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Licensed under the Apache License, Version 2.0.
 */

#include <stdio.h>
#include <string.h>
#include "app_init.h"
#include "soc_osal.h"
#include "common_def.h"
#include "m1_pn532.h"

#define M1_TASK_PRIO          24
#define M1_TASK_STACK_SIZE    0x1000
#define M1_SECTOR_TRAILER     7
#define M1_MAX_CONSEC_ERRORS  5

static m1_error_t m1_auth_sector_1(const m1_card_t *card)
{
    m1_error_t ret;

    ret = m1_pn532_auth_key_a(card, M1_SECTOR_TRAILER);
    if (ret != M1_OK) {
        printf("[M1] auth key A failed, ret=%d\n", ret);
        return ret;
    }

    return M1_OK;
}


static m1_error_t m1_read_person_info(m1_person_info_t *info)
{
    uint8_t name_blocks[M1_NAME_BLOCK_COUNT][M1_BLOCK_SIZE];
    uint8_t id_block[M1_BLOCK_SIZE];
    m1_error_t ret;

    if (info == NULL) {
        return M1_ERR_PARAM;
    }
    memset(info, 0, sizeof(*info));

    ret = m1_pn532_read_block(M1_NAME_START_BLOCK, name_blocks[0]);
    if (ret != M1_OK) {
        return ret;
    }
    ret = m1_pn532_read_block(M1_ID_BLOCK, id_block);
    if (ret != M1_OK) {
        return ret;
    }

    ret = m1_unpack_name_blocks(name_blocks, info->name_utf8, &info->name_len);
    if (ret != M1_OK) {
        return ret;
    }
    ret = m1_unpack_id_block(id_block, info->id_number);
    if (ret != M1_OK) {
        return ret;
    }

    return M1_OK;
}

static void *m1_task(const char *arg)
{
    m1_error_t ret;
    m1_card_t card;
    m1_person_info_t info;
    uint8_t err_count = 0;

    unused(arg);

    ret = (m1_error_t)m1_pn532_soft_i2c_pin_init();
    if (ret != M1_OK) {
        printf("[M1] soft I2C init failed, ret=%d\n", ret);
        return NULL;
    }
    printf("[M1] soft I2C init OK, scl=%d, sda=%d\n",
           CONFIG_M1_PN532_SOFT_I2C_SCL_PIN,
           CONFIG_M1_PN532_SOFT_I2C_SDA_PIN);

    ret = m1_pn532_i2c_probe();
    if (ret != M1_OK) {
        printf("[M1] PN532 I2C probe failed, ret=%d\n", ret);
        return NULL;
    }
    printf("[M1] PN532 I2C probe OK\n");

    ret = m1_pn532_sam_config();
    if (ret != M1_OK) {
        printf("[M1] SAM config failed, ret=%d\n", ret);
        return NULL;
    }
    printf("[M1] SAM config OK\n");

    while (1) {
        memset(&card, 0, sizeof(card));

        if (err_count >= M1_MAX_CONSEC_ERRORS) {
            (void)m1_pn532_sam_config();
            err_count = 0;
            osal_msleep(CONFIG_M1_PN532_POLL_INTERVAL_MS);
        }

        ret = m1_pn532_poll_card(&card);
        if (ret == M1_ERR_NO_TARGET) {
            continue;
        }
        if (ret != M1_OK) {
            err_count++;
            (void)m1_pn532_release_target(1);
            continue;
        }

        err_count = 0;

        ret = m1_auth_sector_1(&card);
        if (ret != M1_OK) {
            err_count++;
            (void)m1_pn532_release_target(1);
            continue;
        }

        ret = m1_read_person_info(&info);
        if (ret == M1_OK) {
            printf("ID: %s, NAME: %s\n", info.id_number, (const char *)info.name_utf8);
            osal_msleep(1000);
        }

        (void)m1_pn532_release_target(1);
    }
}

static void m1_entry(void)
{
    osal_task *task_handle = NULL;

    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)m1_task, 0,
                                      "M1Task", M1_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, M1_TASK_PRIO);
        printf("[M1] create task ok\n");
    } else {
        printf("[M1] create task failed\n");
    }
    osal_kthread_unlock();
}

app_run(m1_entry);
