#include "vitals_report.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "mqtt_task.h"
#include "nv.h"
#include "securec.h"
#include "soc_osal.h"
#include "../model/watch_model.h"

#define VITALS_REPORT_NV_KEY 0x5002
#define VITALS_REPORT_MAGIC 0x56525031U
#define VITALS_REPORT_VERSION 1
#define VITALS_REPORT_PENDING_MAX 8
#define VITALS_REPORT_MAX_LEN 768
#define VITALS_REPORT_QUEUE_LEN 4
#define VITALS_REPORT_TASK_STACK_SIZE 0x1800
#define VITALS_REPORT_TASK_PRIORITY 26
#define VITALS_RPT_PREFIX "RPT:"
#define VITALS_RPT_PREFIX_LEN 4
#define VITALS_RPT_MAX_FRAGMENTS 8
#define VITALS_RPT_CHUNK_MAX_LEN 220

typedef struct {
    uint16_t len;
    char payload[VITALS_REPORT_MAX_LEN];
} vitals_report_slot_t;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint8_t count;
    uint8_t head;
    vitals_report_slot_t reports[VITALS_REPORT_PENDING_MAX];
} vitals_pending_store_t;

typedef struct {
    uint8_t valid;
    uint8_t complete;
    uint16_t len;
    char payload[VITALS_REPORT_MAX_LEN];
} vitals_latest_report_t;

typedef struct {
    uint8_t active;
    uint16_t msg_id;
    uint8_t total;
    uint8_t received_mask;
    uint16_t chunk_len[VITALS_RPT_MAX_FRAGMENTS];
    char chunks[VITALS_RPT_MAX_FRAGMENTS][VITALS_RPT_CHUNK_MAX_LEN + 1];
} vitals_rpt_reasm_t;

static vitals_latest_report_t g_latest_reports[WATCH_MODEL_MAX_DEVICES];
static vitals_rpt_reasm_t g_rpt_reasm[WATCH_MODEL_MAX_DEVICES];
static char g_json_parse_buf[VITALS_REPORT_MAX_LEN];
static char g_rpt_packet_buf[VITALS_REPORT_MAX_LEN];
static char g_rpt_full_json_buf[VITALS_REPORT_MAX_LEN];
static osal_mutex g_vitals_lock;
static uint8_t g_vitals_lock_ready = 0;
static vitals_pending_store_t g_pending_store;
static osal_mutex g_pending_lock;
static uint8_t g_pending_lock_ready = 0;
static unsigned long g_vitals_msg_queue = 0;
static uint8_t g_vitals_task_started = 0;

static void vitals_report_handle_json(uint8_t device_index, const uint8_t *data, uint16_t len);
static void vitals_report_handle_fragment(uint8_t device_index, const uint8_t *data, uint16_t len);

static void vitals_lock_init(void)
{
    if (g_vitals_lock_ready == 0) {
        (void)osal_mutex_init(&g_vitals_lock);
        g_vitals_lock_ready = 1;
    }
}

static void vitals_lock(void)
{
    vitals_lock_init();
    if (g_vitals_lock_ready != 0) {
        (void)osal_mutex_lock(&g_vitals_lock);
    }
}

static void vitals_unlock(void)
{
    if (g_vitals_lock_ready != 0) {
        osal_mutex_unlock(&g_vitals_lock);
    }
}

static void pending_lock_init(void)
{
    if (g_pending_lock_ready == 0) {
        (void)osal_mutex_init(&g_pending_lock);
        g_pending_lock_ready = 1;
    }
}

static void pending_lock(void)
{
    pending_lock_init();
    if (g_pending_lock_ready != 0) {
        (void)osal_mutex_lock(&g_pending_lock);
    }
}

static void pending_unlock(void)
{
    if (g_pending_lock_ready != 0) {
        osal_mutex_unlock(&g_pending_lock);
    }
}

