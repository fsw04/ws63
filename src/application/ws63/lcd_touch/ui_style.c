#include "ui_style.h"

lv_style_t style_card;
lv_style_t style_btn_green;
lv_style_t style_btn_red;
lv_style_t style_btn_yellow;
lv_style_t style_input;

static lv_style_t style_status_dot_online;
static lv_style_t style_status_dot_offline;

void ui_style_init(void)
{
    lv_style_init(&style_card);
    lv_style_set_bg_color(&style_card, COLOR_CARD);
    lv_style_set_bg_opa(&style_card, LV_OPA_COVER);
    lv_style_set_border_color(&style_card, COLOR_CARD_BORDER);
    lv_style_set_border_width(&style_card, 1);
    lv_style_set_radius(&style_card, 8);
    lv_style_set_pad_all(&style_card, 8);
    lv_style_set_pad_row(&style_card, 4);
    lv_style_set_pad_column(&style_card, 8);

    lv_style_init(&style_btn_green);
    lv_style_set_bg_color(&style_btn_green, COLOR_GREEN);
    lv_style_set_bg_opa(&style_btn_green, LV_OPA_COVER);
    lv_style_set_radius(&style_btn_green, 6);
    lv_style_set_text_color(&style_btn_green, COLOR_BG);
    lv_style_set_text_font(&style_btn_green, &lv_font_montserrat_12);
    lv_style_set_pad_ver(&style_btn_green, 6);
    lv_style_set_pad_hor(&style_btn_green, 12);

    lv_style_init(&style_btn_red);
    lv_style_set_bg_color(&style_btn_red, COLOR_RED);
    lv_style_set_bg_opa(&style_btn_red, LV_OPA_COVER);
    lv_style_set_radius(&style_btn_red, 6);
    lv_style_set_text_color(&style_btn_red, COLOR_BG);
    lv_style_set_text_font(&style_btn_red, &lv_font_montserrat_12);
    lv_style_set_pad_ver(&style_btn_red, 6);
    lv_style_set_pad_hor(&style_btn_red, 12);

    lv_style_init(&style_btn_yellow);
    lv_style_set_bg_color(&style_btn_yellow, COLOR_YELLOW);
    lv_style_set_bg_opa(&style_btn_yellow, LV_OPA_COVER);
    lv_style_set_radius(&style_btn_yellow, 6);
    lv_style_set_text_color(&style_btn_yellow, COLOR_BG);
    lv_style_set_text_font(&style_btn_yellow, &lv_font_montserrat_12);
    lv_style_set_pad_ver(&style_btn_yellow, 6);
    lv_style_set_pad_hor(&style_btn_yellow, 12);

    lv_style_init(&style_input);
    lv_style_set_bg_color(&style_input, lv_color_hex(0x1A2035));
    lv_style_set_bg_opa(&style_input, LV_OPA_COVER);
    lv_style_set_border_color(&style_input, COLOR_CARD_BORDER);
    lv_style_set_border_width(&style_input, 1);
    lv_style_set_radius(&style_input, 6);
    lv_style_set_text_color(&style_input, COLOR_TEXT_PRIMARY);
    lv_style_set_text_font(&style_input, &lv_font_montserrat_12);
    lv_style_set_pad_all(&style_input, 6);
}

lv_style_t *ui_style_card(void) { return &style_card; }
lv_style_t *ui_style_btn_green(void) { return &style_btn_green; }
lv_style_t *ui_style_btn_red(void) { return &style_btn_red; }
lv_style_t *ui_style_btn_yellow(void) { return &style_btn_yellow; }
lv_style_t *ui_style_input(void) { return &style_input; }
lv_style_t *ui_style_status_dot(bool online) { return online ? &style_status_dot_online : &style_status_dot_offline; }
lv_style_t *ui_style_tab_btn(bool active)
{
    static lv_style_t style_tab_active;
    static lv_style_t style_tab_inactive;
    static bool inited = false;
    if (!inited) {
        lv_style_init(&style_tab_active);
        lv_style_set_text_color(&style_tab_active, COLOR_GREEN);
        lv_style_set_text_font(&style_tab_active, &lv_font_montserrat_12);
        lv_style_init(&style_tab_inactive);
        lv_style_set_text_color(&style_tab_inactive, COLOR_TEXT_DIM);
        lv_style_set_text_font(&style_tab_inactive, &lv_font_montserrat_12);
        inited = true;
    }
    return active ? &style_tab_active : &style_tab_inactive;
}
