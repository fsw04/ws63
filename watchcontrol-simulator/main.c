/**
 * @file main.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#ifndef _DEFAULT_SOURCE
  #define _DEFAULT_SOURCE
#endif

#include <stdlib.h>
#include <stdio.h>
#ifdef _MSC_VER
  #include <Windows.h>
#else
  #include <unistd.h>
  #include <pthread.h>
#endif
#include "lvgl/lvgl.h"
#include "lvgl/examples/lv_examples.h"
#include "lvgl/demos/lv_demos.h"
#include <SDL.h>

#include "hal/hal.h"

#include "ui_common.h"
#include "ui_style.h"
#include "ui_status_page.h"
#include "ui_devices_page.h"
#include "ui_wifi_page.h"
#include "ui_sle_page.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void sim_init_data(void);
static void open_wifi_page(void *user_data);
static void open_sle_page(void *user_data);
static void create_log_page(lv_obj_t *parent);

/**********************
 *  STATIC VARIABLES
 **********************/
static ui_device_t sim_devices[MAX_DEVICES];
static int sim_device_count = 0;
static ui_system_status_t sim_status;
static lv_obj_t *tabview;
static lv_obj_t *log_list;
static lv_obj_t *status_page;
static lv_obj_t *devices_page;
static lv_obj_t *wifi_overlay;
static lv_obj_t *sle_overlay;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

#if LV_USE_OS != LV_OS_FREERTOS

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;

  lv_init();

  sdl_hal_init(320, 480);

  ui_style_init();

  sim_init_data();

  lv_obj_t *scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, COLOR_BG, 0);

  tabview = lv_tabview_create(scr);
  lv_tabview_set_tab_bar_position(tabview, LV_DIR_BOTTOM);
  lv_tabview_set_tab_bar_size(tabview, 50);
  lv_obj_set_style_bg_color(tabview, COLOR_BG, 0);
  lv_obj_set_style_text_font(
      lv_tabview_get_tab_btns(tabview), &lv_font_montserrat_14, 0);

  lv_obj_t *tab_status = lv_tabview_add_tab(tabview, "状态");
  lv_obj_t *tab_devices = lv_tabview_add_tab(tabview, "设备");
  lv_obj_t *tab_logs = lv_tabview_add_tab(tabview, "日志");

  lv_obj_set_style_bg_color(tab_status, COLOR_BG, 0);
  lv_obj_set_style_bg_color(tab_devices, COLOR_BG, 0);
  lv_obj_set_style_bg_color(tab_logs, COLOR_BG, 0);

  status_page = ui_status_page_create(tab_status);
  ui_status_page_update(&sim_status, sim_devices, sim_device_count);

  devices_page = ui_devices_page_create(tab_devices);
  ui_devices_page_update(sim_devices, sim_device_count);

  create_log_page(tab_logs);

  wifi_overlay = ui_wifi_page_create(lv_layer_top());
  sle_overlay = ui_sle_page_create(lv_layer_top());

  ui_set_wifi_page_cb(open_wifi_page);
  ui_set_sle_page_cb(open_sle_page);
  ui_devices_set_add_cb(open_sle_page);

  while(1) {
    uint32_t sleep_time_ms = lv_timer_handler();
    if(sleep_time_ms == LV_NO_TIMER_READY) {
      sleep_time_ms = LV_DEF_REFR_PERIOD;
    }
#ifdef _MSC_VER
    Sleep(sleep_time_ms);
#else
    usleep(sleep_time_ms * 1000);
#endif
  }

  return 0;
}