static const char *skip_spaces(const char *p)
{
    while ((p != NULL) && ((*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n'))) {
        p++;
    }
    return p;
}

static bool json_field_string_equals(const char *payload, const char *field, const char *expected)
{
    char needle[32];
    const char *p;
    size_t needle_len;
    size_t expected_len;

    if ((payload == NULL) || (field == NULL) || (expected == NULL)) {
        return false;
    }
    if (snprintf_s(needle, sizeof(needle), sizeof(needle) - 1, "\"%s\"", field) <= 0) {
        return false;
    }

    needle_len = strlen(needle);
    expected_len = strlen(expected);
    p = payload;
    while ((p = strstr(p, needle)) != NULL) {
        p += needle_len;
        p = skip_spaces(p);
        if ((p == NULL) || (*p != ':')) {
            continue;
        }
        p++;
        p = skip_spaces(p);
        if ((p != NULL) && (*p == '"') && (strncmp(p + 1, expected, expected_len) == 0) &&
            (p[1 + expected_len] == '"')) {
            return true;
        }
    }
    return false;
}

static bool json_field_bool_is_true(const char *payload, const char *field)
{
    char needle[32];
    const char *p;
    size_t needle_len;

    if ((payload == NULL) || (field == NULL)) {
        return false;
    }
    if (snprintf_s(needle, sizeof(needle), sizeof(needle) - 1, "\"%s\"", field) <= 0) {
        return false;
    }

    needle_len = strlen(needle);
    p = payload;
    while ((p = strstr(p, needle)) != NULL) {
        p += needle_len;
        p = skip_spaces(p);
        if ((p == NULL) || (*p != ':')) {
            continue;
        }
        p++;
        p = skip_spaces(p);
        if ((p != NULL) && (strncmp(p, "true", 4) == 0) &&
            ((p[4] == ',') || (p[4] == '}') || (p[4] == ' ') || (p[4] == '\t') || (p[4] == '\r') || (p[4] == '\n'))) {
            return true;
        }
    }
    return false;
}

static bool vitals_report_is_vitals(const char *payload)
{
    return (payload != NULL) && (payload[0] == '{') && json_field_string_equals(payload, "type", "vitals");
}

static const char *json_last_non_space(const char *payload)
{
    const char *last = NULL;

    if (payload == NULL) {
        return NULL;
    }

    while (*payload != '\0') {
        if ((*payload != ' ') && (*payload != '\t') && (*payload != '\r') && (*payload != '\n')) {
            last = payload;
        }
        payload++;
    }
    return last;
}

static bool json_object_is_closed(const char *payload)
{
    const char *p;
    const char *last;
    uint16_t depth = 0;
    bool in_string = false;
    bool escaped = false;

    if ((payload == NULL) || (payload[0] != '{')) {
        return false;
    }

    last = json_last_non_space(payload);
    if ((last == NULL) || (*last != '}')) {
        return false;
    }

    for (p = payload; *p != '\0'; p++) {
        if (escaped) {
            escaped = false;
            continue;
        }

        if (in_string) {
            if (*p == '\\') {
                escaped = true;
            } else if (*p == '"') {
                in_string = false;
            }
            continue;
        }

        if (*p == '"') {
            in_string = true;
        } else if (*p == '{') {
            depth++;
        } else if (*p == '}') {
            if (depth == 0) {
                return false;
            }
            depth--;
        }
    }

    return (!in_string && !escaped && (depth == 0));
}

static bool vitals_report_is_complete(const char *payload)
{
    return vitals_report_is_vitals(payload) && json_object_is_closed(payload) &&
           json_field_bool_is_true(payload, "complete");
}

static bool parse_u16_field(const char **cursor, uint16_t *out)
{
    uint32_t value = 0;
    const char *p;

    if ((cursor == NULL) || (*cursor == NULL) || (out == NULL)) {
        return false;
    }

    p = *cursor;
    if ((*p < '0') || (*p > '9')) {
        return false;
    }

    while ((*p >= '0') && (*p <= '9')) {
        value = value * 10 + (uint32_t)(*p - '0');
        if (value > 65535U) {
            return false;
        }
        p++;
    }

    if (*p != ':') {
        return false;
    }

    *out = (uint16_t)value;
    *cursor = p + 1;
    return true;
}

static void vitals_report_clear_latest(uint8_t device_index)
{
    if (device_index >= WATCH_MODEL_MAX_DEVICES) {
        return;
    }

    vitals_lock();
    (void)memset_s(&g_latest_reports[device_index], sizeof(g_latest_reports[device_index]), 0,
                   sizeof(g_latest_reports[device_index]));
    vitals_unlock();
}

static void pending_store_reset(vitals_pending_store_t *store)
{
    if (store == NULL) {
        return;
    }
    (void)memset_s(store, sizeof(*store), 0, sizeof(*store));
    store->magic = VITALS_REPORT_MAGIC;
    store->version = VITALS_REPORT_VERSION;
}

static errcode_t pending_store_load(vitals_pending_store_t *store)
{
    uint16_t value_len = 0;

    if (store == NULL) {
        return ERRCODE_FAIL;
    }

    (void)memset_s(store, sizeof(*store), 0, sizeof(*store));
    if (uapi_nv_read(VITALS_REPORT_NV_KEY, sizeof(*store), &value_len, (uint8_t *)store) != ERRCODE_SUCC) {
        pending_store_reset(store);
        return ERRCODE_FAIL;
    }

    if ((value_len != sizeof(*store)) || (store->magic != VITALS_REPORT_MAGIC) ||
        (store->version != VITALS_REPORT_VERSION) || (store->count > VITALS_REPORT_PENDING_MAX) ||
        (store->head >= VITALS_REPORT_PENDING_MAX)) {
        pending_store_reset(store);
        return ERRCODE_FAIL;
    }
    return ERRCODE_SUCC;
}

static errcode_t pending_store_save(const vitals_pending_store_t *store)
{
    if (store == NULL) {
        return ERRCODE_FAIL;
    }
    if (uapi_nv_write(VITALS_REPORT_NV_KEY, (const uint8_t *)store, sizeof(*store)) != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }
    (void)uapi_nv_flush();
    return ERRCODE_SUCC;
}

static uint8_t pending_tail_index(const vitals_pending_store_t *store)
{
    return (uint8_t)((store->head + store->count) % VITALS_REPORT_PENDING_MAX);
}

static errcode_t vitals_report_route_payload(const char *payload)
{
    errcode_t ret;

    if ((payload == NULL) || (payload[0] == '\0')) {
        return ERRCODE_FAIL;
    }

    if (!mqtt_task_is_connected()) {
        return vitals_report_cache_pending(payload);
    }

    if (mqtt_task_enqueue_report(payload) == ERRCODE_SUCC) {
        return ERRCODE_SUCC;
    }

    ret = vitals_report_cache_pending(payload);
    return ret;
}

static void *vitals_report_task(const char *arg)
{
    char payload[VITALS_REPORT_MAX_LEN];
    unsigned int read_size;

    (void)arg;

    while (1) {
        read_size = sizeof(payload);
        if (osal_msg_queue_read_copy(g_vitals_msg_queue, payload, &read_size, OSAL_WAIT_FOREVER) != 0) {
            continue;
        }
        if ((read_size == 0) || (read_size > sizeof(payload))) {
            continue;
        }
        payload[sizeof(payload) - 1] = '\0';
        osal_printk("[REPORT] task route len=%u payload:%s\r\n",
                    (unsigned int)strlen(payload), payload);
        (void)vitals_report_route_payload(payload);
    }

    return NULL;
}

void vitals_report_start(void)
{
    osal_task *task_handle = NULL;

    if (g_vitals_task_started != 0) {
        return;
    }

    if (g_vitals_msg_queue == 0) {
        (void)osal_msg_queue_create("vitals_report", VITALS_REPORT_QUEUE_LEN, &g_vitals_msg_queue, 0,
                                    VITALS_REPORT_MAX_LEN);
        if (g_vitals_msg_queue == 0) {
            watch_model_add_log(WATCH_LOG_WARNING, "REPORT", "queue create failed");
            return;
        }
    }

    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)vitals_report_task, 0, "VitalsReportTask",
                                      VITALS_REPORT_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, VITALS_REPORT_TASK_PRIORITY);
        osal_kfree(task_handle);
        g_vitals_task_started = 1;
        osal_printk("[REPORT] task started queue=0x%x\r\n", (unsigned int)g_vitals_msg_queue);
    }
    osal_kthread_unlock();

    if (g_vitals_task_started == 0) {
        watch_model_add_log(WATCH_LOG_WARNING, "REPORT", "task create failed");
    }
}

