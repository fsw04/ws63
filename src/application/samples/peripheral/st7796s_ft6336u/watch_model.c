#include "watch_model.h"

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
    watch_model_seed_demo_devices();
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
    model_lock();
    g_model.wifi_state = state;
    copy_text(g_model.wifi_ssid, sizeof(g_model.wifi_ssid), ssid);
    copy_text(g_model.wifi_ip, sizeof(g_model.wifi_ip), ip);
    g_model.wifi_rssi = rssi;
    g_model.version++;
    model_unlock();
}

void watch_model_set_mqtt(watch_link_state_t state, const char *broker, const char *topic, const char *last_report)
{
    model_lock();
    g_model.mqtt_state = state;
    copy_text(g_model.mqtt_broker, sizeof(g_model.mqtt_broker), broker);
    copy_text(g_model.mqtt_topic, sizeof(g_model.mqtt_topic), topic);
    copy_text(g_model.mqtt_last_report, sizeof(g_model.mqtt_last_report), last_report);
    g_model.version++;
    model_unlock();
}

void watch_model_set_sle(watch_link_state_t state, const char *service, uint8_t connected_count)
{
    model_lock();
    g_model.sle_state = state;
    copy_text(g_model.sle_service, sizeof(g_model.sle_service), service);
    g_model.sle_connected_count = connected_count;
    g_model.version++;
    model_unlock();
}

void watch_model_set_warning_count(uint8_t warning_count)
{
    model_lock();
    g_model.warning_count = warning_count;
    g_model.version++;
    model_unlock();
}

void watch_model_seed_demo_devices(void)
{
    static const watch_device_t demo[] = {
        {"手表-01", "7A:8B:9C:0D:1E:2F", "", -42, 1, 1, WATCH_DEVICE_IDLE},
        {"手表-02", "8C:20:4F:8E:1A:7D", "用:李桂芳", -48, 1, 1, WATCH_DEVICE_BORROWED},
        {"手表-03", "6B:20:4F:8E:1A:7D", "", -80, 0, 0, WATCH_DEVICE_IDLE},
        {"手表-04", "9D:52:01:8E:1A:7D", "用:李桂芳", -50, 1, 1, WATCH_DEVICE_PENDING},
    };

    model_lock();
    g_model.device_count = (uint8_t)(sizeof(demo) / sizeof(demo[0]));
    (void)memcpy_s(g_model.devices, sizeof(g_model.devices), demo, sizeof(demo));
    g_model.sle_connected_count = 3;
    g_model.warning_count = 1;
    g_model.version++;
    model_unlock();
}

void watch_model_update_device(uint8_t index, const watch_device_t *device)
{
    if ((index >= WATCH_MODEL_MAX_DEVICES) || (device == NULL)) {
        return;
    }

    model_lock();
    (void)memcpy_s(&g_model.devices[index], sizeof(g_model.devices[index]), device, sizeof(*device));
    if (index >= g_model.device_count) {
        g_model.device_count = index + 1;
    }
    g_model.version++;
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

void watch_model_add_log(watch_log_type_t type, const char *tag, const char *message)
{
    watch_log_entry_t entry = {0};

    entry.type = type;
    copy_text(entry.tag, sizeof(entry.tag), tag);
    copy_text(entry.message, sizeof(entry.message), message);
    make_time(entry.time, sizeof(entry.time));

    model_lock();
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
    model_unlock();
}
