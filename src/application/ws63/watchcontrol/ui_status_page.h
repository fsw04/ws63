#ifndef UI_STATUS_PAGE_H
#define UI_STATUS_PAGE_H

#include "lvgl.h"
#include "ui_common.h"

lv_obj_t *ui_status_page_create(lv_obj_t *parent);
void ui_status_page_update(const ui_system_status_t *status, const ui_device_t *devices, int device_count);

#endif