errcode_t vitals_report_cache_pending(const char *payload)
{
    uint8_t index;
    uint16_t len;

    if ((payload == NULL) || (payload[0] == '\0')) {
        return ERRCODE_FAIL;
    }

    len = (uint16_t)strlen(payload);
    if (len >= VITALS_REPORT_MAX_LEN) {
        watch_model_add_log(WATCH_LOG_WARNING, "REPORT", "report too large");
        return ERRCODE_FAIL;
    }

    pending_lock();
    (void)pending_store_load(&g_pending_store);
    if (g_pending_store.count >= VITALS_REPORT_PENDING_MAX) {
        g_pending_store.head = (uint8_t)((g_pending_store.head + 1) % VITALS_REPORT_PENDING_MAX);
        g_pending_store.count = VITALS_REPORT_PENDING_MAX - 1;
    }

    index = pending_tail_index(&g_pending_store);
    g_pending_store.reports[index].len = len;
    (void)memset_s(g_pending_store.reports[index].payload, sizeof(g_pending_store.reports[index].payload), 0,
                   sizeof(g_pending_store.reports[index].payload));
    if (strncpy_s(g_pending_store.reports[index].payload, sizeof(g_pending_store.reports[index].payload), payload,
                  sizeof(g_pending_store.reports[index].payload) - 1) != EOK) {
        pending_unlock();
        return ERRCODE_FAIL;
    }
    g_pending_store.count++;

    if (pending_store_save(&g_pending_store) != ERRCODE_SUCC) {
        pending_unlock();
        watch_model_add_log(WATCH_LOG_WARNING, "REPORT", "cache failed");
        return ERRCODE_FAIL;
    }
    pending_unlock();

    watch_model_add_log(WATCH_LOG_MQTT, "REPORT", "report cached");
    return ERRCODE_SUCC;
}