#endif

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void sim_init_data(void)
{
    sim_status.wifi_connected = true;
    snprintf(sim_status.wifi_ssid, sizeof(sim_status.wifi_ssid), "FSW");
    snprintf(sim_status.wifi_ip, sizeof(sim_status.wifi_ip), "192.168.43.110");
    sim_status.wifi_signal = -42;
    sim_status.mqtt_connected = true;
    snprintf(sim_status.mqtt_broker, sizeof(sim_status.mqtt_broker), "192.168.43.110:1883");
    sim_status.sle_broadcasting = true;
    sim_status.sle_connected_count = 3;
    snprintf(sim_status.sle_server_name, sizeof(sim_status.sle_server_name), "sle_speed_server");

    sim_device_count = 4;

    snprintf(sim_devices[0].device_id, 32, "watch_5200075C6713");
    snprintf(sim_devices[0].mac, 18, "52:00:07:5C:67:13");
    snprintf(sim_devices[0].name, 16, "手表-01");
    sim_devices[0].connected = true;
    sim_devices[0].status = DEVICE_STATUS_IDLE;

    snprintf(sim_devices[1].device_id, 32, "watch_A1B2C3D4E5F6");
    snprintf(sim_devices[1].mac, 18, "A1:B2:C3:D4:E5:F6");
    snprintf(sim_devices[1].name, 16, "手表-02");
    sim_devices[1].connected = true;
    sim_devices[1].status = DEVICE_STATUS_BORROWED;
    snprintf(sim_devices[1].borrowed_by, 16, "李桂芳");

    snprintf(sim_devices[2].device_id, 32, "watch_7A8B9C0D1E2F");
    snprintf(sim_devices[2].mac, 18, "7A:8B:9C:0D:1E:2F");
    snprintf(sim_devices[2].name, 16, "手表-03");
    sim_devices[2].connected = false;
    sim_devices[2].status = DEVICE_STATUS_IDLE;

    snprintf(sim_devices[3].device_id, 32, "watch_D4E5F6A7B8C9");
    snprintf(sim_devices[3].mac, 18, "D4:E5:F6:A7:B8:C9");
    snprintf(sim_devices[3].name, 16, "手表-04");
    sim_devices[3].connected = true;
    sim_devices[3].status = DEVICE_STATUS_REQUESTED;
    snprintf(sim_devices[3].borrowed_by, 16, "张明远");
}

static void open_wifi_page(void *user_data)
{
    LV_UNUSED(user_data);
    ui_wifi_page_open();
}

static void open_sle_page(void *user_data)
{
    LV_UNUSED(user_data);
    ui_sle_page_open();

    ui_discovered_t discovered[] = {
        {"52:00:07:5C:67:13", "手表-01", -38},
        {"A1:B2:C3:D4:E5:F6", "手表-02", -55},
        {"F1:E2:D3:C4:B5:A6", "手表-05", -72},
    };

    for (int i = 0; i < 3; i++) {
        ui_sle_page_add_discovered(&discovered[i]);
    }
    ui_sle_page_set_scanning(false);
}

static void create_log_page(lv_obj_t *parent)
{
    lv_obj_t *page = lv_obj_create(parent);
    lv_obj_set_size(page, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(page, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(page, 6, 0);
    lv_obj_set_style_pad_row(page, 4, 0);
    lv_obj_set_style_bg_color(page, COLOR_BG, 0);
    lv_obj_set_style_border_width(page, 0, 0);
    lv_obj_set_scroll_dir(page, LV_DIR_VER);

    lv_obj_t *title = lv_label_create(page);
    lv_label_set_text(title, "系统日志");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, COLOR_TEXT_PRIMARY, 0);

    log_list = lv_obj_create(page);
    lv_obj_set_flex_flow(log_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(log_list, 3, 0);
    lv_obj_set_style_border_width(log_list, 0, 0);
    lv_obj_set_style_bg_opa(log_list, 0, 0);
    lv_obj_set_style_pad_all(log_list, 0, 0);
    lv_obj_set_flex_grow(log_list, 1);
    lv_obj_set_width(log_list, LV_PCT(100));
    lv_obj_set_scroll_dir(log_list, LV_DIR_VER);

    const char *logs[] = {
        "[WiFi] 连接成功, IP: 192.168.43.110",
        "[MQTT] 连接成功",
        "[SLE]  广播已开启",
        "[SLE]  设备 手表-01 已连接",
        "[SLE]  设备 手表-02 已连接",
        "[设备] 李桂芳 申请了 手表-02",
        "[SLE]  设备 手表-03 连接超时",
        "[设备] 张明远 申请了 手表-04",
        "[告警] 设备 手表-03 离线",
        "[MQTT] 订阅 watch/commands 成功",
    };

    lv_color_t type_colors[] = {
        COLOR_CYAN, COLOR_GREEN, COLOR_YELLOW,
        COLOR_GREEN, COLOR_GREEN,
        COLOR_CYAN,
        COLOR_YELLOW,
        COLOR_CYAN,
        COLOR_RED,
        COLOR_GREEN,
    };

    for (int i = 0; i < 10; i++) {
        lv_obj_t *row = lv_obj_create(log_list);
        lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(row, 6, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_bg_opa(row, 0, 0);
        lv_obj_set_style_pad_all(row, 2, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *dot = lv_obj_create(row);
        lv_obj_set_size(dot, 6, 6);
        lv_obj_set_style_bg_color(dot, type_colors[i], 0);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_set_style_pad_all(dot, 0, 0);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *msg = lv_label_create(row);
        lv_label_set_text(msg, logs[i]);
        lv_obj_set_style_text_font(msg, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(msg, COLOR_TEXT_SECONDARY, 0);
    }
}
