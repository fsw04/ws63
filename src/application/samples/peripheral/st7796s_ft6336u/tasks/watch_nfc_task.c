#include "watch_nfc_task.h"

#include <stdio.h>
#include <string.h>
#include "common_def.h"
#include "../drivers/m1_pn532.h"
#include "securec.h"
#include "soc_osal.h"
#include "../business/watch_borrow.h"
#include "../model/watch_model.h"

#define WATCH_NFC_TASK_PRIO          24
#define WATCH_NFC_TASK_STACK_SIZE    0x1200
#define WATCH_NFC_SECTOR_TRAILER     7
#define WATCH_NFC_MAX_CONSEC_ERRORS  5
#define WATCH_NFC_IDLE_SLEEP_MS      200
#define WATCH_NFC_DUPLICATE_GUARD_MS 1000
#define WATCH_NFC_INIT_RETRY_MS      3000
#define WATCH_NFC_WAIT_LOG_PERIOD    10

static uint8_t g_nfc_started = 0;

static const uint8_t g_watch_nfc_key_88[M1_KEY_LEN] = {
    0x88, 0x88, 0x88, 0x88, 0x88, 0x88
};

static const uint8_t g_watch_nfc_key_ff[M1_KEY_LEN] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

static void watch_nfc_make_uid_text(const m1_card_t *card, char *buf, uint32_t len)
{
    uint32_t pos = 0;
    int n;

    if ((buf == NULL) || (len == 0)) {
        return;
    }
    buf[0] = '\0';
    if (card == NULL) {
        return;
    }

    for (uint8_t i = 0; (i < card->uid_len) && (pos + 2 < len); i++) {
        n = snprintf_s(&buf[pos], len - pos, len - pos - 1, "%02X", card->uid[i]);
        if (n <= 0) {
            break;
        }
        pos += (uint32_t)n;
    }
}

static void watch_nfc_log_card_uid(const m1_card_t *card)
{
    char message[WATCH_MODEL_LONG_LEN];
    char uid[(M1_UID_MAX_LEN * 2) + 1];

    watch_nfc_make_uid_text(card, uid, sizeof(uid));
    (void)snprintf_s(message, sizeof(message), sizeof(message) - 1, "card uid %s", uid);
    watch_model_add_log(WATCH_LOG_DEVICE, "NFC", message);
}

static m1_error_t watch_nfc_auth_person_sector(const m1_card_t *card)
{
    m1_error_t ret;

    ret = m1_pn532_auth_key_a_with_key(card, WATCH_NFC_SECTOR_TRAILER, g_watch_nfc_key_88);
    if (ret == M1_OK) {
        watch_model_add_log(WATCH_LOG_DEVICE, "NFC", "auth key 88 OK");
        return M1_OK;
    }
    printf("[NFC] auth key 88 failed, ret=%d\n", ret);

    ret = m1_pn532_auth_key_a_with_key(card, WATCH_NFC_SECTOR_TRAILER, g_watch_nfc_key_ff);
    if (ret == M1_OK) {
        watch_model_add_log(WATCH_LOG_DEVICE, "NFC", "auth key FF OK");
        return M1_OK;
    }

    printf("[NFC] auth key FF failed, ret=%d\n", ret);
    watch_model_add_log(WATCH_LOG_WARNING, "NFC", "auth failed");
    return ret;
}

static m1_error_t watch_nfc_read_person_info(m1_person_info_t *info)
{
    uint8_t name_blocks[M1_NAME_BLOCK_COUNT][M1_BLOCK_SIZE];
    uint8_t id_block[M1_BLOCK_SIZE];
    m1_error_t ret;

    if (info == NULL) {
        return M1_ERR_PARAM;
    }
    (void)memset_s(info, sizeof(*info), 0, sizeof(*info));

    ret = m1_pn532_read_block(M1_NAME_START_BLOCK, name_blocks[0]);
    if (ret != M1_OK) {
        watch_model_add_log(WATCH_LOG_WARNING, "NFC", "read name block failed");
        return ret;
    }
    ret = m1_pn532_read_block(M1_ID_BLOCK, id_block);
    if (ret != M1_OK) {
        watch_model_add_log(WATCH_LOG_WARNING, "NFC", "read id block failed");
        return ret;
    }

    ret = m1_unpack_name_blocks(name_blocks, info->name_utf8, &info->name_len);
    if (ret != M1_OK) {
        watch_model_add_log(WATCH_LOG_WARNING, "NFC", "parse name failed");
        return ret;
    }
    ret = m1_unpack_id_block(id_block, info->id_number);
    if (ret != M1_OK) {
        watch_model_add_log(WATCH_LOG_WARNING, "NFC", "parse id failed");
    }
    return ret;
}

