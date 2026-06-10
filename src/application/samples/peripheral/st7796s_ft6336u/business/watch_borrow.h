#ifndef WATCH_BORROW_H
#define WATCH_BORROW_H

#include <stdint.h>
#include "errcode.h"

errcode_t watch_borrow_request(uint8_t device_index);
errcode_t watch_borrow_return(uint8_t device_index);
uint8_t watch_borrow_is_waiting_card(void);
void watch_borrow_on_identity_received(const char *name, const char *id_number);
void watch_borrow_on_identity_ack(const char *name, const char *id_number);
void watch_borrow_on_unbound(void);
void watch_borrow_make_masked_id(const char *id_number, char *out, uint32_t out_len);
void watch_borrow_simulate_card(void);

#endif
