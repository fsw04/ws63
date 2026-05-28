#ifndef UI_WIFI_PAGE_H
#define UI_WIFI_PAGE_H

#include "lvgl.h"
#include "ui_common.h"

lv_obj_t *ui_wifi_page_create(lv_obj_t *parent);
void ui_wifi_page_open(void);
void ui_wifi_page_close(void);

#endif