static bool vitals_report_store_json_locked(uint8_t device_index, const char *payload, uint16_t len,
                                            bool *is_complete_out, bool *complete_field_true_out)
{
    bool is_complete;
    bool complete_field_true;

    if (!vitals_report_is_vitals(payload)) {
        return false;
    }

    if (device_index >= WATCH_MODEL_MAX_DEVICES) {
        device_index = 0;
    }

    is_complete = vitals_report_is_complete(payload);
    complete_field_true = json_field_bool_is_true(payload, "complete");

    g_latest_reports[device_index].valid = 1;
    g_latest_reports[device_index].complete = is_complete ? 1 : 0;
    g_latest_reports[device_index].len = len;
    (void)strncpy_s(g_latest_reports[device_index].payload, sizeof(g_latest_reports[device_index].payload),
                    payload, sizeof(g_latest_reports[device_index].payload) - 1);
    if (is_complete_out != NULL) {
        *is_complete_out = is_complete;
    }
    if (complete_field_true_out != NULL) {
        *complete_field_true_out = complete_field_true;
    }
    return true;
}

static void vitals_report_handle_json(uint8_t device_index, const uint8_t *data, uint16_t len)
{
    char *payload = g_json_parse_buf;
    uint16_t copy_len;
    bool is_complete = false;
    bool complete_field_true = false;
    bool stored;

    if ((data == NULL) || (len == 0)) {
        return;
    }
    if (len >= VITALS_REPORT_MAX_LEN) {
        watch_model_add_log(WATCH_LOG_WARNING, "REPORT", "report too large");
        return;
    }

    copy_len = len;
    vitals_lock();
    if (memcpy_s(payload, VITALS_REPORT_MAX_LEN, data, copy_len) != EOK) {
        vitals_unlock();
        return;
    }
    payload[copy_len] = '\0';
    stored = vitals_report_store_json_locked(device_index, payload, copy_len, &is_complete, &complete_field_true);
    vitals_unlock();
    if (!stored) {
        return;
    }

    if (is_complete) {
        osal_printk("[REPORT] report ready device=%u len=%u\r\n",
                    (unsigned int)device_index, (unsigned int)copy_len);
        watch_model_add_log(WATCH_LOG_DEVICE, "REPORT", "report ready");
    } else if (complete_field_true) {
        osal_printk("[REPORT] report incomplete json device=%u len=%u\r\n",
                    (unsigned int)device_index, (unsigned int)copy_len);
    }
}

