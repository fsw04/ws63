#ifndef UI_COMMON_H
#define UI_COMMON_H

#include "lvgl.h"

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 480

#define COLOR_BG            lv_color_hex(0x0A0E1A)
#define COLOR_CARD          lv_color_hex(0x111827)
#define COLOR_CARD_BORDER   lv_color_hex(0x1F2937)
#define COLOR_GREEN         lv_color_hex(0x00E5A0)
#define COLOR_RED           lv_color_hex(0xFF4D6A)
#define COLOR_YELLOW        lv_color_hex(0xFFB020)
#define COLOR_CYAN          lv_color_hex(0x00D4FF)
#define COLOR_TEXT_PRIMARY   lv_color_hex(0xE5E7EB)
#define COLOR_TEXT_SECONDARY lv_color_hex(0x6B7280)
#define COLOR_TEXT_DIM       lv_color_hex(0x4B5563)
#define COLOR_TAB_BAR       lv_color_hex(0x0F1629)

typedef enum {
    DEVICE_STATUS_IDLE = 0,
    DEVICE_STATUS_BORROWED,
    DEVICE_STATUS_REQUESTED,
    DEVICE_STATUS_RETURNING,
} device_status_t;

typedef struct {
    char device_id[32];
    char mac[18];
    char name[16];
    bool connected;
    device_status_t status;
    char borrowed_by[16];
} ui_device_t;

typedef struct {
    bool wifi_connected;
    char wifi_ssid[32];
    char wifi_ip[16];
    int wifi_signal;
    bool mqtt_connected;
    char mqtt_broker[32];
    bool sle_broadcasting;
    int sle_connected_count;
    char sle_server_name[32];
} ui_system_status_t;

typedef void (*ui_event_cb_t)(void *user_data);

typedef enum {
    PAGE_STATUS = 0,
    PAGE_DEVICES,
    PAGE_LOGS,
    PAGE_COUNT,
} ui_page_t;

#define MAX_DEVICES 8
#define MAX_DISCOVERED 8

typedef struct {
    char mac[18];
    char name[16];
    int rssi;
} ui_discovered_t;

void ui_set_active_page(ui_page_t page);
ui_page_t ui_get_active_page(void);

#endif
