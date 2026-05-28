#include "ui_sle_page.h"
#include "ui_style.h"

static lv_obj_t *overlay;
static lv_obj_t *panel;
static lv_obj_t *spinner_obj;
static lv_obj_t *refresh_btn;
static lv_obj_t *device_list;
static lv_obj_t *count_label;

typedef struct {
    lv_obj_t *card;
    lv_obj_t *connect_btn;
    bool added;
} discovered_item_t;

static discovered_item_t discovered_items[MAX_DISCOVERED];
static int discovered_count = 0;

static lv_obj_t *create_signal_bars(lv_obj_t *parent, int rssi)
{
    lv_obj_t *bars = lv_obj_create(parent);
    lv_obj_set_size(bars, 24, 16);
    lv_obj_set_flex_flow(bars, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bars, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(bars, 2, 0);
    lv_obj_set_style_border_width(bars, 0, 0);
    lv_obj_set_style_bg_opa(bars, 0, 0);
    lv_obj_set_style_pad_all(bars, 0, 0);
    lv_obj_clear_flag(bars, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    int level;
    if (rssi > -50) level = 3;
    else if (rssi > -70) level = 2;
    else level = 1;

    int heights[] = {6, 10, 14};
    lv_color_t colors[] = {COLOR_GREEN, COLOR_YELLOW, COLOR_RED};

    for (int i = 0; i < 3; i++) {
        lv_obj_t *bar = lv_obj_create(bars);
        lv_obj_set_size(bar, 5, heights[i]);
        lv_obj_set_style_bg_color(bar, (i < level) ? colors[level - 1] : COLOR_TEXT_DIM, 0);
        lv_obj_set_style_bg_opa(bar, (i < level) ? LV_OPA_COVER : LV_OPA_30, 0);
        lv_obj_set_style_radius(bar, 1, 0);
        lv_obj_set_style_border_width(bar, 0, 0);
        lv_obj_set_style_pad_all(bar, 0, 0);
        lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    }

    return bars;
}

static void close_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_sle_page_close();
}

static void connect_device_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx >= 0 && idx < discovered_count) {
        discovered_items[idx].added = true;
        lv_obj_add_flag(discovered_items[idx].connect_btn, LV_OBJ_FLAG_HIDDEN);
    }
}

static void refresh_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    discovered_count = 0;
    lv_obj_clean(device_list);
}

