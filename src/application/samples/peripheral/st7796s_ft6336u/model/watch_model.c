#include "watch_model.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "securec.h"
#include "soc_osal.h"

static watch_model_snapshot_t g_model;
static osal_mutex g_model_lock;
static uint8_t g_model_lock_ready = 0;

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

static void model_lock(void)
{
    if (g_model_lock_ready != 0) {
        (void)osal_mutex_lock(&g_model_lock);
    }
}

static void model_unlock(void)
{
    if (g_model_lock_ready != 0) {
        osal_mutex_unlock(&g_model_lock);
    }
}

static void make_time(char *buf, uint32_t len)
{
    static uint32_t counter = 0;
    uint32_t minute;
    uint32_t second;

    counter++;
    minute = (38 + (counter / 60)) % 60;
    second = counter % 60;
    (void)snprintf_s(buf, len, len - 1, "13:%02u:%02u", minute, second);
}

static const char *link_state_log_text(watch_link_state_t state)
{
    switch (state) {
        case WATCH_LINK_CONNECTING:
            return "connecting";
        case WATCH_LINK_CONNECTED:
            return "connected";
        case WATCH_LINK_BROADCASTING:
            return "broadcasting";
        case WATCH_LINK_ERROR:
            return "error";
        default:
            return "disconnected";
    }
}

static const char *device_state_log_text(watch_device_state_t state)
{
    switch (state) {
        case WATCH_DEVICE_BORROWED:
            return "borrowed";
        case WATCH_DEVICE_PENDING:
            return "pending";
        default:
            return "idle";
    }
}

static void append_log_locked(watch_log_type_t type, const char *tag, const char *message)
{
    watch_log_entry_t entry = {0};

    entry.type = type;
    copy_text(entry.tag, sizeof(entry.tag), tag);
    copy_text(entry.message, sizeof(entry.message), message);
    make_time(entry.time, sizeof(entry.time));

    if (g_model.log_count < WATCH_MODEL_MAX_LOGS) {
        for (uint8_t i = g_model.log_count; i > 0; i--) {
            g_model.logs[i] = g_model.logs[i - 1];
        }
        g_model.log_count++;
    } else {
        for (uint8_t i = WATCH_MODEL_MAX_LOGS - 1; i > 0; i--) {
            g_model.logs[i] = g_model.logs[i - 1];
        }
    }
    g_model.logs[0] = entry;
    g_model.version++;
}

static void append_link_state_log_locked(watch_log_type_t type, const char *tag, watch_link_state_t state)
{
    char message[WATCH_MODEL_LONG_LEN];

    (void)snprintf_s(message, sizeof(message), sizeof(message) - 1, "state %s", link_state_log_text(state));
    append_log_locked(state == WATCH_LINK_ERROR ? WATCH_LOG_WARNING : type, tag, message);
}

static void append_device_state_log_locked(const watch_device_t *device)
{
    char message[WATCH_MODEL_LONG_LEN];

    if (device == NULL) {
        return;
    }
    (void)snprintf_s(message, sizeof(message), sizeof(message) - 1, "%s %s", device->name,
                     device_state_log_text(device->state));
    append_log_locked(WATCH_LOG_DEVICE, "DEV", message);
}

void watch_model_init(void)
{
    (void)memset_s(&g_model, sizeof(g_model), 0, sizeof(g_model));
    if (g_model_lock_ready == 0) {
        (void)osal_mutex_init(&g_model_lock);
        g_model_lock_ready = 1;
    }

    g_model.wifi_state = WATCH_LINK_DISCONNECTED;
    g_model.mqtt_state = WATCH_LINK_DISCONNECTED;
    g_model.sle_state = WATCH_LINK_CONNECTING;
    g_model.wifi_rssi = -42;
    copy_text(g_model.wifi_ssid, sizeof(g_model.wifi_ssid), "FSW");
    copy_text(g_model.wifi_ip, sizeof(g_model.wifi_ip), "--");
    copy_text(g_model.mqtt_broker, sizeof(g_model.mqtt_broker), "192.168.43.110:1883");
    copy_text(g_model.mqtt_topic, sizeof(g_model.mqtt_topic), "watch/sensors/report");
    copy_text(g_model.mqtt_last_report, sizeof(g_model.mqtt_last_report), "--");
    copy_text(g_model.sle_service, sizeof(g_model.sle_service), "sle_speed_server");
    watch_model_add_log(WATCH_LOG_SLE, "SLE", "WatchControl started");
}

