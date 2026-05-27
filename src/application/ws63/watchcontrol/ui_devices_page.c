#include "ui_devices_page.h"
#include "ui_style.h"

#define SWIPE_THRESHOLD 40

static lv_obj_t *page_root;
static lv_obj_t *device_list;
static lv_obj_t *idle_count_label, *borrowed_count_label;
static int device_count_local = 0;
static ui_device_t devices_local[MAX_DEVICES];

typedef struct {
    lv_obj_t *container;
    lv_obj_t *card;
    lv_obj_t *delete_btn;
    lv_obj_t *status_tag;
    lv_obj_t *action_btn;
    lv_obj_t *conn_dot;
    lv_obj_t *name_label;
    int index;
    lv_coord_t start_x;
    bool delete_revealed;
} device_item_t;

static device_item_t items[MAX_DEVICES];

static const char *status_text(int status)
{
    switch (status) {
        case 0: return "空闲";
        case 1: return "已借出";
        case 2: return "申请中";
        default: return "未知";
    }
}

static lv_color_t status_color(int status)
{
    switch (status) {
        case 0: return COLOR_GREEN;
        case 1: return COLOR_YELLOW;
        case 2: return COLOR_CYAN;
        default: return COLOR_TEXT_DIM;
    }
}

static void show_delete_confirm(int idx);
static void show_borrow_dialog(int idx);
static void show_return_dialog(int idx);

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

static void card_pressing_cb(lv_event_t *e)
{
    lv_obj_t *card = lv_event_get_target(e);
    int idx = -1;
    for (int i = 0; i < device_count_local; i++) {
        if (items[i].card == card) { idx = i; break; }
    }
    if (idx < 0) return;

    lv_indev_t *indev = lv_indev_active();
    lv_point_t vect;
    lv_indev_get_vect(indev, &vect);

    if (vect.x < -2) {
        lv_coord_t cur_x = lv_obj_get_x(card);
        lv_coord_t new_x = LV_MAX(-60, cur_x + vect.x);
        lv_obj_set_x(card, new_x);
    }
}

static void card_release_cb(lv_event_t *e)
{
    lv_obj_t *card = lv_event_get_target(e);
    int idx = -1;
    for (int i = 0; i < device_count_local; i++) {
        if (items[i].card == card) { idx = i; break; }
    }
    if (idx < 0) return;

    lv_coord_t cur_x = lv_obj_get_x(card);
    if (cur_x < -SWIPE_THRESHOLD) {
        lv_obj_set_x(card, -60);
        items[idx].delete_revealed = true;
    } else {
        lv_obj_set_x(card, 0);
        items[idx].delete_revealed = false;
    }
}

static void delete_btn_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    show_delete_confirm(idx);
}

static void action_btn_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (devices_local[idx].status == 0) {
        show_borrow_dialog(idx);
    } else if (devices_local[idx].status == 1) {
        show_return_dialog(idx);
    }
}

static void msgbox_close_cb(lv_event_t *e)
{
    lv_obj_t *mbox = lv_event_get_current_target(e);
    lv_msgbox_close(mbox);
}

static void delete_confirm_cb(lv_event_t *e)
{
    lv_obj_t *mbox = lv_event_get_current_target(e);
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    lv_msgbox_close(mbox);
    ui_devices_page_remove_device(idx);
}

