#include "watch_ui.h"

#include <stdbool.h>
#include <stdio.h>
#include "../services/sle_watch_server.h"
#include "securec.h"
#include "../business/watch_borrow.h"
#include "../model/watch_model.h"
#include "../services/wifi_provision.h"
#include "../services/wifi_task.h"

#define UI_W 320
#define UI_H 480
#define UI_HEADER_H 54
#define UI_NAV_H 62
#define UI_CONTENT_Y 54
#define UI_CONTENT_H (UI_H - UI_HEADER_H - UI_NAV_H)
#define UI_PAD 10
#define UI_CARD_RADIUS 12
#define UI_REFRESH_MS 1000
#define UI_DEVICE_ROW_W 304
#define UI_DEVICE_ROW_H 78
#define UI_DEVICE_ACTION_OPEN_X (-58)

#define C_BG 0x070B15
#define C_PANEL 0x111827
#define C_PANEL_2 0x171D31
#define C_PANEL_3 0x0B3A3D
#define C_TEXT 0xD8DEE9
#define C_MUTED 0x7C8699
#define C_ACCENT 0x00E1AF
#define C_YELLOW 0xFFE45C
#define C_RED 0xFF646A
#define C_BLUE 0x49D6FF

typedef enum {
    PAGE_STATUS = 0,
    PAGE_DEVICE,
    PAGE_LOG,
    PAGE_COUNT,
} watch_page_t;

typedef enum {
    WIFI_UI_CONFIG = 0,
    WIFI_UI_CONNECT,
} wifi_ui_action_t;

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *content;
    lv_obj_t *pages[PAGE_COUNT];
    lv_obj_t *nav_btns[PAGE_COUNT];
    lv_obj_t *nav_labels[PAGE_COUNT];
    lv_obj_t *nav_icons[PAGE_COUNT];
    lv_obj_t *nav_badge;
    lv_obj_t *nav_badge_label;
    lv_obj_t *run_dot;
    lv_obj_t *run_label;
    lv_obj_t *modal;
    lv_obj_t *wifi_ssid_ta;
    lv_obj_t *wifi_pwd_ta;
    lv_obj_t *keyboard;
    lv_obj_t *device_swipe_card;
    lv_obj_t *scan_list;
    lv_obj_t *scan_count_label;
    uint8_t scan_modal_open;
    watch_page_t page;
    uint8_t pending_delete_index;
    char pending_delete_name[WATCH_MODEL_NAME_LEN];
    uint32_t model_version;
} watch_ui_t;

static watch_ui_t g_ui;
static lv_style_t g_style_screen;
static lv_style_t g_style_header;
static lv_style_t g_style_card;
static lv_style_t g_style_card_hot;
static lv_style_t g_style_nav;
static lv_style_t g_style_btn_icon;
static lv_style_t g_style_pill;
static lv_style_t g_style_text;
static lv_style_t g_style_muted;
static lv_style_t g_style_title;
static lv_style_t g_style_accent;
static lv_style_t g_style_yellow;
static lv_style_t g_style_red;
static lv_style_t g_style_muted_sm;
static lv_style_t g_style_accent_sm;
static lv_style_t g_style_red_sm;
static lv_style_t g_style_modal;

static void render_status_page(const watch_model_snapshot_t *model);
static void render_device_page(const watch_model_snapshot_t *model);
static void render_log_page(const watch_model_snapshot_t *model);
static void render_all(const watch_model_snapshot_t *model);
static lv_obj_t *modal_base(const char *title, const char *symbol);
static void close_keyboard(void);
static void close_wifi_modal(void);
static void wifi_action_event_cb(lv_event_t *e);
static void wifi_ssid_event_cb(lv_event_t *e);
static void wifi_pwd_event_cb(lv_event_t *e);
static void wifi_keyboard_event_cb(lv_event_t *e);
static void wifi_submit_event_cb(lv_event_t *e);
static void wifi_disconnect_event_cb(lv_event_t *e);
static void wifi_manual_config_event_cb(lv_event_t *e);
static void wifi_provision_stop_event_cb(lv_event_t *e);
static void open_wifi_provision_modal(void);
static void open_wifi_modal(bool open_keyboard);
static void show_wifi_modal(lv_event_t *e);
static void refresh_scan_modal(const watch_model_snapshot_t *model);
static void device_action_event_cb(lv_event_t *e);

static lv_color_t color(uint32_t value)
{
    return lv_color_hex(value);
}

static void style_base(lv_style_t *style, uint32_t bg, uint32_t text)
{
    lv_style_init(style);
    lv_style_set_bg_color(style, color(bg));
    lv_style_set_bg_opa(style, LV_OPA_COVER);
    lv_style_set_text_color(style, color(text));
    lv_style_set_border_width(style, 0);
}

static void watch_ui_styles_init(void)
{
    style_base(&g_style_screen, C_BG, C_TEXT);

    style_base(&g_style_header, C_PANEL, C_TEXT);
    lv_style_set_border_width(&g_style_header, 1);
    lv_style_set_border_color(&g_style_header, color(0x1B2335));
    lv_style_set_pad_all(&g_style_header, 0);

    style_base(&g_style_card, C_PANEL, C_TEXT);
    lv_style_set_radius(&g_style_card, UI_CARD_RADIUS);
    lv_style_set_border_width(&g_style_card, 1);
    lv_style_set_border_color(&g_style_card, color(0x1A2337));
    lv_style_set_pad_all(&g_style_card, 12);

    style_base(&g_style_card_hot, C_PANEL, C_TEXT);
    lv_style_set_radius(&g_style_card_hot, UI_CARD_RADIUS);
    lv_style_set_border_width(&g_style_card_hot, 1);
    lv_style_set_border_color(&g_style_card_hot, color(0x0B665B));
    lv_style_set_pad_all(&g_style_card_hot, 12);

    style_base(&g_style_nav, C_PANEL, C_TEXT);
    lv_style_set_border_width(&g_style_nav, 1);
    lv_style_set_border_color(&g_style_nav, color(0x1B2335));
    lv_style_set_pad_all(&g_style_nav, 0);

    style_base(&g_style_btn_icon, C_PANEL_3, C_ACCENT);
    lv_style_set_radius(&g_style_btn_icon, 10);
    lv_style_set_pad_all(&g_style_btn_icon, 0);

    style_base(&g_style_pill, C_PANEL_3, C_ACCENT);
    lv_style_set_radius(&g_style_pill, LV_RADIUS_CIRCLE);
    lv_style_set_pad_hor(&g_style_pill, 8);
    lv_style_set_pad_ver(&g_style_pill, 3);

    lv_style_init(&g_style_text);
    lv_style_set_text_color(&g_style_text, color(C_TEXT));
    lv_style_set_text_font(&g_style_text, &lv_font_source_han_sans_sc_16_cjk);

    lv_style_init(&g_style_muted);
    lv_style_set_text_color(&g_style_muted, color(C_MUTED));
    lv_style_set_text_font(&g_style_muted, &lv_font_source_han_sans_sc_16_cjk);

    lv_style_init(&g_style_title);
    lv_style_set_text_color(&g_style_title, color(C_TEXT));
    lv_style_set_text_font(&g_style_title, &lv_font_source_han_sans_sc_16_cjk);

    lv_style_init(&g_style_accent);
    lv_style_set_text_color(&g_style_accent, color(C_ACCENT));
    lv_style_set_text_font(&g_style_accent, &lv_font_source_han_sans_sc_16_cjk);

    lv_style_init(&g_style_yellow);
    lv_style_set_text_color(&g_style_yellow, color(C_YELLOW));
    lv_style_set_text_font(&g_style_yellow, &lv_font_source_han_sans_sc_16_cjk);

    lv_style_init(&g_style_red);
    lv_style_set_text_color(&g_style_red, color(C_RED));
    lv_style_set_text_font(&g_style_red, &lv_font_source_han_sans_sc_16_cjk);

    lv_style_init(&g_style_muted_sm);
    lv_style_set_text_color(&g_style_muted_sm, color(C_MUTED));
    lv_style_set_text_font(&g_style_muted_sm, &lv_font_source_han_sans_sc_16_cjk);

    lv_style_init(&g_style_accent_sm);
    lv_style_set_text_color(&g_style_accent_sm, color(C_ACCENT));
    lv_style_set_text_font(&g_style_accent_sm, &lv_font_source_han_sans_sc_16_cjk);

    lv_style_init(&g_style_red_sm);
    lv_style_set_text_color(&g_style_red_sm, color(C_RED));
    lv_style_set_text_font(&g_style_red_sm, &lv_font_source_han_sans_sc_16_cjk);

    style_base(&g_style_modal, C_PANEL, C_TEXT);
    lv_style_set_radius(&g_style_modal, 16);
    lv_style_set_pad_all(&g_style_modal, 16);
}

