#ifndef MQTT_TASK_H
#define MQTT_TASK_H

// 暴露全局消息队列句柄，供传感器采集任务写入数据
extern unsigned long g_mqtt_msg_queue;

void mqtt_task_start(void);

#endif // MQTT_TASK_H