static void create_discovered_item(int idx, const ui_discovered_t *dev)
{
    lv_obj_t *card = lv_obj_create(device_list);
    lv_obj_add_style(card, &style_card, 0);
    lv_obj_set_size(card, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(card, 8, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *info = lv_obj_create(card);
    lv_obj_set_size(info, LV_PCT(1), LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(info, 1);
    lv_obj_set_flex_flow(info, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(info, 2, 0);
    lv_obj_set_style_border_width(info, 0, 0);
    lv_obj_set_style_bg_opa(info, 0, 0);
    lv_obj_set_style_pad_all(info, 0, 0);
    lv_obj_clear_flag(info, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *name_label = lv_label_create(info);
    lv_label_set_text(name_label, dev->name[0] ? dev->name : "未知设备");
    lv_obj_set_style_text_font(name_label, FONT_CN_14, 0);
    lv_obj_set_style_text_color(name_label, COLOR_TEXT_PRIMARY, 0);

    lv_obj_t *mac_label = lv_label_create(info);
    lv_label_set_text(mac_label, dev->mac);
    lv_obj_set_style_text_font(mac_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(mac_label, COLOR_TEXT_SECONDARY, 0);

    create_signal_bars(card, dev->rssi);

    lv_obj_t *connect_btn = lv_btn_create(card);
    lv_obj_set_size(connect_btn, 52, 28);
    lv_obj_add_style(connect_btn, &style_btn_green, 0);
    lv_obj_add_event_cb(connect_btn, connect_device_cb, LV_EVENT_CLICKED,
                        (void *)(intptr_t)idx);
    lv_obj_t *btn_lbl = lv_label_create(connect_btn);
    lv_label_set_text(btn_lbl, "连接");
    lv_obj_set_style_text_font(btn_lbl, FONT_CN_14, 0);
    lv_obj_center(btn_lbl);

    discovered_items[idx].card = card;
    discovered_items[idx].connect_btn = connect_btn;
    discovered_items[idx].added = false;
}

lv_obj_t *ui_sle_page_create(lv_obj_t *parent)
{
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
    lv_obj_set_style_max_height(panel, LV_PCT(70), 0);
    lv_obj_set_style_bg_color(panel, COLOR_CARD, 0);
    lv_obj_set_style_radius(panel, 16, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 12, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(panel, 10, 0);
    lv_obj_align(panel, LV_ALIGN_BOTTOM_MID, 0, 0);
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
    lv_label_set_text(title, "SLE设备扫描");
    lv_obj_set_style_text_font(title, FONT_CN_16, 0);
    lv_obj_set_style_text_color(title, COLOR_TEXT_PRIMARY, 0);

    lv_obj_t *spacer = lv_obj_create(header);
    lv_obj_set_flex_grow(spacer, 1);
    lv_obj_set_size(spacer, 0, 0);
    lv_obj_set_style_border_width(spacer, 0, 0);
    lv_obj_set_style_bg_opa(spacer, 0, 0);
    lv_obj_clear_flag(spacer, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    refresh_btn = lv_btn_create(header);
    lv_obj_set_size(refresh_btn, 28, 28);
    lv_obj_set_style_bg_color(refresh_btn, COLOR_CARD, 0);
    lv_obj_set_style_radius(refresh_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_color(refresh_btn, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_border_width(refresh_btn, 1, 0);
    lv_obj_add_event_cb(refresh_btn, refresh_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *refresh_lbl = lv_label_create(refresh_btn);
    lv_label_set_text(refresh_lbl, LV_SYMBOL_REFRESH);
    lv_obj_set_style_text_font(refresh_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(refresh_lbl, COLOR_TEXT_SECONDARY, 0);
    lv_obj_center(refresh_lbl);

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

    spinner_obj = lv_spinner_create(panel);
    lv_obj_set_size(spinner_obj, 28, 28);
    lv_obj_set_style_arc_color(spinner_obj, COLOR_GREEN, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(spinner_obj, 3, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(spinner_obj, COLOR_TEXT_DIM, LV_PART_MAIN);
    lv_obj_set_style_arc_width(spinner_obj, 3, LV_PART_MAIN);
    lv_obj_align(spinner_obj, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(spinner_obj, LV_OBJ_FLAG_HIDDEN);

    device_list = lv_obj_create(panel);
    lv_obj_set_flex_flow(device_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(device_list, 4, 0);
    lv_obj_set_style_border_width(device_list, 0, 0);
    lv_obj_set_style_bg_opa(device_list, 0, 0);
    lv_obj_set_style_pad_all(device_list, 0, 0);
    lv_obj_set_width(device_list, LV_PCT(100));
    lv_obj_set_scroll_dir(device_list, LV_DIR_VER);
    lv_obj_add_flag(device_list, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_style_max_height(device_list, 240, 0);

    lv_obj_t *legend = lv_obj_create(panel);
    lv_obj_set_size(legend, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(legend, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(legend, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(legend, 12, 0);
    lv_obj_set_style_border_width(legend, 0, 0);
    lv_obj_set_style_bg_opa(legend, 0, 0);
    lv_obj_set_style_pad_all(legend, 0, 0);
    lv_obj_clear_flag(legend, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *leg_strong = lv_label_create(legend);
    lv_label_set_text(leg_strong, "强");
    lv_obj_set_style_text_font(leg_strong, FONT_CN_14, 0);
    lv_obj_set_style_text_color(leg_strong, COLOR_GREEN, 0);

    lv_obj_t *leg_mid = lv_label_create(legend);
    lv_label_set_text(leg_mid, "中");
    lv_obj_set_style_text_font(leg_mid, FONT_CN_14, 0);
    lv_obj_set_style_text_color(leg_mid, COLOR_YELLOW, 0);

    lv_obj_t *leg_weak = lv_label_create(legend);
    lv_label_set_text(leg_weak, "弱");
    lv_obj_set_style_text_font(leg_weak, FONT_CN_14, 0);
    lv_obj_set_style_text_color(leg_weak, COLOR_RED, 0);

    lv_obj_t *leg_spacer = lv_obj_create(legend);
    lv_obj_set_flex_grow(leg_spacer, 1);
    lv_obj_set_size(leg_spacer, 0, 0);
    lv_obj_set_style_border_width(leg_spacer, 0, 0);
    lv_obj_set_style_bg_opa(leg_spacer, 0, 0);
    lv_obj_clear_flag(leg_spacer, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    count_label = lv_label_create(legend);
    lv_label_set_text(count_label, "发现: 0");
    lv_obj_set_style_text_font(count_label, FONT_CN_14, 0);
    lv_obj_set_style_text_color(count_label, COLOR_TEXT_SECONDARY, 0);

    return overlay;
}

void ui_sle_page_open(void)
{
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_HIDDEN);
    ui_sle_page_set_scanning(true);
    discovered_count = 0;
    lv_obj_clean(device_list);
}

void ui_sle_page_close(void)
{
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
    ui_sle_page_set_scanning(false);
}

void ui_sle_page_add_discovered(const ui_discovered_t *device)
{
    if (!device || discovered_count >= MAX_DISCOVERED) return;
    create_discovered_item(discovered_count, device);
    discovered_count++;
    lv_label_set_text_fmt(count_label, "发现: %d", discovered_count);
}

void ui_sle_page_set_scanning(bool scanning)
{
    if (scanning) {
        lv_obj_clear_flag(spinner_obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(spinner_obj, LV_OBJ_FLAG_HIDDEN);
    }
}
