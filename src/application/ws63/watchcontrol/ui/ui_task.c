#include "ui_task.h"
#include "soc_osal.h"
#include "common_def.h"
#include "display_port.h"
#include "ui_style.h"
#include "ui_status_page.h"
#include "ui_devices_page.h"
#include "ui_wifi_page.h"
#include "ui_sle_page.h"

#define UI_TASK_STACK_SIZE    0x2000
#define UI_TASK_PRIORITY      20
#define UI_TASK_NAME          "UITask"

#define COLOR_BG              0x0A1E1A

#define LOG_MAX_ENTRIES       50

typedef enum {
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
} log_level_t;

typedef struct {
    char timestamp[16];
    log_level_t level;
    char type[16];
    char message[64];
} log_entry_t;

static lv_obj_t *g_tabview = NULL;
static lv_obj_t *g_log_list = NULL;
static log_entry_t g_log_entries[LOG_MAX_ENTRIES];
static int g_log_count = 0;

static const lv_color_t log_level_colors[] = {
    [LOG_LEVEL_INFO]  = {.red = 0x00, .green = 0xCC, .blue = 0x66},
    [LOG_LEVEL_WARN]  = {.red = 0xFF, .green = 0xCC, .blue = 0x00},
    [LOG_LEVEL_ERROR] = {.red = 0xFF, .green = 0x44, .blue = 0x44},
};

static const char * const log_level_labels[] = {
    [LOG_LEVEL_INFO]  = "INFO",
    [LOG_LEVEL_WARN]  = "WARN",
    [LOG_LEVEL_ERROR] = "ERR",
};

static void ui_add_log_entry(const char *type, log_level_t level, const char *message)
{
    if (g_log_list == NULL) {
        return;
    }

    if (g_log_count >= LOG_MAX_ENTRIES) {
        lv_obj_t *first_child = lv_obj_get_child(g_log_list, 0);
        if (first_child != NULL) {
            lv_obj_delete(first_child);
        }
        g_log_count = LOG_MAX_ENTRIES - 1;
    }

    lv_obj_t *row = lv_obj_create(g_log_list);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(row, 2, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);

    lv_obj_t *time_label = lv_label_create(row);
    lv_label_set_text_fmt(time_label, "%02d:%02d:%02d",
        (int)(osal_get_sys_time() / 3600000) % 24,
        (int)(osal_get_sys_time() / 60000) % 60,
        (int)(osal_get_sys_time() / 1000) % 60);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(time_label, lv_color_hex(0x888888), 0);

    lv_obj_t *level_label = lv_label_create(row);
    lv_label_set_text(level_label, log_level_labels[level]);
    lv_obj_set_style_text_font(level_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(level_label, log_level_colors[level], 0);
    lv_obj_set_style_pad_right(level_label, 4, 0);

    lv_obj_t *type_label = lv_label_create(row);
    lv_label_set_text(type_label, type);
    lv_obj_set_style_text_font(type_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(type_label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_pad_right(type_label, 4, 0);

    lv_obj_t *msg_label = lv_label_create(row);
    lv_label_set_text(msg_label, message);
    lv_obj_set_style_text_font(msg_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(msg_label, lv_color_hex(0xFFFFFF), 0);

    g_log_count++;
}

static void create_log_page(lv_obj_t *parent)
{
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(parent, 8, 0);
    lv_obj_set_style_bg_color(parent, lv_color_hex(COLOR_BG), 0);

    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "系统日志");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_pad_bottom(title, 6, 0);

    g_log_list = lv_obj_create(parent);
    lv_obj_set_flex_flow(g_log_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_grow(g_log_list, 1);
    lv_obj_set_size(g_log_list, lv_pct(100), lv_pct(90));
    lv_obj_set_style_pad_all(g_log_list, 4, 0);
    lv_obj_set_style_border_width(g_log_list, 0, 0);
    lv_obj_set_style_bg_opa(g_log_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_row(g_log_list, 2, 0);
    lv_obj_add_flag(g_log_list, LV_OBJ_FLAG_SCROLLABLE);

    ui_add_log_entry("SYS", LOG_LEVEL_INFO, "UI系统初始化完成");
}

static void *ui_main_task(const char *arg)
{
    unused(arg);

    display_port_init();

    ui_style_init();

    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(COLOR_BG), 0);

    g_tabview = lv_tabview_create(scr);
    lv_tabview_set_tab_bar_position(g_tabview, LV_DIR_BOTTOM);
    lv_tabview_set_tab_bar_size(g_tabview, 36);
    lv_obj_set_size(g_tabview, lv_pct(100), lv_pct(100));

    lv_obj_t *tab_status = lv_tabview_add_tab(g_tabview, "状态");
    lv_obj_t *tab_devices = lv_tabview_add_tab(g_tabview, "设备");
    lv_obj_t *tab_log = lv_tabview_add_tab(g_tabview, "日志");

    ui_status_page_create(tab_status);
    ui_devices_page_create(tab_devices);
    create_log_page(tab_log);

    ui_wifi_page_create(lv_layer_top());
    ui_sle_page_create(lv_layer_top());

    ui_add_log_entry("NET", LOG_LEVEL_INFO, "等待WiFi连接...");

    while (1) {
        lv_timer_handler();
        osal_msleep(5);
    }

    return NULL;
}

void ui_task_start(void)
{
    osal_task *task_handle = NULL;
    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)ui_main_task, 0,
                                      UI_TASK_NAME, UI_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, UI_TASK_PRIORITY);
    }
    osal_kthread_unlock();
}

void ui_update_system_status(const ui_system_status_t *status)
{
    ui_status_page_update(status);
}

void ui_update_devices(const ui_device_t *devices, int count)
{
    ui_devices_page_update(devices, count);
}

void ui_add_discovered_device(const ui_discovered_t *device)
{
    ui_sle_page_add_discovered(device);
}

void ui_set_scanning(bool scanning)
{
    ui_sle_page_set_scanning(scanning);
}
