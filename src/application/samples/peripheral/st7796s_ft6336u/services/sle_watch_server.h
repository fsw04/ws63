#ifndef SLE_WATCH_SERVER_H
#define SLE_WATCH_SERVER_H

#include <stdint.h>
#include "errcode.h"

void sle_watch_server_start(void);
errcode_t sle_watch_server_send(const uint8_t *data, uint16_t len);
errcode_t sle_watch_send_to_device(uint8_t device_index, const uint8_t *data, uint16_t len);
errcode_t sle_watch_send_identity_to_device(uint8_t device_index, const char *name, const char *id_number);
errcode_t sle_watch_send_unbind_to_device(uint8_t device_index);
errcode_t sle_watch_scan_start(void);
errcode_t sle_watch_scan_stop(void);
errcode_t sle_watch_connect_scan_result(uint8_t scan_index);

#endif
