#include "ui_wifi_page.h"
#include "ui_style.h"

static lv_obj_t *overlay;
static lv_obj_t *panel;
static lv_obj_t *ssid_ta;
static lv_obj_t *password_ta;
static lv_obj_t *connect_btn;
static lv_obj_t *spinner_obj;
static lv_obj_t *conn_info_label;

static void close_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_wifi_page_close();
}

static void connect_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    lv_obj_add_flag(connect_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(spinner_obj, LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t *ui_wifi_page_create(lv_obj_t *parent)
{
    (void)parent;
    overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_50, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_pad_all(overlay, 0, 0);
    lv_obj_set_style_radius(overlay, 0, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(overlay, close_cb, LV_EVENT_CLICKED, NULL);

    panel = lv_obj_create(overlay);
    lv_obj_set_size(panel, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_max_height(panel, LV_PCT(60), 0);
    lv_obj_set_style_bg_color(panel, COLOR_CARD, 0);
    lv_obj_set_style_radius(panel, 16, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 12, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(panel, 10, 0);
    lv_obj_align(panel, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(panel, NULL, LV_EVENT_CLICKED, NULL);

    lv_obj_t *header = lv_obj_create(panel);
    lv_obj_set_size(header, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(header, 8, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_bg_opa(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "连接WiFi");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, COLOR_TEXT_PRIMARY, 0);

    lv_obj_t *spacer = lv_obj_create(header);
    lv_obj_set_flex_grow(spacer, 1);
    lv_obj_set_size(spacer, 0, 0);
    lv_obj_set_style_border_width(spacer, 0, 0);
    lv_obj_set_style_bg_opa(spacer, 0, 0);
    lv_obj_clear_flag(spacer, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *close_btn = lv_btn_create(header);
    lv_obj_set_size(close_btn, 28, 28);
    lv_obj_set_style_bg_color(close_btn, COLOR_CARD, 0);
    lv_obj_set_style_radius(close_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_color(close_btn, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_border_width(close_btn, 1, 0);
    lv_obj_add_event_cb(close_btn, close_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *close_lbl = lv_label_create(close_btn);
    lv_label_set_text(close_lbl, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_font(close_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(close_lbl, COLOR_TEXT_SECONDARY, 0);
    lv_obj_center(close_lbl);

    conn_info_label = lv_label_create(panel);
    lv_label_set_text(conn_info_label, "");
    lv_obj_set_style_text_font(conn_info_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(conn_info_label, COLOR_TEXT_SECONDARY, 0);
    lv_obj_add_flag(conn_info_label, LV_OBJ_FLAG_HIDDEN);

    ssid_ta = lv_textarea_create(panel);
    lv_textarea_set_one_line(ssid_ta, true);
    lv_textarea_set_placeholder_text(ssid_ta, "输入WiFi名称");
    lv_obj_set_style_text_font(ssid_ta, &lv_font_montserrat_14, 0);
    lv_obj_add_style(ssid_ta, &style_input, 0);
    lv_obj_set_width(ssid_ta, LV_PCT(100));

    password_ta = lv_textarea_create(panel);
    lv_textarea_set_one_line(password_ta, true);
    lv_textarea_set_password_mode(password_ta, true);
    lv_textarea_set_placeholder_text(password_ta, "输入WiFi密码");
    lv_obj_set_style_text_font(password_ta, &lv_font_montserrat_14, 0);
    lv_obj_add_style(password_ta, &style_input, 0);
    lv_obj_set_width(password_ta, LV_PCT(100));

    connect_btn = lv_btn_create(panel);
    lv_obj_set_size(connect_btn, LV_PCT(100), 40);
    lv_obj_add_style(connect_btn, &style_btn_green, 0);
    lv_obj_add_event_cb(connect_btn, connect_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *btn_lbl = lv_label_create(connect_btn);
    lv_label_set_text(btn_lbl, "连接");
    lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(btn_lbl);

    spinner_obj = lv_spinner_create(panel);
    lv_obj_set_size(spinner_obj, 32, 32);
    lv_obj_set_style_arc_color(spinner_obj, COLOR_GREEN, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(spinner_obj, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(spinner_obj, COLOR_TEXT_DIM, LV_PART_MAIN);
    lv_obj_set_style_arc_width(spinner_obj, 4, LV_PART_MAIN);
    lv_obj_align(spinner_obj, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(spinner_obj, LV_OBJ_FLAG_HIDDEN);

    return overlay;
}

void ui_wifi_page_open(void)
{
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(connect_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(spinner_obj, LV_OBJ_FLAG_HIDDEN);
    lv_textarea_set_text(ssid_ta, "");
    lv_textarea_set_text(password_ta, "");
}

void ui_wifi_page_close(void)
{
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
}
