#ifndef UI_DEVICES_PAGE_H
#define UI_DEVICES_PAGE_H

#include "lvgl.h"
#include "ui_common.h"

lv_obj_t *ui_devices_page_create(lv_obj_t *parent);
void ui_devices_page_update(const ui_device_t *devices, int device_count);
void ui_devices_page_add_device(const ui_device_t *device);
void ui_devices_page_remove_device(int index);

#endif