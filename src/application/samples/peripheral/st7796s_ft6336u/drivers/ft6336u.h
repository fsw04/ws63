#ifndef FT6336U_H
#define FT6336U_H

#include <stdint.h>
#include "errcode.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FT6336U_I2C_ADDR 0x38
#define FT6336U_MAX_POINTS 5

typedef struct {
    uint16_t x;
    uint16_t y;
    uint8_t event;
    uint8_t id;
} ft6336u_point_t;

errcode_t ft6336u_init(void);
uint8_t ft6336u_read_points(ft6336u_point_t *points, uint8_t max_points);

#ifdef __cplusplus
}
#endif

#endif