static void vitals_report_handle_fragment(uint8_t device_index, const uint8_t *data, uint16_t len)
{
    char *packet = g_rpt_packet_buf;
    char *full_json = g_rpt_full_json_buf;
    const char *p;
    const char *chunk;
    uint16_t msg_id;
    uint16_t seq;
    uint16_t total;
    uint16_t chunk_len;
    uint16_t full_len = 0;
    uint8_t expected_mask;
    uint8_t full_ready = 0;
    bool stored = false;
    bool is_complete = false;
    bool complete_field_true = false;
    vitals_rpt_reasm_t *reasm;

    if ((data == NULL) || (len <= VITALS_RPT_PREFIX_LEN) || (len >= VITALS_REPORT_MAX_LEN)) {
        return;
    }

    if (device_index >= WATCH_MODEL_MAX_DEVICES) {
        device_index = 0;
    }

    vitals_lock();
    if (memcpy_s(packet, VITALS_REPORT_MAX_LEN, data, len) != EOK) {
        vitals_unlock();
        return;
    }
    packet[len] = '\0';

    p = packet + VITALS_RPT_PREFIX_LEN;
    if (!parse_u16_field(&p, &msg_id) || !parse_u16_field(&p, &seq) || !parse_u16_field(&p, &total)) {
        osal_printk("[REPORT] bad fragment header device=%u len=%u\r\n",
                    (unsigned int)device_index, (unsigned int)len);
        vitals_unlock();
        return;
    }

    if ((total == 0) || (total > VITALS_RPT_MAX_FRAGMENTS) || (seq >= total)) {
        osal_printk("[REPORT] bad fragment index device=%u msg=%u seq=%u total=%u\r\n",
                    (unsigned int)device_index, (unsigned int)msg_id,
                    (unsigned int)seq, (unsigned int)total);
        vitals_unlock();
        return;
    }

    chunk = p;
    chunk_len = (uint16_t)strlen(chunk);
    if (chunk_len > VITALS_RPT_CHUNK_MAX_LEN) {
        osal_printk("[REPORT] fragment chunk too large device=%u msg=%u seq=%u chunk=%u\r\n",
                    (unsigned int)device_index, (unsigned int)msg_id,
                    (unsigned int)seq, (unsigned int)chunk_len);
        vitals_unlock();
        return;
    }

    reasm = &g_rpt_reasm[device_index];
    if ((reasm->active == 0) || (reasm->msg_id != msg_id)) {
        (void)memset_s(reasm, sizeof(*reasm), 0, sizeof(*reasm));
        reasm->active = 1;
        reasm->msg_id = msg_id;
        reasm->total = (uint8_t)total;
    }

    if (reasm->total != (uint8_t)total) {
        osal_printk("[REPORT] fragment total changed device=%u msg=%u old=%u new=%u\r\n",
                    (unsigned int)device_index, (unsigned int)msg_id,
                    (unsigned int)reasm->total, (unsigned int)total);
        (void)memset_s(reasm, sizeof(*reasm), 0, sizeof(*reasm));
        vitals_unlock();
        return;
    }

    (void)memset_s(reasm->chunks[seq], sizeof(reasm->chunks[seq]), 0, sizeof(reasm->chunks[seq]));
    if ((chunk_len > 0) &&
        (memcpy_s(reasm->chunks[seq], sizeof(reasm->chunks[seq]), chunk, chunk_len) != EOK)) {
        (void)memset_s(reasm, sizeof(*reasm), 0, sizeof(*reasm));
        vitals_unlock();
        return;
    }
    reasm->chunks[seq][chunk_len] = '\0';
    reasm->chunk_len[seq] = chunk_len;
    reasm->received_mask |= (uint8_t)(1U << seq);

    osal_printk("[REPORT] rpt fragment device=%u msg=%u seq=%u total=%u chunk=%u mask=0x%x\r\n",
                (unsigned int)device_index, (unsigned int)msg_id,
                (unsigned int)seq, (unsigned int)total,
                (unsigned int)chunk_len, (unsigned int)reasm->received_mask);

    expected_mask = (uint8_t)((1U << total) - 1U);
    if (reasm->received_mask == expected_mask) {
        (void)memset_s(full_json, VITALS_REPORT_MAX_LEN, 0, VITALS_REPORT_MAX_LEN);
        for (uint8_t i = 0; i < total; i++) {
            if ((full_len + reasm->chunk_len[i]) >= VITALS_REPORT_MAX_LEN) {
                osal_printk("[REPORT] fragment overflow device=%u msg=%u len=%u chunk=%u\r\n",
                            (unsigned int)device_index, (unsigned int)msg_id,
                            (unsigned int)full_len, (unsigned int)reasm->chunk_len[i]);
                (void)memset_s(reasm, sizeof(*reasm), 0, sizeof(*reasm));
                vitals_unlock();
                return;
            }
            if ((reasm->chunk_len[i] > 0) &&
                (memcpy_s(&full_json[full_len], VITALS_REPORT_MAX_LEN - full_len,
                          reasm->chunks[i], reasm->chunk_len[i]) != EOK)) {
                (void)memset_s(reasm, sizeof(*reasm), 0, sizeof(*reasm));
                vitals_unlock();
                return;
            }
            full_len += reasm->chunk_len[i];
        }
        full_json[full_len] = '\0';
        full_ready = 1;
        stored = vitals_report_store_json_locked(device_index, full_json, full_len,
                                                 &is_complete, &complete_field_true);
        (void)memset_s(reasm, sizeof(*reasm), 0, sizeof(*reasm));
    }
    vitals_unlock();

    if (full_ready != 0) {
        osal_printk("[REPORT] rpt complete device=%u msg=%u len=%u\r\n",
                    (unsigned int)device_index, (unsigned int)msg_id, (unsigned int)full_len);
        osal_printk("[REPORT] rpt json device=%u len=%u json:%s\r\n",
                    (unsigned int)device_index, (unsigned int)full_len, full_json);
        if (stored && is_complete) {
            osal_printk("[REPORT] report ready device=%u len=%u\r\n",
                        (unsigned int)device_index, (unsigned int)full_len);
            watch_model_add_log(WATCH_LOG_DEVICE, "REPORT", "report ready");
        } else if (stored && complete_field_true) {
            osal_printk("[REPORT] report incomplete json device=%u len=%u\r\n",
                        (unsigned int)device_index, (unsigned int)full_len);
        }
    }
}

