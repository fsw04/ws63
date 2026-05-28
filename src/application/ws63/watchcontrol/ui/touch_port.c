#include "touch_port.h"
#include "ft6336u.h"

static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;

    ft6336u_touch_t touch;
    bool pressed = ft6336u_read_touch(&touch);

    if (pressed && touch.count > 0) {
        data->point.x = touch.x[0];
        data->point.y = touch.y[0];
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }

    data->continue_reading = false;
}

void touch_port_init(void)
{
    ft6336u_init();

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cb);
}