static lv_obj_t *plain_obj(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_add_style(obj, &g_style_screen, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static lv_obj_t *label(lv_obj_t *parent, const char *text, const lv_style_t *style)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_label_set_text(obj, text);
    lv_obj_add_style(obj, style, 0);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_MODE_DOTS);
    return obj;
}

static lv_obj_t *symbol_label(lv_obj_t *parent, const char *text, uint32_t c)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_label_set_text(obj, text);
    lv_obj_set_style_text_color(obj, color(c), 0);
    lv_obj_set_style_text_font(obj, &lv_font_source_han_sans_sc_16_cjk, 0);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_MODE_CLIP);
    return obj;
}

static lv_obj_t *mono_label(lv_obj_t *parent, const char *text, uint32_t c)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_label_set_text(obj, text);
    lv_obj_set_style_text_color(obj, color(c), 0);
    lv_obj_set_style_text_font(obj, &lv_font_source_han_sans_sc_16_cjk, 0);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_MODE_DOTS);
    return obj;
}

static lv_obj_t *mono_title(lv_obj_t *parent, const char *text, uint32_t c)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_label_set_text(obj, text);
    lv_obj_set_style_text_color(obj, color(c), 0);
    lv_obj_set_style_text_font(obj, &lv_font_source_han_sans_sc_16_cjk, 0);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_MODE_DOTS);
    return obj;
}

