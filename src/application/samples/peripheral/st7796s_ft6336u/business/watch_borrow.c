#include "watch_borrow.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "securec.h"
#include "soc_osal.h"
#include "../model/watch_model.h"
#include "../services/sle_watch_server.h"
#include "../services/vitals_report.h"
#include "../ui/watch_ui.h"

#define WATCH_BORROW_MAX_MASK_CHARS 6

typedef struct {
    uint8_t active;
    uint8_t identity_sent;
    uint8_t device_index;
    char name[WATCH_MODEL_NAME_LEN];
    char id_number[WATCH_MODEL_ID_LEN];
} watch_borrow_context_t;

static watch_borrow_context_t g_borrow_ctx = {
    .active = 0,
    .device_index = WATCH_MODEL_MAX_DEVICES,
};

static void copy_text(char *dst, uint32_t dst_len, const char *src)
{
    if ((dst == NULL) || (dst_len == 0)) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    if (strncpy_s(dst, dst_len, src, dst_len - 1) != EOK) {
        dst[dst_len - 1] = '\0';
    }
}

void watch_borrow_make_masked_id(const char *id_number, char *out, uint32_t out_len)
{
    uint32_t len;
    uint32_t keep;
    uint32_t mask_len;

    if ((out == NULL) || (out_len == 0)) {
        return;
    }
    out[0] = '\0';
    if (id_number == NULL) {
        return;
    }

    len = (uint32_t)strlen(id_number);
    if (len == 0) {
        return;
    }
    keep = len > 4 ? 4 : len;
    mask_len = len - keep;
    if (mask_len > WATCH_BORROW_MAX_MASK_CHARS) {
        mask_len = WATCH_BORROW_MAX_MASK_CHARS;
    }
    if (mask_len > out_len - 1) {
        mask_len = out_len - 1;
    }
    for (uint32_t i = 0; i < mask_len; i++) {
        out[i] = '*';
    }
    out[mask_len] = '\0';
    if (out_len > mask_len + 1) {
        (void)strncat_s(out, out_len, id_number + len - keep, keep);
    }
}

static bool borrow_index_valid(const watch_model_snapshot_t *model, uint8_t device_index)
{
    return (model != NULL) && (device_index < model->device_count) &&
           (device_index < WATCH_MODEL_MAX_DEVICES);
}

errcode_t watch_borrow_request(uint8_t device_index)
{
    watch_model_snapshot_t model;
    watch_device_t device;

    watch_model_get_snapshot(&model);
    if (!borrow_index_valid(&model, device_index)) {
        watch_model_add_log(WATCH_LOG_WARNING, "BORROW", "invalid device");
        return ERRCODE_FAIL;
    }

    device = model.devices[device_index];
    if (device.online == 0) {
        watch_model_add_log(WATCH_LOG_WARNING, "BORROW", "device offline");
        return ERRCODE_FAIL;
    }
    if (device.state == WATCH_DEVICE_BORROWED) {
        return watch_borrow_return(device_index);
    }

    device.state = WATCH_DEVICE_PENDING;
    copy_text(device.borrower, sizeof(device.borrower), "waiting card");
    device.borrower_id[0] = '\0';
    watch_model_update_device(device_index, &device);

    g_borrow_ctx.active = 1;
    g_borrow_ctx.identity_sent = 0;
    g_borrow_ctx.device_index = device_index;
    g_borrow_ctx.name[0] = '\0';
    g_borrow_ctx.id_number[0] = '\0';
    watch_model_add_log(WATCH_LOG_DEVICE, "BORROW", "waiting NFC card");
    return ERRCODE_SUCC;
}

errcode_t watch_borrow_return(uint8_t device_index)
{
    watch_model_snapshot_t model;
    watch_device_t device;
    errcode_t ret;
    errcode_t report_ret;

    watch_model_get_snapshot(&model);
    if (!borrow_index_valid(&model, device_index)) {
        watch_model_add_log(WATCH_LOG_WARNING, "BORROW", "invalid return");
        return ERRCODE_FAIL;
    }

    report_ret = vitals_report_submit_on_return(device_index);
    osal_printk("[BORROW] return submit report device=%u ret=0x%x\r\n",
                (unsigned int)device_index, (unsigned int)report_ret);

    ret = sle_watch_send_unbind_to_device(device_index);
    device = model.devices[device_index];
    device.state = WATCH_DEVICE_IDLE;
    device.borrower[0] = '\0';
    device.borrower_id[0] = '\0';
    watch_model_update_device(device_index, &device);
    watch_model_add_log(ret == ERRCODE_SUCC ? WATCH_LOG_DEVICE : WATCH_LOG_WARNING,
                        "BORROW", ret == ERRCODE_SUCC ? "return sent" : "return local");

    if ((g_borrow_ctx.active != 0) && (g_borrow_ctx.device_index == device_index)) {
        g_borrow_ctx.active = 0;
        g_borrow_ctx.identity_sent = 0;
        g_borrow_ctx.device_index = WATCH_MODEL_MAX_DEVICES;
    }
    return ret;
}

