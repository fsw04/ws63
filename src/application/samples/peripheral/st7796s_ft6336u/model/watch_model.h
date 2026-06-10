#ifndef WATCH_MODEL_H
#define WATCH_MODEL_H

#include <stdint.h>
#include "errcode.h"

#define WATCH_MODEL_MAX_DEVICES 5
#define WATCH_MODEL_MAX_SCAN_RESULTS 5
#define WATCH_MODEL_MAX_LOGS 12
#define WATCH_MODEL_NAME_LEN 24
#define WATCH_MODEL_SHORT_LEN 32
#define WATCH_MODEL_LONG_LEN 64
#define WATCH_MODEL_ID_LEN 20

typedef enum {
    WATCH_LINK_DISCONNECTED = 0,
    WATCH_LINK_CONNECTING,
    WATCH_LINK_CONNECTED,
    WATCH_LINK_BROADCASTING,
    WATCH_LINK_ERROR,
} watch_link_state_t;

typedef enum {
    WATCH_DEVICE_IDLE = 0,
    WATCH_DEVICE_BORROWED,
    WATCH_DEVICE_PENDING,
} watch_device_state_t;

typedef enum {
    WATCH_LOG_SLE = 0,
    WATCH_LOG_WIFI,
    WATCH_LOG_MQTT,
    WATCH_LOG_DEVICE,
    WATCH_LOG_WARNING,
} watch_log_type_t;

typedef struct {
    char name[WATCH_MODEL_NAME_LEN];
    char address[WATCH_MODEL_SHORT_LEN];
    char borrower[WATCH_MODEL_NAME_LEN];
    char borrower_id[WATCH_MODEL_ID_LEN];
    int8_t rssi;
    uint8_t connected;
    uint8_t online;
    watch_device_state_t state;
} watch_device_t;

typedef struct {
    char name[WATCH_MODEL_NAME_LEN];
    char address[WATCH_MODEL_SHORT_LEN];
    int8_t rssi;
    uint8_t added;
} watch_scan_device_t;

typedef struct {
    watch_log_type_t type;
    char tag[WATCH_MODEL_NAME_LEN];
    char time[WATCH_MODEL_SHORT_LEN];
    char message[WATCH_MODEL_LONG_LEN];
} watch_log_entry_t;

typedef struct {
    watch_link_state_t wifi_state;
    watch_link_state_t mqtt_state;
    watch_link_state_t sle_state;
    char wifi_ssid[WATCH_MODEL_NAME_LEN];
    char wifi_ip[WATCH_MODEL_SHORT_LEN];
    int8_t wifi_rssi;
    char mqtt_broker[WATCH_MODEL_SHORT_LEN];
    char mqtt_topic[WATCH_MODEL_SHORT_LEN];
    char mqtt_last_report[WATCH_MODEL_SHORT_LEN];
    char sle_service[WATCH_MODEL_SHORT_LEN];
    uint8_t sle_connected_count;
    uint8_t warning_count;
    uint8_t device_count;
    watch_device_t devices[WATCH_MODEL_MAX_DEVICES];
    uint8_t scan_active;
    uint8_t scan_count;
    watch_scan_device_t scan_results[WATCH_MODEL_MAX_SCAN_RESULTS];
    uint8_t log_count;
    watch_log_entry_t logs[WATCH_MODEL_MAX_LOGS];
    uint32_t version;
} watch_model_snapshot_t;

void watch_model_init(void);
void watch_model_get_snapshot(watch_model_snapshot_t *snapshot);
void watch_model_set_wifi(watch_link_state_t state, const char *ssid, const char *ip, int8_t rssi);
void watch_model_set_mqtt(watch_link_state_t state, const char *broker, const char *topic, const char *last_report);
void watch_model_set_sle(watch_link_state_t state, const char *service, uint8_t connected_count);
void watch_model_set_warning_count(uint8_t warning_count);
void watch_model_update_device(uint8_t index, const watch_device_t *device);
errcode_t watch_model_remove_device(uint8_t index);
void watch_model_clear_scan_results(void);
void watch_model_set_scan_active(uint8_t active);
errcode_t watch_model_add_scan_result(const watch_scan_device_t *device, uint8_t *scan_index);
errcode_t watch_model_add_device_from_scan(uint8_t scan_index);
void watch_model_add_log(watch_log_type_t type, const char *tag, const char *message);

#endif
