#ifndef UI_TASK_H
#define UI_TASK_H

#include "lvgl.h"
#include "ui_common.h"

void ui_task_start(void);
void ui_update_system_status(const ui_system_status_t *status);
void ui_update_devices(const ui_device_t *devices, int count);
void ui_add_discovered_device(const ui_discovered_t *device);
void ui_set_scanning(bool scanning);

#endif