void watch_model_get_snapshot(watch_model_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return;
    }

    model_lock();
    (void)memcpy_s(snapshot, sizeof(*snapshot), &g_model, sizeof(g_model));
    model_unlock();
}

void watch_model_set_wifi(watch_link_state_t state, const char *ssid, const char *ip, int8_t rssi)
{
    watch_link_state_t old_state;

    model_lock();
    old_state = g_model.wifi_state;
    g_model.wifi_state = state;
    copy_text(g_model.wifi_ssid, sizeof(g_model.wifi_ssid), ssid);
    copy_text(g_model.wifi_ip, sizeof(g_model.wifi_ip), ip);
    g_model.wifi_rssi = rssi;
    g_model.version++;
    if (old_state != state) {
        append_link_state_log_locked(WATCH_LOG_WIFI, "WiFi", state);
    }
    model_unlock();
}

void watch_model_set_mqtt(watch_link_state_t state, const char *broker, const char *topic, const char *last_report)
{
    watch_link_state_t old_state;

    model_lock();
    old_state = g_model.mqtt_state;
    g_model.mqtt_state = state;
    copy_text(g_model.mqtt_broker, sizeof(g_model.mqtt_broker), broker);
    copy_text(g_model.mqtt_topic, sizeof(g_model.mqtt_topic), topic);
    copy_text(g_model.mqtt_last_report, sizeof(g_model.mqtt_last_report), last_report);
    g_model.version++;
    if (old_state != state) {
        append_link_state_log_locked(WATCH_LOG_MQTT, "MQTT", state);
    }
    model_unlock();
}

void watch_model_set_sle(watch_link_state_t state, const char *service, uint8_t connected_count)
{
    watch_link_state_t old_state;

    model_lock();
    old_state = g_model.sle_state;
    g_model.sle_state = state;
    copy_text(g_model.sle_service, sizeof(g_model.sle_service), service);
    g_model.sle_connected_count = connected_count;
    g_model.version++;
    if (old_state != state) {
        append_link_state_log_locked(WATCH_LOG_SLE, "SLE", state);
    }
    model_unlock();
}

void watch_model_set_warning_count(uint8_t warning_count)
{
    model_lock();
    g_model.warning_count = warning_count;
    g_model.version++;
    model_unlock();
}

void watch_model_update_device(uint8_t index, const watch_device_t *device)
{
    watch_device_state_t old_state;
    bool state_changed = false;

    if ((index >= WATCH_MODEL_MAX_DEVICES) || (device == NULL)) {
        return;
    }

    model_lock();
    if (index < g_model.device_count) {
        old_state = g_model.devices[index].state;
        state_changed = (old_state != device->state);
    }
    (void)memcpy_s(&g_model.devices[index], sizeof(g_model.devices[index]), device, sizeof(*device));
    if (index >= g_model.device_count) {
        g_model.device_count = index + 1;
    }
    g_model.version++;
    if (state_changed) {
        append_device_state_log_locked(device);
    }
    model_unlock();
}