static m1_error_t watch_nfc_init_reader(void)
{
    m1_error_t ret;
    uint8_t scl = 0;
    uint8_t sda = 0;

    ret = (m1_error_t)m1_pn532_soft_i2c_pin_init();
    if (ret != M1_OK) {
        printf("[NFC] soft I2C init failed, ret=%d\n", ret);
        watch_model_add_log(WATCH_LOG_WARNING, "NFC", "soft I2C init failed");
        return ret;
    }
    printf("[NFC] soft I2C init OK, scl=%d, sda=%d\n",
           CONFIG_M1_PN532_SOFT_I2C_SCL_PIN,
           CONFIG_M1_PN532_SOFT_I2C_SDA_PIN);

    m1_pn532_get_i2c_levels(&scl, &sda);
    printf("[NFC] I2C level before probe: scl=%u, sda=%u\n", scl, sda);

    ret = m1_pn532_i2c_probe();
    if (ret != M1_OK) {
        printf("[NFC] PN532 I2C probe failed, ret=%d\n", ret);
        watch_model_add_log(WATCH_LOG_WARNING, "NFC", "PN532 probe failed");
    }

    m1_pn532_get_i2c_levels(&scl, &sda);
    printf("[NFC] I2C level after probe: scl=%u, sda=%u\n", scl, sda);

    ret = m1_pn532_sam_config();
    if (ret != M1_OK) {
        printf("[NFC] SAM config failed, ret=%d\n", ret);
        watch_model_add_log(WATCH_LOG_WARNING, "NFC", "SAM config failed");
        return ret;
    }

    watch_model_add_log(WATCH_LOG_DEVICE, "NFC", "PN532 ready");
    return M1_OK;
}

static void watch_nfc_handle_card(const m1_card_t *card, uint8_t *err_count)
{
    m1_error_t ret;
    m1_person_info_t info;

    ret = watch_nfc_auth_person_sector(card);
    if (ret != M1_OK) {
        (*err_count)++;
        return;
    }

    ret = watch_nfc_read_person_info(&info);
    if (ret != M1_OK) {
        printf("[NFC] read person info failed, ret=%d\n", ret);
        watch_model_add_log(WATCH_LOG_WARNING, "NFC", "read card failed");
        (*err_count)++;
        return;
    }

    printf("[NFC] ID: %s, NAME: %s\n", info.id_number, (const char *)info.name_utf8);
    watch_model_add_log(WATCH_LOG_DEVICE, "NFC", "identity read OK");
    watch_borrow_on_identity_received((const char *)info.name_utf8, info.id_number);
    *err_count = 0;
}

static void *watch_nfc_task(const char *arg)
{
    m1_error_t ret;
    m1_card_t card;
    uint8_t err_count = 0;
    uint8_t wait_log_count = 0;

    unused(arg);
    watch_model_add_log(WATCH_LOG_DEVICE, "NFC", "task started");

    while (watch_nfc_init_reader() != M1_OK) {
        watch_model_add_log(WATCH_LOG_WARNING, "NFC", "PN532 init retry");
        osal_msleep(WATCH_NFC_INIT_RETRY_MS);
    }

    while (1) {
        if (watch_borrow_is_waiting_card() == 0) {
            wait_log_count = 0;
            osal_msleep(WATCH_NFC_IDLE_SLEEP_MS);
            continue;
        }

        if (wait_log_count == 0) {
            watch_model_add_log(WATCH_LOG_DEVICE, "NFC", "waiting card");
        }
        wait_log_count = (uint8_t)((wait_log_count + 1) % WATCH_NFC_WAIT_LOG_PERIOD);

        if (err_count >= WATCH_NFC_MAX_CONSEC_ERRORS) {
            watch_model_add_log(WATCH_LOG_WARNING, "NFC", "reset SAM");
            ret = m1_pn532_sam_config();
            if (ret != M1_OK) {
                printf("[NFC] SAM reset failed, ret=%d\n", ret);
                watch_model_add_log(WATCH_LOG_WARNING, "NFC", "SAM reset failed");
            }
            err_count = 0;
            osal_msleep(CONFIG_M1_PN532_POLL_INTERVAL_MS);
        }

        (void)memset_s(&card, sizeof(card), 0, sizeof(card));
        ret = m1_pn532_poll_card(&card);
        if (ret == M1_ERR_NO_TARGET) {
            osal_msleep(CONFIG_M1_PN532_POLL_INTERVAL_MS);
            continue;
        }
        if (ret != M1_OK) {
            printf("[NFC] poll card failed, ret=%d\n", ret);
            watch_model_add_log(WATCH_LOG_WARNING, "NFC", "poll failed");
            err_count++;
            (void)m1_pn532_release_target(card.target_number != 0 ? card.target_number : 1);
            osal_msleep(CONFIG_M1_PN532_POLL_INTERVAL_MS);
            continue;
        }

        watch_nfc_log_card_uid(&card);
        watch_nfc_handle_card(&card, &err_count);
        (void)m1_pn532_release_target(card.target_number != 0 ? card.target_number : 1);
        osal_msleep(WATCH_NFC_DUPLICATE_GUARD_MS);
    }

    return NULL;
}

void watch_nfc_task_start(void)
{
    osal_task *task_handle = NULL;

    if (g_nfc_started != 0) {
        return;
    }
    g_nfc_started = 1;

    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)watch_nfc_task, 0, "WatchNfcTask",
                                      WATCH_NFC_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, WATCH_NFC_TASK_PRIO);
        osal_kfree(task_handle);
    } else {
        g_nfc_started = 0;
        printf("[NFC] create task failed\n");
        watch_model_add_log(WATCH_LOG_WARNING, "NFC", "task create failed");
    }
    osal_kthread_unlock();
}
