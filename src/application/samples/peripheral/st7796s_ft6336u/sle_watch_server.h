#ifndef SLE_WATCH_SERVER_H
#define SLE_WATCH_SERVER_H

#include "errcode.h"

void sle_watch_server_start(void);
errcode_t sle_watch_server_send(const uint8_t *data, uint16_t len);

#endif