errcode_t watch_model_remove_device(uint8_t index)
{
    model_lock();
    if (index >= g_model.device_count) {
        model_unlock();
        return ERRCODE_FAIL;
    }

    for (uint8_t i = index; i + 1 < g_model.device_count; i++) {
        g_model.devices[i] = g_model.devices[i + 1];
    }
    if (g_model.device_count > 0) {
        g_model.device_count--;
        (void)memset_s(&g_model.devices[g_model.device_count], sizeof(g_model.devices[g_model.device_count]), 0,
                       sizeof(g_model.devices[g_model.device_count]));
    }
    g_model.sle_connected_count = 0;
    for (uint8_t i = 0; i < g_model.device_count; i++) {
        if (g_model.devices[i].online != 0) {
            g_model.sle_connected_count++;
        }
    }
    g_model.version++;
    model_unlock();
    return ERRCODE_SUCC;
}

void watch_model_clear_scan_results(void)
{
    model_lock();
    (void)memset_s(g_model.scan_results, sizeof(g_model.scan_results), 0, sizeof(g_model.scan_results));
    g_model.scan_count = 0;
    g_model.version++;
    model_unlock();
}

void watch_model_set_scan_active(uint8_t active)
{
    model_lock();
    g_model.scan_active = active != 0 ? 1 : 0;
    g_model.version++;
    model_unlock();
}

errcode_t watch_model_add_scan_result(const watch_scan_device_t *device, uint8_t *scan_index)
{
    if ((device == NULL) || (device->name[0] == '\0') || (device->address[0] == '\0')) {
        return ERRCODE_FAIL;
    }

    model_lock();
    for (uint8_t i = 0; i < g_model.scan_count; i++) {
        if (strncmp(g_model.scan_results[i].address, device->address,
                    sizeof(g_model.scan_results[i].address)) == 0) {
            g_model.scan_results[i].rssi = device->rssi;
            copy_text(g_model.scan_results[i].name, sizeof(g_model.scan_results[i].name), device->name);
            g_model.version++;
            if (scan_index != NULL) {
                *scan_index = i;
            }
            model_unlock();
            return ERRCODE_SUCC;
        }
    }

    if (g_model.scan_count < WATCH_MODEL_MAX_SCAN_RESULTS) {
        uint8_t index = g_model.scan_count;
        (void)memcpy_s(&g_model.scan_results[g_model.scan_count], sizeof(g_model.scan_results[g_model.scan_count]),
                       device, sizeof(*device));
        g_model.scan_count++;
        g_model.version++;
        if (scan_index != NULL) {
            *scan_index = index;
        }
        model_unlock();
        return ERRCODE_SUCC;
    }
    model_unlock();
    return ERRCODE_FAIL;
}

errcode_t watch_model_add_device_from_scan(uint8_t scan_index)
{
    watch_device_t device = {0};

    model_lock();
    if ((scan_index >= g_model.scan_count) || (g_model.device_count >= WATCH_MODEL_MAX_DEVICES)) {
        model_unlock();
        return ERRCODE_FAIL;
    }

    for (uint8_t i = 0; i < g_model.device_count; i++) {
        if (strncmp(g_model.devices[i].address, g_model.scan_results[scan_index].address,
                    sizeof(g_model.devices[i].address)) == 0) {
            g_model.scan_results[scan_index].added = 1;
            g_model.version++;
            model_unlock();
            return ERRCODE_SUCC;
        }
    }

    copy_text(device.name, sizeof(device.name), g_model.scan_results[scan_index].name);
    copy_text(device.address, sizeof(device.address), g_model.scan_results[scan_index].address);
    device.rssi = g_model.scan_results[scan_index].rssi;
    device.connected = 0;
    device.online = 0;
    device.state = WATCH_DEVICE_IDLE;

    (void)memcpy_s(&g_model.devices[g_model.device_count], sizeof(g_model.devices[g_model.device_count]),
                   &device, sizeof(device));
    g_model.device_count++;
    g_model.scan_results[scan_index].added = 1;
    g_model.version++;
    append_device_state_log_locked(&device);
    model_unlock();
    return ERRCODE_SUCC;
}

void watch_model_add_log(watch_log_type_t type, const char *tag, const char *message)
{
    model_lock();
    append_log_locked(type, tag, message);
    model_unlock();
}
