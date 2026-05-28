#ifndef UI_SLE_PAGE_H
#define UI_SLE_PAGE_H

#include "lvgl.h"
#include "ui_common.h"

lv_obj_t *ui_sle_page_create(lv_obj_t *parent);
void ui_sle_page_open(void);
void ui_sle_page_close(void);
void ui_sle_page_add_discovered(const ui_discovered_t *device);
void ui_sle_page_set_scanning(bool scanning);

#endif