static lv_obj_t *card(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h, bool hot)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_add_style(obj, hot ? &g_style_card_hot : &g_style_card, 0);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static lv_obj_t *dot(lv_obj_t *parent, int32_t x, int32_t y, uint32_t c)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, 10, 10);
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(obj, color(c), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static lv_obj_t *bar(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t c, int32_t radius)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_bg_color(obj, color(c), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static lv_obj_t *icon_box(lv_obj_t *parent, int32_t x, int32_t y, const char *symbol, uint32_t bg, uint32_t fg)
{
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_set_pos(box, x, y);
    lv_obj_set_size(box, 36, 36);
    lv_obj_set_style_radius(box, 10, 0);
    lv_obj_set_style_bg_color(box, color(bg), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *txt = symbol_label(box, symbol, fg);
    lv_obj_center(txt);
    return box;
}

static const char *link_text(watch_link_state_t state)
{
    switch (state) {
        case WATCH_LINK_CONNECTED:
            return "已连接";
        case WATCH_LINK_CONNECTING:
            return "连接中";
        case WATCH_LINK_BROADCASTING:
            return "广播中";
        case WATCH_LINK_ERROR:
            return "失效";
        default:
            return "未连接";
    }
}

static uint32_t link_color(watch_link_state_t state)
{
    switch (state) {
        case WATCH_LINK_CONNECTED:
        case WATCH_LINK_BROADCASTING:
            return C_ACCENT;
        case WATCH_LINK_CONNECTING:
            return C_BLUE;
        case WATCH_LINK_ERROR:
            return C_RED;
        default:
            return C_RED;
    }
}

static const lv_style_t *link_style_sm(watch_link_state_t state)
{
    switch (state) {
        case WATCH_LINK_CONNECTED:
        case WATCH_LINK_BROADCASTING:
            return &g_style_accent_sm;
        case WATCH_LINK_CONNECTING:
            return &g_style_muted_sm;
        case WATCH_LINK_ERROR:
        case WATCH_LINK_DISCONNECTED:
        default:
            return &g_style_red_sm;
    }
}

static uint32_t link_icon_bg(watch_link_state_t state)
{
    switch (state) {
        case WATCH_LINK_CONNECTED:
        case WATCH_LINK_BROADCASTING:
            return C_PANEL_3;
        case WATCH_LINK_CONNECTING:
            return 0x163144;
        case WATCH_LINK_ERROR:
        case WATCH_LINK_DISCONNECTED:
        default:
            return 0x3B1F30;
    }
}

static void show_page(watch_page_t page)
{
    static const char *names[PAGE_COUNT] = {"WiFi", "device", "log"};
    static const char *icons[PAGE_COUNT] = {LV_SYMBOL_WIFI, LV_SYMBOL_HOME, LV_SYMBOL_BELL};
    watch_model_snapshot_t model;

    if (page == g_ui.page) {
        return;
    }

    g_ui.page = page;
    for (uint8_t i = 0; i < PAGE_COUNT; i++) {
        if (g_ui.pages[i] != NULL) {
            if (i == (uint8_t)page) {
                lv_obj_remove_flag(g_ui.pages[i], LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_style_text_color(g_ui.nav_labels[i], color(C_ACCENT), 0);
                lv_obj_set_style_text_color(g_ui.nav_icons[i], color(C_ACCENT), 0);
                lv_obj_set_style_bg_color(g_ui.nav_btns[i], color(0x0C2F35), 0);
            } else {
                lv_obj_clean(g_ui.pages[i]);
                lv_obj_add_flag(g_ui.pages[i], LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_style_text_color(g_ui.nav_labels[i], color(C_MUTED), 0);
                lv_obj_set_style_text_color(g_ui.nav_icons[i], color(C_MUTED), 0);
                lv_obj_set_style_bg_color(g_ui.nav_btns[i], color(C_PANEL), 0);
            }
            lv_label_set_text(g_ui.nav_labels[i], names[i]);
            lv_label_set_text(g_ui.nav_icons[i], icons[i]);
        }
    }

    watch_model_get_snapshot(&model);
    switch (page) {
        case PAGE_STATUS:
            render_status_page(&model);
            break;
        case PAGE_DEVICE:
            render_device_page(&model);
            break;
        case PAGE_LOG:
        default:
            render_log_page(&model);
            break;
    }
}

static void nav_event_cb(lv_event_t *e)
{
    uintptr_t index = (uintptr_t)lv_event_get_user_data(e);
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        show_page((watch_page_t)index);
    }
}

static void close_modal(void)
{
    if (g_ui.scan_modal_open != 0) {
        (void)sle_watch_scan_stop();
        g_ui.scan_modal_open = 0;
        g_ui.scan_list = NULL;
        g_ui.scan_count_label = NULL;
    }
    close_keyboard();
    if (g_ui.modal != NULL) {
        lv_obj_delete(g_ui.modal);
        g_ui.modal = NULL;
    }
    g_ui.wifi_ssid_ta = NULL;
    g_ui.wifi_pwd_ta = NULL;
}

static void close_keyboard(void)
{
    if (g_ui.keyboard != NULL) {
        lv_obj_delete(g_ui.keyboard);
        g_ui.keyboard = NULL;
    }
}

static void close_wifi_modal(void)
{
    close_modal();
}

static void close_modal_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        close_modal();
    }
}

static void anim_card_x(lv_obj_t *obj, int32_t target_x)
{
    lv_anim_t anim;

    lv_anim_init(&anim);
    lv_anim_set_var(&anim, obj);
    lv_anim_set_values(&anim, lv_obj_get_x(obj), target_x);
    lv_anim_set_duration(&anim, 140);
    lv_anim_set_exec_cb(&anim, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    (void)lv_anim_start(&anim);
}

static void close_device_swipe(void)
{
    if (g_ui.device_swipe_card != NULL) {
        anim_card_x(g_ui.device_swipe_card, 0);
        g_ui.device_swipe_card = NULL;
    }
}

static void confirm_delete_event_cb(lv_event_t *e)
{
    watch_model_snapshot_t model;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    if (watch_model_remove_device(g_ui.pending_delete_index) == ERRCODE_SUCC) {
        watch_model_add_log(WATCH_LOG_DEVICE, "DEV", "Device removed");
        watch_ui_request_refresh();
        watch_model_get_snapshot(&model);
        g_ui.model_version = model.version;
        render_all(&model);
    }
    close_modal();
}

static void show_delete_confirm_modal(uint8_t index, const char *name)
{
    g_ui.pending_delete_index = index;
    if (strncpy_s(g_ui.pending_delete_name, sizeof(g_ui.pending_delete_name), name,
                  sizeof(g_ui.pending_delete_name) - 1) != EOK) {
        g_ui.pending_delete_name[sizeof(g_ui.pending_delete_name) - 1] = '\0';
    }

    lv_obj_t *modal = modal_base("移除设备", LV_SYMBOL_TRASH);
    lv_obj_set_pos(modal, 10, 112);
    lv_obj_set_size(modal, UI_W - 20, 196);

    lv_obj_t *hint = label(modal, "移除手表设备?", &g_style_text);
    lv_obj_set_pos(hint, 0, 58);
    lv_obj_set_size(hint, 240, 24);

    lv_obj_t *dev_name = label(modal, g_ui.pending_delete_name, &g_style_accent);
    lv_obj_set_pos(dev_name, 0, 86);
    lv_obj_set_size(dev_name, 220, 24);

    lv_obj_t *cancel_btn = lv_button_create(modal);
    lv_obj_remove_style_all(cancel_btn);
    lv_obj_set_pos(cancel_btn, 0, 126);
    lv_obj_set_size(cancel_btn, 108, 42);
    lv_obj_set_style_radius(cancel_btn, 10, 0);
    lv_obj_set_style_bg_color(cancel_btn, color(0x20263A), 0);
    lv_obj_set_style_bg_opa(cancel_btn, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(cancel_btn, close_modal_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *cancel_label = label(cancel_btn, "取消", &g_style_muted);
    lv_obj_center(cancel_label);

    lv_obj_t *delete_btn = lv_button_create(modal);
    lv_obj_remove_style_all(delete_btn);
    lv_obj_set_pos(delete_btn, 132, 126);
    lv_obj_set_size(delete_btn, 108, 42);
    lv_obj_set_style_radius(delete_btn, 10, 0);
    lv_obj_set_style_bg_color(delete_btn, color(0x4A1F2C), 0);
    lv_obj_set_style_bg_opa(delete_btn, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(delete_btn, confirm_delete_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *delete_label = label(delete_btn, "移除", &g_style_red);
    lv_obj_center(delete_label);
}

static void device_delete_event_cb(lv_event_t *e)
{
    uintptr_t index = (uintptr_t)lv_event_get_user_data(e);
    watch_model_snapshot_t model;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    watch_model_get_snapshot(&model);
    if (index < model.device_count) {
        show_delete_confirm_modal((uint8_t)index, model.devices[index].name);
    }
}

static void device_card_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *card_obj = lv_event_get_current_target_obj(e);
    lv_indev_t *indev = lv_indev_active();

    if (code == LV_EVENT_GESTURE) {
        lv_dir_t dir = indev == NULL ? LV_DIR_NONE : lv_indev_get_gesture_dir(indev);
        if (dir == LV_DIR_LEFT) {
            if ((g_ui.device_swipe_card != NULL) && (g_ui.device_swipe_card != card_obj)) {
                close_device_swipe();
            }
            g_ui.device_swipe_card = card_obj;
            anim_card_x(card_obj, UI_DEVICE_ACTION_OPEN_X);
        } else if (dir == LV_DIR_RIGHT) {
            if (g_ui.device_swipe_card == card_obj) {
                close_device_swipe();
            } else {
                anim_card_x(card_obj, 0);
            }
        }
    }
}

static lv_obj_t *modal_base(const char *title, const char *symbol)
{
    close_modal();

    g_ui.modal = lv_obj_create(g_ui.screen);
    lv_obj_remove_style_all(g_ui.modal);
    lv_obj_add_style(g_ui.modal, &g_style_modal, 0);
    lv_obj_set_pos(g_ui.modal, 10, 30);
    lv_obj_set_size(g_ui.modal, UI_W - 20, UI_H - 82);
    lv_obj_move_foreground(g_ui.modal);
    lv_obj_clear_flag(g_ui.modal, LV_OBJ_FLAG_SCROLLABLE);

    icon_box(g_ui.modal, 0, 0, symbol, C_PANEL_3, C_ACCENT);
    lv_obj_t *title_label = label(g_ui.modal, title, &g_style_title);
    lv_obj_set_pos(title_label, 48, 7);
    lv_obj_set_size(title_label, 200, 28);

    lv_obj_t *close_btn = lv_button_create(g_ui.modal);
    lv_obj_remove_style_all(close_btn);
    lv_obj_set_pos(close_btn, 242, 0);
    lv_obj_set_size(close_btn, 36, 36);
    lv_obj_set_style_radius(close_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(close_btn, color(0x20263A), 0);
    lv_obj_set_style_bg_opa(close_btn, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(close_btn, close_modal_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *close_label = symbol_label(close_btn, LV_SYMBOL_CLOSE, C_MUTED);
    lv_obj_center(close_label);
    return g_ui.modal;
}

static lv_obj_t *text_button(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h,
                             const char *text, uint32_t bg, const lv_style_t *text_style,
                             lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_bg_color(btn, color(bg), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    if (cb != NULL) {
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    }
    lv_obj_t *btn_label = label(btn, text, text_style);
    lv_obj_center(btn_label);
    return btn;
}

static lv_obj_t *wifi_textarea(lv_obj_t *parent, int32_t x, int32_t y, const char *placeholder, uint32_t max_len,
                               bool password)
{
    lv_obj_t *ta = lv_textarea_create(parent);
    lv_obj_remove_style_all(ta);
    lv_obj_add_flag(ta, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_set_pos(ta, x, y);
    lv_obj_set_size(ta, 280, 42);
    lv_obj_set_style_radius(ta, 10, 0);
    lv_obj_set_style_bg_color(ta, color(C_PANEL_2), 0);
    lv_obj_set_style_bg_opa(ta, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ta, 1, 0);
    lv_obj_set_style_border_color(ta, color(0x25314A), 0);
    lv_obj_set_style_text_color(ta, color(C_TEXT), 0);
    lv_obj_set_style_text_color(ta, color(C_MUTED), LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_text_font(ta, &lv_font_source_han_sans_sc_16_cjk, 0);
    lv_obj_set_style_text_font(ta, &lv_font_source_han_sans_sc_16_cjk, LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_pad_left(ta, 10, 0);
    lv_obj_set_style_pad_right(ta, 10, 0);
    lv_obj_set_style_pad_top(ta, 10, 0);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_max_length(ta, max_len);
    lv_textarea_set_placeholder_text(ta, placeholder);
    lv_textarea_set_password_mode(ta, password);
    lv_obj_set_scrollbar_mode(ta, LV_SCROLLBAR_MODE_OFF);
    return ta;
}

static void wifi_show_keyboard(lv_obj_t *ta)
{
    if (ta == NULL) {
        return;
    }

    if (g_ui.keyboard == NULL) {
        g_ui.keyboard = lv_keyboard_create(g_ui.screen);
        lv_obj_set_pos(g_ui.keyboard, 0, 314);
        lv_obj_set_size(g_ui.keyboard, UI_W, UI_H - 314);
        lv_obj_set_style_bg_color(g_ui.keyboard, color(C_PANEL), 0);
        lv_obj_set_style_bg_opa(g_ui.keyboard, LV_OPA_COVER, 0);
        lv_obj_set_style_text_font(g_ui.keyboard, &lv_font_source_han_sans_sc_16_cjk, 0);
        lv_keyboard_set_mode(g_ui.keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
        lv_keyboard_set_popovers(g_ui.keyboard, false);
        lv_obj_add_event_cb(g_ui.keyboard, wifi_keyboard_event_cb, LV_EVENT_READY, NULL);
        lv_obj_add_event_cb(g_ui.keyboard, wifi_keyboard_event_cb, LV_EVENT_CANCEL, NULL);
    }
    lv_keyboard_set_textarea(g_ui.keyboard, ta);
    lv_textarea_set_cursor_pos(ta, LV_TEXTAREA_CURSOR_LAST);
    lv_obj_add_state(ta, LV_STATE_FOCUSED);
    lv_obj_move_foreground(ta);
    lv_obj_clear_flag(g_ui.keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(g_ui.keyboard);
}

static void wifi_ssid_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if ((code == LV_EVENT_PRESSED) || (code == LV_EVENT_FOCUSED) || (code == LV_EVENT_CLICKED)) {
        lv_event_stop_bubbling(e);
        wifi_show_keyboard(lv_event_get_current_target_obj(e));
    }
}

static void wifi_pwd_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if ((code == LV_EVENT_PRESSED) || (code == LV_EVENT_FOCUSED) || (code == LV_EVENT_CLICKED)) {
        lv_event_stop_bubbling(e);
        wifi_show_keyboard(lv_event_get_current_target_obj(e));
    }
}

static void wifi_keyboard_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if ((code == LV_EVENT_READY) || (code == LV_EVENT_CANCEL)) {
        close_keyboard();
    }
}

static void wifi_submit_event_cb(lv_event_t *e)
{
    const char *ssid;
    const char *pwd;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if ((g_ui.wifi_ssid_ta == NULL) || (g_ui.wifi_pwd_ta == NULL)) {
        return;
    }

    ssid = lv_textarea_get_text(g_ui.wifi_ssid_ta);
    pwd = lv_textarea_get_text(g_ui.wifi_pwd_ta);
    if (wifi_task_request_connect(ssid, pwd) == ERRCODE_SUCC) {
        close_wifi_modal();
        watch_ui_request_refresh();
    }
}

static void wifi_disconnect_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        (void)wifi_task_request_disconnect();
        close_wifi_modal();
        watch_ui_request_refresh();
    }
}

static void wifi_action_event_cb(lv_event_t *e)
{
    wifi_ui_action_t action = (wifi_ui_action_t)(uintptr_t)lv_event_get_user_data(e);
    wifi_profile_t profile = {0};

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    lv_event_stop_bubbling(e);

    if (action == WIFI_UI_CONFIG) {
        (void)wifi_provision_start();
        open_wifi_provision_modal();
        watch_ui_request_refresh();
        return;
    }

    if (wifi_task_get_saved_profile(&profile)) {
        (void)wifi_task_request_connect(profile.ssid, profile.pwd);
        watch_ui_request_refresh();
    } else {
        open_wifi_modal(true);
    }
}

static void open_wifi_modal(bool open_keyboard)
{
    wifi_profile_t profile = {0};
    bool has_profile = wifi_task_get_saved_profile(&profile);
    lv_obj_t *modal = modal_base("连接 WiFi", LV_SYMBOL_WIFI);
    lv_obj_set_pos(modal, 10, 22);
    lv_obj_set_size(modal, UI_W - 20, 288);

    lv_obj_t *ssid = label(modal, "SSID", &g_style_muted);
    lv_obj_set_pos(ssid, 0, 52);
    g_ui.wifi_ssid_ta = wifi_textarea(modal, 0, 76, "WiFi SSID", WIFI_MAX_SSID_LEN - 1, false);
    lv_obj_add_event_cb(g_ui.wifi_ssid_ta, wifi_ssid_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(g_ui.wifi_ssid_ta, wifi_ssid_event_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(g_ui.wifi_ssid_ta, wifi_ssid_event_cb, LV_EVENT_CLICKED, NULL);
    if (has_profile) {
        lv_textarea_set_text(g_ui.wifi_ssid_ta, profile.ssid);
    }

    lv_obj_t *pwd = label(modal, "密码", &g_style_muted);
    lv_obj_set_pos(pwd, 0, 126);
    g_ui.wifi_pwd_ta = wifi_textarea(modal, 0, 150, "Password", WIFI_MAX_KEY_LEN - 1, true);
    lv_obj_add_event_cb(g_ui.wifi_pwd_ta, wifi_pwd_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(g_ui.wifi_pwd_ta, wifi_pwd_event_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(g_ui.wifi_pwd_ta, wifi_pwd_event_cb, LV_EVENT_CLICKED, NULL);
    if (has_profile) {
        lv_textarea_set_text(g_ui.wifi_pwd_ta, profile.pwd);
    }

    (void)text_button(modal, 0, 214, 132, 44, "保存连接", C_PANEL_3, &g_style_accent,
                      wifi_submit_event_cb, NULL);
    (void)text_button(modal, 148, 214, 132, 44, "断开", 0x4A1F2C, &g_style_red,
                      wifi_disconnect_event_cb, NULL);

    if (open_keyboard) {
        wifi_show_keyboard(g_ui.wifi_ssid_ta);
    }
}

static void show_wifi_modal(lv_event_t *e)
{
    (void)e;
    open_wifi_modal(true);
}

static void wifi_manual_config_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        lv_event_stop_bubbling(e);
        open_wifi_modal(true);
    }
}

static void wifi_provision_stop_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        lv_event_stop_bubbling(e);
        wifi_provision_stop();
        close_wifi_modal();
        watch_ui_request_refresh();
    }
}

static void open_wifi_provision_modal(void)
{
    lv_obj_t *modal = modal_base("WiFi App Setup", LV_SYMBOL_WIFI);
    lv_obj_set_pos(modal, 10, 22);
    lv_obj_set_size(modal, UI_W - 20, 300);

    lv_obj_t *line = label(modal, "SoftAP", &g_style_muted);
    lv_obj_set_pos(line, 0, 56);
    lv_obj_set_size(line, 280, 22);

    line = label(modal, "SSID: " WIFI_PROV_AP_SSID, &g_style_text);
    lv_obj_set_pos(line, 0, 84);
    lv_obj_set_size(line, 280, 22);

    line = label(modal, "PWD: " WIFI_PROV_AP_PASSWORD, &g_style_text);
    lv_obj_set_pos(line, 0, 112);
    lv_obj_set_size(line, 280, 22);

    line = label(modal, "URL: http://" WIFI_PROV_AP_IP, &g_style_accent);
    lv_obj_set_pos(line, 0, 150);
    lv_obj_set_size(line, 280, 22);

    line = label(modal, "POST " WIFI_PROV_HTTP_PATH, &g_style_muted);
    lv_obj_set_pos(line, 0, 178);
    lv_obj_set_size(line, 280, 22);

    line = label(modal, "JSON: ssid,password", &g_style_muted);
    lv_obj_set_pos(line, 0, 206);
    lv_obj_set_size(line, 280, 22);

    (void)text_button(modal, 0, 236, 132, 44, "Manual", C_PANEL_3, &g_style_accent,
                      wifi_manual_config_event_cb, NULL);
    (void)text_button(modal, 148, 236, 132, 44, "Stop AP", 0x4A1F2C, &g_style_red,
                      wifi_provision_stop_event_cb, NULL);
}

static uint32_t scan_rssi_color(int8_t rssi)
{
    if (rssi == 127) {
        return C_MUTED;
    }
    if (rssi >= -55) {
        return C_ACCENT;
    }
    if (rssi >= -75) {
        return C_YELLOW;
    }
    return C_RED;
}

static const char *scan_rssi_level(int8_t rssi)
{
    if (rssi == 127) {
        return "--";
    }
    if (rssi >= -55) {
        return "好";
    }
    if (rssi >= -75) {
        return "中";
    }
    return "差";
}

static void scan_add_event_cb(lv_event_t *e)
{
    uintptr_t index = (uintptr_t)lv_event_get_user_data(e);
    watch_model_snapshot_t model;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    lv_event_stop_bubbling(e);

    if (sle_watch_connect_scan_result((uint8_t)index) == ERRCODE_SUCC) {
        watch_model_get_snapshot(&model);
        g_ui.model_version = model.version;
        render_all(&model);
        refresh_scan_modal(&model);
    }
}

static void refresh_scan_modal(const watch_model_snapshot_t *model)
{
    char text[64];

    if ((g_ui.scan_modal_open == 0) || (g_ui.scan_list == NULL) || (model == NULL)) {
        return;
    }

    lv_obj_clean(g_ui.scan_list);
    if (model->scan_count == 0) {
        lv_obj_t *hint = label(g_ui.scan_list, model->scan_active ? "SLE扫描中..." : "未发现 watch-xx 设备",
                               model->scan_active ? &g_style_accent : &g_style_muted);
        lv_obj_set_size(hint, 260, 24);
        lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(hint);
    } else {
        for (uint8_t i = 0; i < model->scan_count; i++) {
            const watch_scan_device_t *dev = &model->scan_results[i];
            int32_t y = (int32_t)i * 74;
            lv_obj_t *item = card(g_ui.scan_list, 0, y, 280, 64, false);
            uint32_t rssi_color = scan_rssi_color(dev->rssi);

            icon_box(item, 0, 2, LV_SYMBOL_HOME, C_PANEL_3, rssi_color);

            lv_obj_t *name = label(item, dev->name, &g_style_title);
            lv_obj_set_pos(name, 50, 0);
            lv_obj_set_size(name, 86, 24);

            lv_obj_t *addr = mono_label(item, dev->address, C_MUTED);
            lv_obj_set_pos(addr, 50, 28);
            lv_obj_set_size(addr, 126, 20);

            if (dev->rssi == 127) {
                (void)snprintf_s(text, sizeof(text), sizeof(text) - 1, "%s  -- dBm", scan_rssi_level(dev->rssi));
            } else {
                (void)snprintf_s(text, sizeof(text), sizeof(text) - 1, "%s  %d dBm", scan_rssi_level(dev->rssi),
                                 dev->rssi);
            }
            lv_obj_t *rssi = label(item, text, rssi_color == C_RED ? &g_style_red_sm :
                                   (rssi_color == C_YELLOW ? &g_style_yellow : &g_style_accent_sm));
            lv_obj_set_pos(rssi, 140, 2);
            lv_obj_set_size(rssi, 72, 20);
            lv_obj_set_style_text_align(rssi, LV_TEXT_ALIGN_RIGHT, 0);

            lv_obj_t *btn = lv_button_create(item);
            lv_obj_remove_style_all(btn);
            lv_obj_set_pos(btn, 220, 8);
            lv_obj_set_size(btn, 52, 40);
            lv_obj_set_style_radius(btn, 10, 0);
            lv_obj_set_style_bg_color(btn, color(dev->added ? 0x20263A : C_PANEL_3), 0);
            lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
            if (dev->added == 0) {
                lv_obj_add_event_cb(btn, scan_add_event_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
            }
            lv_obj_t *btn_label = symbol_label(btn, dev->added ? LV_SYMBOL_OK : LV_SYMBOL_PLUS,
                                               dev->added ? C_MUTED : C_ACCENT);
            lv_obj_center(btn_label);
        }
    }

    if (g_ui.scan_count_label != NULL) {
        (void)snprintf_s(text, sizeof(text), sizeof(text) - 1,
                         model->scan_active ? "扫描 %u 台设备" : "发现 %u 台设备", model->scan_count);
        lv_label_set_text(g_ui.scan_count_label, text);
    }
}

static void show_scan_modal(lv_event_t *e)
{
    watch_model_snapshot_t model;

    (void)e;
    lv_obj_t *modal = modal_base("SLE 设备搜寻", LV_SYMBOL_REFRESH);
    lv_obj_set_pos(modal, 10, 58);
    lv_obj_set_size(modal, UI_W - 20, 324);

    g_ui.scan_modal_open = 1;
    g_ui.scan_list = plain_obj(modal);
    lv_obj_set_pos(g_ui.scan_list, 0, 58);
    lv_obj_set_size(g_ui.scan_list, 280, 214);
    lv_obj_set_scroll_dir(g_ui.scan_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(g_ui.scan_list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(g_ui.scan_list, LV_OBJ_FLAG_SCROLLABLE);

    dot(modal, 0, 288, C_ACCENT);
    lv_obj_t *strong = label(modal, "好", &g_style_muted);
    lv_obj_set_pos(strong, 17, 280);
    dot(modal, 58, 288, C_YELLOW);
    lv_obj_t *mid = label(modal, "中", &g_style_muted);
    lv_obj_set_pos(mid, 75, 280);
    dot(modal, 116, 288, C_RED);
    lv_obj_t *weak = label(modal, "差", &g_style_muted);
    lv_obj_set_pos(weak, 133, 280);
    g_ui.scan_count_label = label(modal, "扫描 0 台设备", &g_style_muted);
    lv_obj_set_pos(g_ui.scan_count_label, 166, 280);
    lv_obj_set_size(g_ui.scan_count_label, 110, 24);

    (void)sle_watch_scan_start();
    watch_model_get_snapshot(&model);
    refresh_scan_modal(&model);
}

static void create_header(lv_obj_t *screen)
{
    lv_obj_t *header = lv_obj_create(screen);
    lv_obj_remove_style_all(header);
    lv_obj_add_style(header, &g_style_header, 0);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_size(header, UI_W, UI_HEADER_H);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    icon_box(header, 10, 9, LV_SYMBOL_WIFI, C_PANEL_3, C_ACCENT);
    lv_obj_t *title = label(header, "WatchControl", &g_style_title);
    lv_obj_set_pos(title, 56, 12);
    lv_obj_set_size(title, 170, 28);
    g_ui.run_dot = dot(header, 256, 22, C_ACCENT);
    g_ui.run_label = label(header, "运行中", &g_style_muted);
    lv_obj_set_pos(g_ui.run_label, 272, 15);
    lv_obj_set_size(g_ui.run_label, 48, 24);
}

static void create_nav(lv_obj_t *screen)
{
    lv_obj_t *nav = lv_obj_create(screen);
    const int32_t nav_item_w = UI_W / PAGE_COUNT;
    const int32_t nav_icon_w = 34;
    const int32_t nav_label_w = 84;
    lv_obj_remove_style_all(nav);
    lv_obj_add_style(nav, &g_style_nav, 0);
    lv_obj_set_pos(nav, 0, UI_H - UI_NAV_H);
    lv_obj_set_size(nav, UI_W, UI_NAV_H);
    lv_obj_clear_flag(nav, LV_OBJ_FLAG_SCROLLABLE);

    for (uint8_t i = 0; i < PAGE_COUNT; i++) {
        lv_obj_t *btn = lv_button_create(nav);
        lv_obj_remove_style_all(btn);
        lv_obj_set_pos(btn, i * nav_item_w, 0);
        lv_obj_set_size(btn, nav_item_w, UI_NAV_H);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(btn, color(C_PANEL), 0);
        lv_obj_add_event_cb(btn, nav_event_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);

        g_ui.nav_btns[i] = btn;
        g_ui.nav_icons[i] = symbol_label(btn, "", C_MUTED);
        lv_obj_set_pos(g_ui.nav_icons[i], (nav_item_w - nav_icon_w) / 2, 7);
        lv_obj_set_size(g_ui.nav_icons[i], nav_icon_w, 24);
        lv_obj_set_style_text_align(g_ui.nav_icons[i], LV_TEXT_ALIGN_CENTER, 0);
        g_ui.nav_labels[i] = mono_label(btn, "", C_MUTED);
        lv_obj_set_pos(g_ui.nav_labels[i], (nav_item_w - nav_label_w) / 2, 36);
        lv_obj_set_size(g_ui.nav_labels[i], nav_label_w, 20);
        lv_obj_set_style_text_align(g_ui.nav_labels[i], LV_TEXT_ALIGN_CENTER, 0);
    }

    g_ui.nav_badge = lv_obj_create(nav);
    lv_obj_remove_style_all(g_ui.nav_badge);
    lv_obj_set_pos(g_ui.nav_badge, nav_item_w * PAGE_LOG + 55, 3);
    lv_obj_set_size(g_ui.nav_badge, 18, 18);
    lv_obj_set_style_radius(g_ui.nav_badge, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(g_ui.nav_badge, color(C_YELLOW), 0);
    lv_obj_set_style_bg_opa(g_ui.nav_badge, LV_OPA_COVER, 0);
    g_ui.nav_badge_label = mono_label(g_ui.nav_badge, "1", C_BG);
    lv_obj_set_size(g_ui.nav_badge_label, 18, 18);
    lv_obj_set_style_text_align(g_ui.nav_badge_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(g_ui.nav_badge_label);
}

static void render_status_page(const watch_model_snapshot_t *model)
{
    lv_obj_t *page = g_ui.pages[PAGE_STATUS];
    uint8_t idle = 0;
    uint8_t borrowed = 0;

    for (uint8_t i = 0; i < model->device_count; i++) {
        if (model->devices[i].state == WATCH_DEVICE_BORROWED) {
            borrowed++;
        } else if (model->devices[i].state == WATCH_DEVICE_IDLE) {
            idle++;
        }
    }

    lv_obj_clean(page);

    uint32_t wifi_color = link_color(model->wifi_state);
    uint32_t mqtt_color = link_color(model->mqtt_state);
    uint32_t sle_color = link_color(model->sle_state);

    /* ========== WiFi 卡片创建代码 ========== */

    lv_obj_t *wifi = card(page, 8, 8, 304, 112, false);

    /* 1. 移除卡片本身的点击能力和模态框事件 —— 点击卡片空白处不再有任何反应 */
    // lv_obj_add_flag(wifi, LV_OBJ_FLAG_CLICKABLE);                         // 已移除
    // lv_obj_add_event_cb(wifi, show_wifi_modal, LV_EVENT_CLICKED, NULL);   // 已移除

    icon_box(wifi, 0, 0, LV_SYMBOL_WIFI, link_icon_bg(model->wifi_state), wifi_color);

    lv_obj_t *wifi_title = label(wifi, "WiFi", &g_style_title);
    lv_obj_set_pos(wifi_title, 50, 7);
    lv_obj_set_size(wifi_title, 74, 24);

    /* 2. 设置按钮：保留原来的动作回调，并新增模态框回调 */
    lv_obj_t *wifi_cfg_btn = lv_button_create(wifi);
    lv_obj_remove_style_all(wifi_cfg_btn);
    lv_obj_set_pos(wifi_cfg_btn, 178, 3);
    lv_obj_set_size(wifi_cfg_btn, 36, 36);
    lv_obj_set_style_radius(wifi_cfg_btn, 10, 0);
    lv_obj_set_style_bg_color(wifi_cfg_btn, color(0x20263A), 0);
    lv_obj_set_style_bg_opa(wifi_cfg_btn, LV_OPA_COVER, 0);

    /* 先执行配置动作（WIFI_UI_CONFIG），再弹出模态框 */
    lv_obj_add_event_cb(wifi_cfg_btn, wifi_action_event_cb, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)WIFI_UI_CONFIG);

    lv_obj_t *wifi_cfg_icon = symbol_label(wifi_cfg_btn, LV_SYMBOL_SETTINGS, C_MUTED);
    lv_obj_center(wifi_cfg_icon);

    dot(wifi, 226, 15, wifi_color);

    lv_obj_t *wifi_state = label(wifi, link_text(model->wifi_state), link_style_sm(model->wifi_state));
    lv_obj_set_pos(wifi_state, 212, 9);
    lv_obj_set_size(wifi_state, 58, 20);
    lv_obj_set_style_text_align(wifi_state, LV_TEXT_ALIGN_RIGHT, 0);

    lv_obj_t *wifi_caption = label(wifi, "状态", &g_style_muted);
    lv_obj_set_pos(wifi_caption, 0, 62);
    lv_obj_set_size(wifi_caption, 72, 22);
    lv_obj_set_style_text_align(wifi_caption, LV_TEXT_ALIGN_LEFT, 0);

    lv_obj_t *wifi_detail = label(wifi, "", model->wifi_state == WATCH_LINK_ERROR ? &g_style_red : &g_style_text);
    if (model->wifi_state == WATCH_LINK_CONNECTED) {
        lv_label_set_text(wifi_detail, model->wifi_ssid[0] != '\0' ? model->wifi_ssid : "已连接");
    } else if (model->wifi_state == WATCH_LINK_CONNECTING) {
        lv_label_set_text(wifi_detail, "正在连接");
    } else if (model->wifi_state == WATCH_LINK_ERROR) {
        lv_label_set_text(wifi_detail, "连接失效");
    } else {
        lv_label_set_text(wifi_detail, "未连接");
    }
    lv_obj_set_pos(wifi_detail, 120, 62);
    lv_obj_set_size(wifi_detail, 158, 22);
    lv_obj_set_style_text_align(wifi_detail, LV_TEXT_ALIGN_RIGHT, 0);

    lv_obj_t *mqtt = card(page, 8, 132, 304, 125, false);
    icon_box(mqtt, 0, 4, LV_SYMBOL_DRIVE, link_icon_bg(model->mqtt_state), mqtt_color);
    lv_obj_t *mqtt_title = mono_title(mqtt, "MQTT", C_TEXT);
    lv_obj_set_pos(mqtt_title, 50, 10);
    dot(mqtt, 212, 17, mqtt_color);
    lv_obj_t *mqtt_state = label(mqtt, link_text(model->mqtt_state), link_style_sm(model->mqtt_state));
    lv_obj_set_pos(mqtt_state, 212, 9);
    lv_obj_set_size(mqtt_state, 58, 20);
    lv_obj_set_style_text_align(mqtt_state, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_t *broker = mono_label(mqtt, "Broker", C_MUTED);
    lv_obj_set_pos(broker, 0, 58);
    lv_obj_set_size(broker, 70, 18);
    lv_obj_set_style_text_align(broker, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_t *broker_value = mono_label(mqtt, model->mqtt_broker, C_TEXT);
    lv_obj_set_pos(broker_value, 90, 58);
    lv_obj_set_size(broker_value, 188, 17);
    lv_obj_set_style_text_align(broker_value, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_t *topic = mono_label(mqtt, "Topic", C_MUTED);
    lv_obj_set_pos(topic, 0, 84);
    lv_obj_set_size(topic, 70, 18);
    lv_obj_set_style_text_align(topic, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_t *topic_value = mono_label(mqtt, model->mqtt_topic, C_TEXT);
    lv_obj_set_pos(topic_value, 90, 84);
    lv_obj_set_size(topic_value, 180, 18);
    lv_obj_set_style_text_align(topic_value, LV_TEXT_ALIGN_RIGHT, 0);

    lv_obj_t *sle = card(page, 8, 270, 304, 116, false);
    icon_box(sle, 0, 0, LV_SYMBOL_WIFI, link_icon_bg(model->sle_state), sle_color);
    lv_obj_t *sle_title = label(sle, "SLE", &g_style_title);
    lv_obj_set_pos(sle_title, 50, 6);
    dot(sle, 212, 15, sle_color);
    lv_obj_t *sle_state = label(sle, link_text(model->sle_state), link_style_sm(model->sle_state));
    lv_obj_set_pos(sle_state, 212, 9);
    lv_obj_set_size(sle_state, 58, 20);
    lv_obj_set_style_text_align(sle_state, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_t *service = label(sle, "服务", &g_style_muted);
    lv_obj_set_pos(service, 0, 52);
    lv_obj_set_size(service, 70, 22);
    lv_obj_set_style_text_align(service, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_t *service_value = mono_label(sle, model->sle_service, C_TEXT);
    lv_obj_set_pos(service_value, 128, 52);
    lv_obj_set_size(service_value, 142, 22);
    lv_obj_set_style_text_align(service_value, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_t *connected = label(sle, "已连接", &g_style_muted);
    lv_obj_set_pos(connected, 0, 76);
    lv_obj_set_size(connected, 70, 22);
    lv_obj_set_style_text_align(connected, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_t *connected_value = label(sle, "", &g_style_text);
    lv_label_set_text_fmt(connected_value, "%u 台设备", model->sle_connected_count);
    lv_obj_set_pos(connected_value, 188, 76);
    lv_obj_set_size(connected_value, 82, 22);
    lv_obj_set_style_text_align(connected_value, LV_TEXT_ALIGN_RIGHT, 0);

    lv_obj_t *summary = card(page, 8, 402, 304, 92, false);
    lv_obj_t *arc = lv_arc_create(summary);
    lv_obj_set_size(arc, 58, 58);
    lv_obj_set_pos(arc, 0, -4);
    lv_arc_set_range(arc, 0, 4);
    lv_arc_set_value(arc, model->sle_connected_count);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(arc, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, color(0x233047), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, color(C_ACCENT), LV_PART_INDICATOR);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *num = label(summary, "", &g_style_accent);
    lv_label_set_text_fmt(num, "%u", model->sle_connected_count);
    lv_obj_set_pos(num, 22, 16);
    lv_obj_t *sum_title = label(summary, "SLE 设备", &g_style_title);
    lv_obj_set_pos(sum_title, 78, 2);
    lv_obj_t *sum_count = label(summary, "", &g_style_muted);
    lv_label_set_text_fmt(sum_count, "%u 已连接 / %u 合计", model->sle_connected_count, model->device_count);
    lv_obj_set_pos(sum_count, 78, 27);
    lv_obj_set_size(sum_count, 142, 22);
    lv_obj_t *sum_detail = label(summary, "", &g_style_accent);
    lv_label_set_text_fmt(sum_detail, "%u 空闲  ", idle);
    lv_obj_set_pos(sum_detail, 78, 52);
    lv_obj_set_size(sum_detail, 70, 20);
    lv_obj_t *sum_warn = label(summary, "", &g_style_yellow);
    lv_label_set_text_fmt(sum_warn, "%u 借出", borrowed);
    lv_obj_set_pos(sum_warn, 150, 52);
    lv_obj_set_size(sum_warn, 70, 20);

}

static const char *device_state_text(watch_device_state_t state)
{
    switch (state) {
        case WATCH_DEVICE_BORROWED:
            return "借出";
        case WATCH_DEVICE_PENDING:
            return "待定";
        default:
            return "空闲";
    }
}

static const lv_style_t *device_state_style(watch_device_state_t state)
{
    switch (state) {
        case WATCH_DEVICE_BORROWED:
            return &g_style_yellow;
        case WATCH_DEVICE_PENDING:
            return &g_style_accent;
        default:
            return &g_style_accent;
    }
}

static void device_action_event_cb(lv_event_t *e)
{
    uintptr_t index = (uintptr_t)lv_event_get_user_data(e);
    watch_model_snapshot_t model;
    watch_device_t device;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    lv_event_stop_bubbling(e);

    watch_model_get_snapshot(&model);
    if (index >= model.device_count) {
        return;
    }

    device = model.devices[index];
    if (device.online == 0) {
        return;
    }

    if (device.state == WATCH_DEVICE_BORROWED) {
        (void)watch_borrow_return((uint8_t)index);
    } else if (device.state == WATCH_DEVICE_IDLE) {
        (void)watch_borrow_request((uint8_t)index);
    } else {
        watch_model_add_log(WATCH_LOG_DEVICE, "BORROW", "waiting card");
    }

    watch_model_get_snapshot(&model);
    g_ui.model_version = model.version;
    render_all(&model);
    return;

#if 0
    if (device.state == WATCH_DEVICE_IDLE) {
        device.state = WATCH_DEVICE_BORROWED;
        if (strncpy_s(device.borrower, sizeof(device.borrower), "用户",
                      sizeof(device.borrower) - 1) != EOK) {
            device.borrower[sizeof(device.borrower) - 1] = '\0';
        }
    }

    watch_model_update_device((uint8_t)index, &device);
    watch_model_get_snapshot(&model);
    g_ui.model_version = model.version;
    render_all(&model);
#endif
}

static void render_device_page(const watch_model_snapshot_t *model)
{
    lv_obj_t *page = g_ui.pages[PAGE_DEVICE];
    char count_buf[64];
    lv_obj_clean(page);
    g_ui.device_swipe_card = NULL;

    uint8_t idle = 0;
    uint8_t borrowed = 0;
    uint8_t pending = 0;
    for (uint8_t i = 0; i < model->device_count; i++) {
        if (model->devices[i].state == WATCH_DEVICE_BORROWED) {
            borrowed++;
        } else if (model->devices[i].state == WATCH_DEVICE_PENDING) {
            pending++;
        } else {
            idle++;
        }
    }

    lv_obj_t *title = label(page, "设备列表", &g_style_title);
    lv_obj_set_pos(title, 6, 6);
    lv_obj_t *summary = label(page, count_buf, &g_style_accent_sm);
    (void)snprintf_s(count_buf, sizeof(count_buf), sizeof(count_buf) - 1, "%u 空闲 %u 借出 %u 待定", idle, borrowed, pending);
    lv_label_set_text(summary, count_buf);
    lv_obj_set_pos(summary, 112, 9);
    lv_obj_set_size(summary, 154, 20);

    lv_obj_t *add_btn = lv_button_create(page);
    lv_obj_remove_style_all(add_btn);
    lv_obj_set_pos(add_btn, 272, 0);
    lv_obj_set_size(add_btn, 40, 40);
    lv_obj_set_style_radius(add_btn, 10, 0);
    lv_obj_set_style_bg_color(add_btn, color(C_PANEL_3), 0);
    lv_obj_set_style_bg_opa(add_btn, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(add_btn, show_scan_modal, LV_EVENT_CLICKED, NULL);
    lv_obj_t *add_label = symbol_label(add_btn, LV_SYMBOL_PLUS, C_ACCENT);
    lv_obj_center(add_label);

    for (uint8_t i = 0; i < model->device_count; i++) {
        const watch_device_t *dev = &model->devices[i];
        int32_t y = 48 + (int32_t)i * 88;
        lv_obj_t *row = plain_obj(page);
        lv_obj_set_pos(row, 8, y);
        lv_obj_set_size(row, UI_DEVICE_ROW_W, UI_DEVICE_ROW_H);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_GESTURE_BUBBLE);

        lv_obj_t *delete_btn = lv_button_create(row);
        lv_obj_remove_style_all(delete_btn);
        lv_obj_set_pos(delete_btn, 252, 9);
        lv_obj_set_size(delete_btn, 42, 60);
        lv_obj_set_style_radius(delete_btn, 10, 0);
        lv_obj_set_style_bg_color(delete_btn, color(0x4A1F2C), 0);
        lv_obj_set_style_bg_opa(delete_btn, LV_OPA_COVER, 0);
        lv_obj_add_event_cb(delete_btn, device_delete_event_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        lv_obj_t *delete_icon = symbol_label(delete_btn, LV_SYMBOL_TRASH, C_RED);
        lv_obj_center(delete_icon);

        lv_obj_t *item = card(row, 0, 0, UI_DEVICE_ROW_W, UI_DEVICE_ROW_H, false);
        lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_GESTURE_BUBBLE);
        lv_obj_add_event_cb(item, device_card_event_cb, LV_EVENT_GESTURE, NULL);
        if ((dev->online == 0) || (dev->state != WATCH_DEVICE_IDLE)) {
            bar(item, 292, 0, 8, UI_DEVICE_ROW_H, C_RED, 8);
        }
        icon_box(item, 0, 1, LV_SYMBOL_HOME, dev->online ? C_PANEL_3 : 0x273044, dev->online ? C_ACCENT : C_MUTED);
        lv_obj_t *name = label(item, dev->name, &g_style_title);
        lv_obj_set_pos(name, 50, 0);
        lv_obj_set_size(name, 76, 24);
        lv_obj_t *pill = label(item, device_state_text(dev->state), device_state_style(dev->state));
        lv_obj_set_pos(pill, 128, 2);
        lv_obj_set_size(pill, 48, 22);
        dot(item, 50, 34, dev->online ? C_ACCENT : C_RED);
        lv_obj_t *state = label(item, dev->online ? "已连接" : "未连接", dev->online ? &g_style_muted_sm : &g_style_red_sm);
        lv_obj_set_pos(state, 66, 25);
        lv_obj_set_size(state, 58, 20);
        if (dev->borrower[0] != '\0') {
            lv_obj_t *borrower = label(item, dev->borrower, &g_style_muted_sm);
            lv_obj_set_pos(borrower, 128, 25);
            lv_obj_set_size(borrower, 104, 20);
        }
        if (dev->borrower_id[0] != '\0') {
            lv_obj_t *borrower_id = mono_label(item, dev->borrower_id, C_MUTED);
            lv_obj_set_style_text_font(borrower_id, &lv_font_source_han_sans_sc_14_cjk, 0);
            lv_obj_set_pos(borrower_id, 50, 49);
            lv_obj_set_size(borrower_id, 182, 18);
        }
        if (dev->online != 0) {
            lv_obj_t *action = lv_button_create(item);
            lv_obj_remove_style_all(action);
            lv_obj_set_pos(action, 236, 4);
            lv_obj_set_size(action, 36, 36);
            lv_obj_set_style_radius(action, 10, 0);
            lv_obj_set_style_bg_color(action, color(dev->state == WATCH_DEVICE_BORROWED ? 0x34342B : C_PANEL_3), 0);
            lv_obj_set_style_bg_opa(action, LV_OPA_COVER, 0);
            lv_obj_add_event_cb(action, device_action_event_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
            lv_obj_t *action_label = symbol_label(action,
                                                  dev->state == WATCH_DEVICE_BORROWED ? LV_SYMBOL_UPLOAD : LV_SYMBOL_DOWNLOAD,
                                                  dev->state == WATCH_DEVICE_BORROWED ? C_YELLOW : C_ACCENT);
            lv_obj_center(action_label);
        }
    }
}

static const lv_style_t *log_style(watch_log_type_t type)
{
    switch (type) {
        case WATCH_LOG_WIFI:
            return &g_style_yellow;
        case WATCH_LOG_WARNING:
            return &g_style_red;
        case WATCH_LOG_MQTT:
            return &g_style_accent;
        default:
            return &g_style_text;
    }
}

static const char *log_icon(watch_log_type_t type)
{
    switch (type) {
        case WATCH_LOG_WIFI:
            return LV_SYMBOL_WIFI;
        case WATCH_LOG_MQTT:
            return LV_SYMBOL_DRIVE;
        case WATCH_LOG_DEVICE:
            return LV_SYMBOL_HOME;
        case WATCH_LOG_WARNING:
            return LV_SYMBOL_WARNING;
        default:
            return LV_SYMBOL_WIFI;
    }
}

static void render_log_page(const watch_model_snapshot_t *model)
{
    lv_obj_t *page = g_ui.pages[PAGE_LOG];
    lv_obj_clean(page);

    if (model->warning_count > 0) {
        lv_obj_t *warn = card(page, 8, 10, 304, 42, false);
        lv_obj_set_style_bg_color(warn, color(0x251421), 0);
        lv_obj_set_style_border_color(warn, color(0x6B2D40), 0);
        lv_obj_t *warn_text = label(warn, "存在失效通知", &g_style_red);
        lv_obj_set_size(warn_text, 304, 26);           /* 宽度与卡片一致 */
        lv_obj_set_style_text_align(warn_text, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(warn_text);                      /* 在卡片内完全居中 */
    }

    lv_obj_t *title = label(page, "通信日志", &g_style_title);
    lv_obj_set_pos(title, 8, 62);

    for (uint8_t i = 0; i < model->log_count; i++) {
        const watch_log_entry_t *entry = &model->logs[i];
        int32_t y = 96 + (int32_t)i * 54;
        if (i + 1 < model->log_count) {
            bar(page, 28, y + 36, 2, 16, 0x172039, 0);
        }
        icon_box(page, 14, y, log_icon(entry->type), 0x172039, entry->type == WATCH_LOG_WARNING ? C_RED : C_ACCENT);
        lv_obj_t *tag = label(page, entry->tag, log_style(entry->type));
        lv_obj_set_pos(tag, 58, y + 1);
        lv_obj_set_size(tag, 54, 20);
        lv_obj_t *time = mono_label(page, entry->time, C_MUTED);
        lv_obj_set_pos(time, 112, y + 1);
        lv_obj_set_size(time, 76, 20);
        lv_obj_t *msg = label(page, entry->message, log_style(entry->type));
        lv_obj_set_pos(msg, 58, y + 24);
        lv_obj_set_size(msg, 246, 20);
    }
}

static void render_all(const watch_model_snapshot_t *model)
{
    char badge_buf[5];

    lv_label_set_text(g_ui.run_label, model->warning_count > 0 ? "正常" : "运行中");
    lv_obj_set_style_text_color(g_ui.run_label, color(model->warning_count > 0 ? C_YELLOW : C_MUTED), 0);
    if (g_ui.nav_badge != NULL) {
        if (model->warning_count > 0) {
            if (g_ui.nav_badge_label != NULL) {
                if (model->warning_count > 9) {
                    lv_label_set_text(g_ui.nav_badge_label, "9+");
                } else {
                    (void)snprintf_s(badge_buf, sizeof(badge_buf), sizeof(badge_buf) - 1,
                                     "%u", model->warning_count);
                    lv_label_set_text(g_ui.nav_badge_label, badge_buf);
                }
            }
            lv_obj_remove_flag(g_ui.nav_badge, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(g_ui.nav_badge, LV_OBJ_FLAG_HIDDEN);
        }
    }
    switch (g_ui.page) {
        case PAGE_STATUS:
            render_status_page(model);
            break;
        case PAGE_DEVICE:
            render_device_page(model);
            break;
        case PAGE_LOG:
            render_log_page(model);
            break;
        default:
            break;
    }
    refresh_scan_modal(model);
}

static void refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    watch_model_snapshot_t model;
    watch_model_get_snapshot(&model);
    if (model.version != g_ui.model_version) {
        g_ui.model_version = model.version;
        render_all(&model);
    }
}

void watch_ui_request_refresh(void)
{
    g_ui.model_version = 0xFFFFFFFF;
}

void watch_ui_create(void)
{
    watch_model_snapshot_t model;

    watch_ui_styles_init();
    g_ui.screen = lv_screen_active();
    lv_obj_remove_style_all(g_ui.screen);
    lv_obj_add_style(g_ui.screen, &g_style_screen, 0);
    lv_obj_clear_flag(g_ui.screen, LV_OBJ_FLAG_SCROLLABLE);

    create_header(g_ui.screen);
    g_ui.content = plain_obj(g_ui.screen);
    lv_obj_set_pos(g_ui.content, 0, UI_CONTENT_Y);
    lv_obj_set_size(g_ui.content, UI_W, UI_CONTENT_H);

    for (uint8_t i = 0; i < PAGE_COUNT; i++) {
        g_ui.pages[i] = plain_obj(g_ui.content);
        lv_obj_set_pos(g_ui.pages[i], 0, 0);
        lv_obj_set_size(g_ui.pages[i], UI_W, UI_CONTENT_H);
        lv_obj_set_scrollbar_mode(g_ui.pages[i], LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_scroll_dir(g_ui.pages[i], LV_DIR_VER);
        lv_obj_add_flag(g_ui.pages[i], LV_OBJ_FLAG_SCROLLABLE);
    }

    create_nav(g_ui.screen);
    g_ui.page = PAGE_COUNT;
    watch_model_get_snapshot(&model);
    g_ui.model_version = model.version;
    render_all(&model);
    show_page(PAGE_STATUS);
    (void)lv_timer_create(refresh_timer_cb, UI_REFRESH_MS, NULL);

    lv_obj_add_event_cb(g_ui.pages[PAGE_STATUS], show_wifi_modal, LV_EVENT_LONG_PRESSED, NULL);
}