void vitals_report_handle(uint8_t device_index, const uint8_t *data, uint16_t len)
{
    if ((data == NULL) || (len == 0)) {
        return;
    }

    if ((len >= VITALS_RPT_PREFIX_LEN) &&
        (memcmp(data, VITALS_RPT_PREFIX, VITALS_RPT_PREFIX_LEN) == 0)) {
        vitals_report_handle_fragment(device_index, data, len);
        return;
    }

    vitals_report_handle_json(device_index, data, len);
}

errcode_t vitals_report_submit_on_return(uint8_t device_index)
{
    char payload[VITALS_REPORT_MAX_LEN];
    uint8_t has_report;
    uint8_t complete;
    uint32_t len;
    errcode_t ret = ERRCODE_SUCC;

    if (device_index >= WATCH_MODEL_MAX_DEVICES) {
        osal_printk("[REPORT] submit invalid device=%u\r\n", (unsigned int)device_index);
        return ERRCODE_FAIL;
    }

    vitals_lock();
    has_report = g_latest_reports[device_index].valid;
    complete = g_latest_reports[device_index].complete;
    osal_printk("[REPORT] submit on return device=%u has=%u complete=%u mqtt=%u\r\n",
                (unsigned int)device_index, (unsigned int)has_report,
                (unsigned int)complete, (unsigned int)mqtt_task_is_connected());

    if (has_report == 0) {
        vitals_unlock();
        osal_printk("[REPORT] submit no report device=%u\r\n", (unsigned int)device_index);
        watch_model_add_log(WATCH_LOG_WARNING, "REPORT", "no vitals report");
        return ERRCODE_FAIL;
    }
    if (complete == 0) {
        vitals_unlock();
        osal_printk("[REPORT] submit incomplete device=%u\r\n", (unsigned int)device_index);
        watch_model_add_log(WATCH_LOG_WARNING, "REPORT", "vitals incomplete");
        vitals_report_clear_latest(device_index);
        return ERRCODE_FAIL;
    }

    len = (uint32_t)strlen(g_latest_reports[device_index].payload) + 1;
    if (len > sizeof(payload)) {
        vitals_unlock();
        osal_printk("[REPORT] submit payload too large len=%u\r\n", (unsigned int)len);
        watch_model_add_log(WATCH_LOG_WARNING, "REPORT", "report too large");
        return ERRCODE_FAIL;
    }
    if (memcpy_s(payload, sizeof(payload), g_latest_reports[device_index].payload, len) != EOK) {
        vitals_unlock();
        osal_printk("[REPORT] submit payload copy failed len=%u\r\n", (unsigned int)len);
        return ERRCODE_FAIL;
    }
    osal_printk("[REPORT] submit payload len=%u json:%s\r\n",
                (unsigned int)(len - 1), payload);
    vitals_unlock();

    ret = vitals_report_route_payload(payload);
    osal_printk("[REPORT] submit route ret=0x%x len=%u\r\n", (unsigned int)ret, (unsigned int)len);

    if (ret == ERRCODE_SUCC) {
        vitals_lock();
        (void)memset_s(&g_latest_reports[device_index], sizeof(g_latest_reports[device_index]), 0,
                       sizeof(g_latest_reports[device_index]));
        vitals_unlock();
    }

    if (ret != ERRCODE_SUCC) {
        watch_model_add_log(WATCH_LOG_WARNING, "REPORT", "submit failed");
    }
    return ret;
}

void vitals_report_flush_pending(void)
{
    if (!mqtt_task_is_connected()) {
        return;
    }

    pending_lock();
    if (pending_store_load(&g_pending_store) != ERRCODE_SUCC) {
        pending_unlock();
        return;
    }

    while ((g_pending_store.count > 0) && mqtt_task_is_connected()) {
        vitals_report_slot_t *slot = &g_pending_store.reports[g_pending_store.head];
        if ((slot->len == 0) || (slot->len >= VITALS_REPORT_MAX_LEN) || (slot->payload[0] == '\0')) {
            g_pending_store.head = (uint8_t)((g_pending_store.head + 1) % VITALS_REPORT_PENDING_MAX);
            g_pending_store.count--;
            (void)pending_store_save(&g_pending_store);
            continue;
        }
        if (mqtt_task_publish(slot->payload) != ERRCODE_SUCC) {
            break;
        }
        (void)memset_s(slot, sizeof(*slot), 0, sizeof(*slot));
        g_pending_store.head = (uint8_t)((g_pending_store.head + 1) % VITALS_REPORT_PENDING_MAX);
        g_pending_store.count--;
        (void)pending_store_save(&g_pending_store);
    }
    pending_unlock();
}
