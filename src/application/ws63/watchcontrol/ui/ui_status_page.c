#include "ui_status_page.h"
#include "ui_style.h"

static lv_obj_t *page_root;
static lv_obj_t *wifi_card, *mqtt_card, *sle_card;
static lv_obj_t *wifi_status_dot, *mqtt_status_dot, *sle_status_dot;
static lv_obj_t *wifi_ssid_label, *wifi_ip_label;
static lv_obj_t *mqtt_broker_label;
static lv_obj_t *sle_broadcast_label, *sle_count_label;
static lv_obj_t *arc_obj;
static lv_obj_t *arc_online_label, *arc_offline_label;
static lv_obj_t *arc_idle_label, *arc_borrowed_label;
static lv_obj_t *wifi_settings_btn;

static ui_event_cb_t wifi_page_cb = NULL;
static ui_event_cb_t sle_page_cb = NULL;

void ui_set_wifi_page_cb(ui_event_cb_t cb)
{
    wifi_page_cb = cb;
}

void ui_set_sle_page_cb(ui_event_cb_t cb)
{
    sle_page_cb = cb;
}

static lv_obj_t *create_status_dot(lv_obj_t *parent, lv_color_t color)
{
    lv_obj_t *dot = lv_obj_create(parent);
    lv_obj_set_size(dot, 8, 8);
    lv_obj_set_style_bg_color(dot, color, 0);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_set_style_pad_all(dot, 0, 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return dot;
}

static lv_obj_t *create_icon_block(lv_obj_t *parent, lv_color_t color)
{
    lv_obj_t *icon = lv_obj_create(parent);
    lv_obj_set_size(icon, 24, 24);
    lv_obj_set_style_bg_color(icon, color, 0);
    lv_obj_set_style_radius(icon, 4, 0);
    lv_obj_set_style_border_width(icon, 0, 0);
    lv_obj_set_style_pad_all(icon, 0, 0);
    lv_obj_clear_flag(icon, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return icon;
}

static lv_obj_t *create_card(lv_obj_t *parent, const char *title,
                              lv_color_t icon_color, lv_obj_t **dot_out,
                              lv_obj_t **icon_out)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_add_style(card, &style_card, 0);
    lv_obj_set_size(card, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(card, 10, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *icon = create_icon_block(card, icon_color);
    if (icon_out) *icon_out = icon;

    lv_obj_t *mid = lv_obj_create(card);
    lv_obj_set_size(mid, LV_PCT(1), LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(mid, 1);
    lv_obj_set_flex_flow(mid, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(mid, 2, 0);
    lv_obj_set_style_border_width(mid, 0, 0);
    lv_obj_set_style_bg_opa(mid, 0, 0);
    lv_obj_set_style_pad_all(mid, 0, 0);
    lv_obj_clear_flag(mid, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title_label = lv_label_create(mid);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title_label, COLOR_TEXT_PRIMARY, 0);

    lv_obj_t *detail_label = lv_label_create(mid);
    lv_label_set_text(detail_label, "--");
    lv_obj_set_style_text_font(detail_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(detail_label, COLOR_TEXT_SECONDARY, 0);

    lv_obj_t *dot = create_status_dot(card, COLOR_RED);
    if (dot_out) *dot_out = dot;

    lv_obj_set_user_data(card, detail_label);
    return card;
}

static void wifi_settings_event_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (wifi_page_cb) {
        wifi_page_cb(NULL);
    }
}

static lv_obj_t *create_wifi_card(lv_obj_t *parent)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_add_style(card, &style_card, 0);
    lv_obj_set_size(card, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(card, 10, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *icon = create_icon_block(card, COLOR_CYAN);
    (void)icon;

    lv_obj_t *mid = lv_obj_create(card);
    lv_obj_set_size(mid, LV_PCT(1), LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(mid, 1);
    lv_obj_set_flex_flow(mid, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(mid, 2, 0);
    lv_obj_set_style_border_width(mid, 0, 0);
    lv_obj_set_style_bg_opa(mid, 0, 0);
    lv_obj_set_style_pad_all(mid, 0, 0);
    lv_obj_clear_flag(mid, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title_label = lv_label_create(mid);
    lv_label_set_text(title_label, "WiFi");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title_label, COLOR_TEXT_PRIMARY, 0);

    wifi_ssid_label = lv_label_create(mid);
    lv_label_set_text(wifi_ssid_label, "未连接");
    lv_obj_set_style_text_font(wifi_ssid_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(wifi_ssid_label, COLOR_TEXT_SECONDARY, 0);

    wifi_ip_label = lv_label_create(mid);
    lv_label_set_text(wifi_ip_label, "");
    lv_obj_set_style_text_font(wifi_ip_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(wifi_ip_label, COLOR_TEXT_DIM, 0);

    wifi_settings_btn = lv_btn_create(card);
    lv_obj_set_size(wifi_settings_btn, 28, 28);
    lv_obj_set_style_bg_color(wifi_settings_btn, COLOR_CARD, 0);
    lv_obj_set_style_radius(wifi_settings_btn, 6, 0);
    lv_obj_set_style_border_color(wifi_settings_btn, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_border_width(wifi_settings_btn, 1, 0);
    lv_obj_add_event_cb(wifi_settings_btn, wifi_settings_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_label = lv_label_create(wifi_settings_btn);
    lv_label_set_text(btn_label, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(btn_label, COLOR_TEXT_SECONDARY, 0);
    lv_obj_center(btn_label);

    wifi_status_dot = create_status_dot(card, COLOR_RED);

    return card;
}

static lv_obj_t *create_arc_section(lv_obj_t *parent)
{
    lv_obj_t *section = lv_obj_create(parent);
    lv_obj_add_style(section, &style_card, 0);
    lv_obj_set_size(section, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(section, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(section, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(section, 16, 0);
    lv_obj_clear_flag(section, LV_OBJ_FLAG_SCROLLABLE);

    arc_obj = lv_arc_create(section);
    lv_obj_set_size(arc_obj, 80, 80);
    lv_arc_set_rotation(arc_obj, 270);
    lv_arc_set_bg_angles(arc_obj, 0, 360);
    lv_arc_set_range(arc_obj, 0, 100);
    lv_arc_set_value(arc_obj, 0);
    lv_obj_set_style_arc_color(arc_obj, COLOR_GREEN, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc_obj, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc_obj, COLOR_TEXT_DIM, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_obj, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_color(arc_obj, COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(arc_obj, 0, 0);
    lv_obj_remove_style(arc_obj, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc_obj, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *labels = lv_obj_create(section);
    lv_obj_set_flex_flow(labels, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(labels, 4, 0);
    lv_obj_set_style_border_width(labels, 0, 0);
    lv_obj_set_style_bg_opa(labels, 0, 0);
    lv_obj_set_style_pad_all(labels, 0, 0);
    lv_obj_clear_flag(labels, LV_OBJ_FLAG_SCROLLABLE);

    arc_online_label = lv_label_create(labels);
    lv_label_set_text(arc_online_label, "在线: 0");
    lv_obj_set_style_text_font(arc_online_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(arc_online_label, COLOR_GREEN, 0);

    arc_offline_label = lv_label_create(labels);
    lv_label_set_text(arc_offline_label, "离线: 0");
    lv_obj_set_style_text_font(arc_offline_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(arc_offline_label, COLOR_RED, 0);

    arc_idle_label = lv_label_create(labels);
    lv_label_set_text(arc_idle_label, "空闲: 0");
    lv_obj_set_style_text_font(arc_idle_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(arc_idle_label, COLOR_YELLOW, 0);

    arc_borrowed_label = lv_label_create(labels);
    lv_label_set_text(arc_borrowed_label, "借出: 0");
    lv_obj_set_style_text_font(arc_borrowed_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(arc_borrowed_label, COLOR_CYAN, 0);

    return section;
}

lv_obj_t *ui_status_page_create(lv_obj_t *parent)
{
    page_root = lv_obj_create(parent);
    lv_obj_set_size(page_root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(page_root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(page_root, 6, 0);
    lv_obj_set_style_pad_row(page_root, 6, 0);
    lv_obj_set_style_bg_color(page_root, COLOR_BG, 0);
    lv_obj_set_style_border_width(page_root, 0, 0);
    lv_obj_set_scroll_dir(page_root, LV_DIR_VER);

    wifi_card = create_wifi_card(page_root);

    mqtt_card = create_card(page_root, "MQTT", COLOR_GREEN, &mqtt_status_dot, NULL);
    mqtt_broker_label = (lv_obj_t *)lv_obj_get_user_data(mqtt_card);
    lv_label_set_text(mqtt_broker_label, "未连接");

    sle_card = create_card(page_root, "SLE 星闪", COLOR_YELLOW, &sle_status_dot, NULL);
    lv_obj_t *sle_detail = (lv_obj_t *)lv_obj_get_user_data(sle_card);
    lv_label_set_text(sle_detail, "未广播");
    sle_broadcast_label = sle_detail;

    sle_count_label = lv_label_create(lv_obj_get_parent(sle_detail));
    lv_label_set_text(sle_count_label, "连接: 0");
    lv_obj_set_style_text_font(sle_count_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(sle_count_label, COLOR_TEXT_DIM, 0);

    create_arc_section(page_root);

    return page_root;
}

void ui_status_page_update(const ui_system_status_t *status, const ui_device_t *devices, int device_count)
{
    if (!status) return;

    if (status->wifi_connected) {
        lv_obj_set_style_bg_color(wifi_status_dot, COLOR_GREEN, 0);
        lv_label_set_text_fmt(wifi_ssid_label, "SSID: %s", status->wifi_ssid);
        lv_label_set_text_fmt(wifi_ip_label, "IP: %s", status->wifi_ip);
    } else {
        lv_obj_set_style_bg_color(wifi_status_dot, COLOR_RED, 0);
        lv_label_set_text(wifi_ssid_label, "未连接");
        lv_label_set_text(wifi_ip_label, "");
    }

    if (status->mqtt_connected) {
        lv_obj_set_style_bg_color(mqtt_status_dot, COLOR_GREEN, 0);
        lv_label_set_text(mqtt_broker_label, "已连接");
    } else {
        lv_obj_set_style_bg_color(mqtt_status_dot, COLOR_RED, 0);
        lv_label_set_text(mqtt_broker_label, "未连接");
    }

    if (status->sle_broadcasting) {
        lv_obj_set_style_bg_color(sle_status_dot, COLOR_GREEN, 0);
        lv_label_set_text(sle_broadcast_label, "广播中");
    } else {
        lv_obj_set_style_bg_color(sle_status_dot, COLOR_RED, 0);
        lv_label_set_text(sle_broadcast_label, "未广播");
    }
    lv_label_set_text_fmt(sle_count_label, "连接: %d", status->sle_connected_count);

    if (devices && device_count > 0) {
        int online = 0, offline = 0, idle = 0, borrowed = 0;
        for (int i = 0; i < device_count && i < MAX_DEVICES; i++) {
            if (devices[i].connected) online++; else offline++;
            if (devices[i].status == 0) idle++;
            else if (devices[i].status == 1) borrowed++;
        }
        int total = device_count;
        int pct = total > 0 ? (online * 100 / total) : 0;
        lv_arc_set_value(arc_obj, pct);

        lv_label_set_text_fmt(arc_online_label, "在线: %d", online);
        lv_label_set_text_fmt(arc_offline_label, "离线: %d", offline);
        lv_label_set_text_fmt(arc_idle_label, "空闲: %d", idle);
        lv_label_set_text_fmt(arc_borrowed_label, "借出: %d", borrowed);
    } else {
        lv_arc_set_value(arc_obj, 0);
        lv_label_set_text(arc_online_label, "在线: 0");
        lv_label_set_text(arc_offline_label, "离线: 0");
        lv_label_set_text(arc_idle_label, "空闲: 0");
        lv_label_set_text(arc_borrowed_label, "借出: 0");
    }
}