uint8_t watch_borrow_is_waiting_card(void)
{
    return ((g_borrow_ctx.active != 0) && (g_borrow_ctx.identity_sent == 0)) ? 1 : 0;
}

void watch_borrow_on_identity_received(const char *name, const char *id_number)
{
    watch_model_snapshot_t model;
    watch_device_t device;
    char masked[WATCH_MODEL_ID_LEN] = {0};
    errcode_t ret;

    if ((name == NULL) || (id_number == NULL)) {
        watch_model_add_log(WATCH_LOG_WARNING, "NFC", "invalid card data");
        return;
    }
    if (g_borrow_ctx.active == 0) {
        watch_model_add_log(WATCH_LOG_WARNING, "NFC", "no pending device");
        return;
    }
    if (g_borrow_ctx.identity_sent != 0) {
        watch_model_add_log(WATCH_LOG_WARNING, "NFC", "identity pending");
        return;
    }

    watch_model_get_snapshot(&model);
    if (!borrow_index_valid(&model, g_borrow_ctx.device_index)) {
        watch_model_add_log(WATCH_LOG_WARNING, "NFC", "pending device lost");
        g_borrow_ctx.active = 0;
        g_borrow_ctx.identity_sent = 0;
        g_borrow_ctx.device_index = WATCH_MODEL_MAX_DEVICES;
        return;
    }

    watch_borrow_make_masked_id(id_number, masked, sizeof(masked));
    device = model.devices[g_borrow_ctx.device_index];
    device.state = WATCH_DEVICE_BORROWED;
    copy_text(device.borrower, sizeof(device.borrower), name);
    copy_text(device.borrower_id, sizeof(device.borrower_id), masked);
    watch_model_update_device(g_borrow_ctx.device_index, &device);
    watch_ui_request_refresh();

    copy_text(g_borrow_ctx.name, sizeof(g_borrow_ctx.name), name);
    copy_text(g_borrow_ctx.id_number, sizeof(g_borrow_ctx.id_number), id_number);
    g_borrow_ctx.identity_sent = 1;

    ret = sle_watch_send_identity_to_device(g_borrow_ctx.device_index, name, id_number);
    if (ret == ERRCODE_SUCC) {
        watch_model_add_log(WATCH_LOG_SLE, "SLE", "identity sent");
    } else {
        watch_model_add_log(WATCH_LOG_WARNING, "SLE", "identity send failed");
    }
}

void watch_borrow_on_identity_ack(const char *name, const char *id_number)
{
    watch_model_snapshot_t model;
    watch_device_t device;
    char masked[WATCH_MODEL_ID_LEN] = {0};

    if (g_borrow_ctx.active == 0) {
        watch_model_add_log(WATCH_LOG_SLE, "SLE", "identity ack");
        return;
    }

    watch_model_get_snapshot(&model);
    if (!borrow_index_valid(&model, g_borrow_ctx.device_index)) {
        return;
    }

    device = model.devices[g_borrow_ctx.device_index];
    device.state = WATCH_DEVICE_BORROWED;
    copy_text(device.borrower, sizeof(device.borrower),
              (name != NULL && name[0] != '\0') ? name : g_borrow_ctx.name);
    watch_borrow_make_masked_id((id_number != NULL && id_number[0] != '\0') ? id_number : g_borrow_ctx.id_number,
                                masked, sizeof(masked));
    copy_text(device.borrower_id, sizeof(device.borrower_id), masked);
    watch_model_update_device(g_borrow_ctx.device_index, &device);
    watch_ui_request_refresh();

    g_borrow_ctx.active = 0;
    g_borrow_ctx.identity_sent = 0;
    g_borrow_ctx.device_index = WATCH_MODEL_MAX_DEVICES;
    watch_model_add_log(WATCH_LOG_DEVICE, "BORROW", "borrow confirmed");
}

void watch_borrow_on_unbound(void)
{
    watch_model_add_log(WATCH_LOG_DEVICE, "BORROW", "device unbound");
}

void watch_borrow_simulate_card(void)
{
    watch_borrow_on_identity_received("Test User", "110101199001011234");
}
