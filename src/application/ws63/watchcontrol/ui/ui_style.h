#ifndef UI_STYLE_H
#define UI_STYLE_H

#include "lvgl.h"
#include "ui_common.h"

void ui_style_init(void);
lv_style_t *ui_style_card(void);
lv_style_t *ui_style_status_dot(bool online);
lv_style_t *ui_style_btn_green(void);
lv_style_t *ui_style_btn_red(void);
lv_style_t *ui_style_btn_yellow(void);
lv_style_t *ui_style_tab_btn(bool active);
lv_style_t *ui_style_input(void);

extern lv_style_t style_card;
extern lv_style_t style_btn_green;
extern lv_style_t style_btn_red;
extern lv_style_t style_btn_yellow;
extern lv_style_t style_input;

#endif
