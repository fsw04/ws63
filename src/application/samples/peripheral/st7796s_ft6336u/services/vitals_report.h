#ifndef VITALS_REPORT_H
#define VITALS_REPORT_H

#include <stdint.h>
#include "errcode.h"

#define VITALS_REPORT_DEVICE_UNKNOWN 0xff

void vitals_report_start(void);
void vitals_report_handle(uint8_t device_index, const uint8_t *data, uint16_t len);
errcode_t vitals_report_submit_on_return(uint8_t device_index);
errcode_t vitals_report_cache_pending(const char *payload);
void vitals_report_flush_pending(void);

#endif
