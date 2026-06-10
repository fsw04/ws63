#ifndef MQTT_TASK_H
#define MQTT_TASK_H

#include <stdbool.h>
#include "errcode.h"

extern unsigned long g_mqtt_msg_queue;

void mqtt_task_start(void);
void mqtt_task_stop(void);
bool mqtt_task_is_connected(void);
errcode_t mqtt_task_enqueue_report(const char *payload);
errcode_t mqtt_task_publish(const char *payload);

#endif