static void show_delete_confirm(int idx)
{
    static char buf[128];
    lv_snprintf(buf, sizeof(buf), "确认删除设备？\n%s\n%s",
                devices_local[idx].name, devices_local[idx].mac);

    lv_obj_t *mbox = lv_msgbox_create(NULL);
    lv_msgbox_add_text(mbox, buf);
    lv_obj_set_style_text_font(mbox, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(mbox, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_bg_color(mbox, COLOR_CARD, 0);

    lv_obj_t *btn_cancel = lv_msgbox_add_footer_button(mbox, "取消");
    lv_obj_add_event_cb(btn_cancel, msgbox_close_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_confirm = lv_msgbox_add_footer_button(mbox, "确认删除");
    lv_obj_set_style_bg_color(btn_confirm, COLOR_RED, 0);
    lv_obj_add_event_cb(btn_confirm, delete_confirm_cb, LV_EVENT_CLICKED,
                        (void *)(intptr_t)idx);
}

static void borrow_confirm_cb(lv_event_t *e)
{
    lv_obj_t *mbox = lv_event_get_current_target(e);
    lv_msgbox_close(mbox);
}

static void show_borrow_dialog(int idx)
{
    static char buf[128];
    lv_snprintf(buf, sizeof(buf), "申请借用设备\n%s\n%s",
                devices_local[idx].name, devices_local[idx].mac);

    lv_obj_t *mbox = lv_msgbox_create(NULL);
    lv_msgbox_add_text(mbox, buf);
    lv_obj_set_style_text_font(mbox, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(mbox, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_bg_color(mbox, COLOR_CARD, 0);

    lv_obj_t *content = lv_msgbox_get_content(mbox);
    lv_obj_t *ta = lv_textarea_create(content);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_placeholder_text(ta, "输入借用人");
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_12, 0);
    lv_obj_add_style(ta, &style_input, 0);
    lv_obj_set_width(ta, LV_PCT(100));

    lv_obj_t *btn_cancel = lv_msgbox_add_footer_button(mbox, "取消");
    lv_obj_add_event_cb(btn_cancel, msgbox_close_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_confirm = lv_msgbox_add_footer_button(mbox, "确认申请");
    lv_obj_set_style_bg_color(btn_confirm, COLOR_GREEN, 0);
    lv_obj_add_event_cb(btn_confirm, borrow_confirm_cb, LV_EVENT_CLICKED,
                        (void *)(intptr_t)idx);
}

static void return_confirm_cb(lv_event_t *e)
{
    lv_obj_t *mbox = lv_event_get_current_target(e);
    lv_msgbox_close(mbox);
}

static void show_return_dialog(int idx)
{
    static char buf[128];
    lv_snprintf(buf, sizeof(buf), "确认返还设备？\n%s\n%s\n借用人: %s",
                devices_local[idx].name, devices_local[idx].mac,
                devices_local[idx].borrowed_by);

    lv_obj_t *mbox = lv_msgbox_create(NULL);
    lv_msgbox_add_text(mbox, buf);
    lv_obj_set_style_text_font(mbox, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(mbox, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_bg_color(mbox, COLOR_CARD, 0);

    lv_obj_t *btn_cancel = lv_msgbox_add_footer_button(mbox, "取消");
    lv_obj_add_event_cb(btn_cancel, msgbox_close_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_confirm = lv_msgbox_add_footer_button(mbox, "确认返还");
    lv_obj_set_style_bg_color(btn_confirm, COLOR_YELLOW, 0);
    lv_obj_add_event_cb(btn_confirm, return_confirm_cb, LV_EVENT_CLICKED,
                        (void *)(intptr_t)idx);
}

static void add_btn_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    extern void ui_set_sle_page_cb(ui_event_cb_t cb);
}

static void create_device_item(int idx)
{
    ui_device_t *dev = &devices_local[idx];

    lv_obj_t *container = lv_obj_create(device_list);
    lv_obj_set_size(container, LV_PCT(100), 52);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(container, 0, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_bg_opa(container, 0, 0);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(container, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    lv_obj_t *delete_btn = lv_btn_create(container);
    lv_obj_set_size(delete_btn, 56, 44);
    lv_obj_set_style_bg_color(delete_btn, COLOR_RED, 0);
    lv_obj_set_style_radius(delete_btn, 4, 0);
    lv_obj_align(delete_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(delete_btn, delete_btn_cb, LV_EVENT_CLICKED,
                        (void *)(intptr_t)idx);
    lv_obj_t *del_label = lv_label_create(delete_btn);
    lv_label_set_text(del_label, "删除");
    lv_obj_set_style_text_font(del_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(del_label, lv_color_white(), 0);
    lv_obj_center(del_label);

    lv_obj_t *card = lv_obj_create(container);
    lv_obj_add_style(card, &style_card, 0);
    lv_obj_set_size(card, LV_PCT(100), 48);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(card, 8, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(card, card_pressing_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(card, card_release_cb, LV_EVENT_RELEASED, NULL);

    lv_obj_t *icon = lv_obj_create(card);
    lv_obj_set_size(icon, 20, 20);
    lv_obj_set_style_bg_color(icon, COLOR_CYAN, 0);
    lv_obj_set_style_radius(icon, 4, 0);
    lv_obj_set_style_border_width(icon, 0, 0);
    lv_obj_set_style_pad_all(icon, 0, 0);
    lv_obj_clear_flag(icon, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *name_label = lv_label_create(card);
    lv_label_set_text(name_label, dev->name);
    lv_obj_set_style_text_font(name_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(name_label, COLOR_TEXT_PRIMARY, 0);

    lv_obj_t *status_tag = lv_label_create(card);
    lv_label_set_text(status_tag, status_text(dev->status));
    lv_obj_set_style_text_font(status_tag, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(status_tag, status_color(dev->status), 0);

    lv_obj_t *conn_dot = create_status_dot(card,
        dev->connected ? COLOR_GREEN : COLOR_RED);

    lv_obj_t *spacer = lv_obj_create(card);
    lv_obj_set_flex_grow(spacer, 1);
    lv_obj_set_size(spacer, 0, 0);
    lv_obj_set_style_border_width(spacer, 0, 0);
    lv_obj_set_style_bg_opa(spacer, 0, 0);
    lv_obj_clear_flag(spacer, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *action_btn = NULL;
    if (dev->status == 0 && dev->connected) {
        action_btn = lv_btn_create(card);
        lv_obj_set_size(action_btn, 48, 28);
        lv_obj_add_style(action_btn, &style_btn_green, 0);
        lv_obj_add_event_cb(action_btn, action_btn_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)idx);
        lv_obj_t *btn_lbl = lv_label_create(action_btn);
        lv_label_set_text(btn_lbl, "申请");
        lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_12, 0);
        lv_obj_center(btn_lbl);
    } else if (dev->status == 1) {
        action_btn = lv_btn_create(card);
        lv_obj_set_size(action_btn, 48, 28);
        lv_obj_add_style(action_btn, &style_btn_yellow, 0);
        lv_obj_add_event_cb(action_btn, action_btn_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)idx);
        lv_obj_t *btn_lbl = lv_label_create(action_btn);
        lv_label_set_text(btn_lbl, "返还");
        lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_12, 0);
        lv_obj_center(btn_lbl);
    }

    items[idx].container = container;
    items[idx].card = card;
    items[idx].delete_btn = delete_btn;
    items[idx].status_tag = status_tag;
    items[idx].action_btn = action_btn;
    items[idx].conn_dot = conn_dot;
    items[idx].name_label = name_label;
    items[idx].index = idx;
    items[idx].delete_revealed = false;
}

static void update_counts(void)
{
    int idle = 0, borrowed = 0;
    for (int i = 0; i < device_count_local; i++) {
        if (devices_local[i].status == 0) idle++;
        else if (devices_local[i].status == 1) borrowed++;
    }
    lv_label_set_text_fmt(idle_count_label, "空闲:%d", idle);
    lv_label_set_text_fmt(borrowed_count_label, "借出:%d", borrowed);
}

lv_obj_t *ui_devices_page_create(lv_obj_t *parent)
{
    page_root = lv_obj_create(parent);
    lv_obj_set_size(page_root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(page_root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(page_root, 6, 0);
    lv_obj_set_style_pad_row(page_root, 6, 0);
    lv_obj_set_style_bg_color(page_root, COLOR_BG, 0);
    lv_obj_set_style_border_width(page_root, 0, 0);

    lv_obj_t *header = lv_obj_create(page_root);
    lv_obj_set_size(header, LV_PCT(100), 36);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(header, 8, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_bg_opa(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "设备列表");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, COLOR_TEXT_PRIMARY, 0);

    idle_count_label = lv_label_create(header);
    lv_label_set_text(idle_count_label, "空闲:0");
    lv_obj_set_style_text_font(idle_count_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(idle_count_label, COLOR_GREEN, 0);

    borrowed_count_label = lv_label_create(header);
    lv_label_set_text(borrowed_count_label, "借出:0");
    lv_obj_set_style_text_font(borrowed_count_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(borrowed_count_label, COLOR_YELLOW, 0);

    lv_obj_t *spacer = lv_obj_create(header);
    lv_obj_set_flex_grow(spacer, 1);
    lv_obj_set_size(spacer, 0, 0);
    lv_obj_set_style_border_width(spacer, 0, 0);
    lv_obj_set_style_bg_opa(spacer, 0, 0);
    lv_obj_clear_flag(spacer, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *add_btn = lv_btn_create(header);
    lv_obj_set_size(add_btn, 32, 28);
    lv_obj_add_style(add_btn, &style_btn_green, 0);
    lv_obj_add_event_cb(add_btn, add_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *add_lbl = lv_label_create(add_btn);
    lv_label_set_text(add_lbl, "+");
    lv_obj_set_style_text_font(add_lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(add_lbl);

    device_list = lv_obj_create(page_root);
    lv_obj_set_flex_flow(device_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(device_list, 4, 0);
    lv_obj_set_style_border_width(device_list, 0, 0);
    lv_obj_set_style_bg_opa(device_list, 0, 0);
    lv_obj_set_style_pad_all(device_list, 0, 0);
    lv_obj_set_flex_grow(device_list, 1);
    lv_obj_set_width(device_list, LV_PCT(100));
    lv_obj_set_scroll_dir(device_list, LV_DIR_VER);
    lv_obj_add_flag(device_list, LV_OBJ_FLAG_SCROLL_CHAIN);

    return page_root;
}

void ui_devices_page_update(const ui_device_t *devices, int device_count)
{
    if (!devices) return;

    lv_obj_clean(device_list);
    device_count_local = 0;

    int count = device_count > MAX_DEVICES ? MAX_DEVICES : device_count;
    for (int i = 0; i < count; i++) {
        devices_local[i] = devices[i];
        device_count_local++;
        create_device_item(i);
    }
    update_counts();
}

void ui_devices_page_add_device(const ui_device_t *device)
{
    if (!device || device_count_local >= MAX_DEVICES) return;
    devices_local[device_count_local] = *device;
    create_device_item(device_count_local);
    device_count_local++;
    update_counts();
}

void ui_devices_page_remove_device(int index)
{
    if (index < 0 || index >= device_count_local) return;

    lv_obj_del(items[index].container);

    for (int i = index; i < device_count_local - 1; i++) {
        devices_local[i] = devices_local[i + 1];
        items[i] = items[i + 1];
        items[i].index = i;
    }
    device_count_local--;
    update_counts();
}
