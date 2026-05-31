#include "display.h"

#include "app_init.h"
#include "soc_osal.h"

static timer_handle_t g_timer_handle = NULL;
static uint8_t g_lvgl_buf[ST7796S_WIDTH * ST7796S_FT6336U_RENDER_ROWS * ST7796S_FT6336U_BYTE_PER_PIXEL];
static lv_point_t g_last_touch_point;
static uint8_t g_touch_ready = 0;

static void lvgl_tick_callback(uintptr_t data)
{
    (void)data;
    lv_tick_inc(1);
    uapi_timer_start(g_timer_handle, ST7796S_FT6336U_TICK_US, lvgl_tick_callback, 0);
}

static void display_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    uint32_t width = area->x2 - area->x1 + 1;
    uint32_t height = area->y2 - area->y1 + 1;

    st7796s_set_window(area->x1, area->y1, area->x2, area->y2);
    st7796s_write_pixels(px_map, width * height * ST7796S_FT6336U_BYTE_PER_PIXEL);
    lv_display_flush_ready(disp);
}

static int16_t clamp_coord(int16_t value, int16_t max)
{
    if (value < 0) {
        return 0;
    }

    if (value > max) {
        return max;
    }

    return value;
}

static void touch_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    ft6336u_point_t point;
    uint8_t points = g_touch_ready ? ft6336u_read_points(&point, 1) : 0;

    if (points > 0) {
        g_last_touch_point.x = clamp_coord(point.x, ST7796S_WIDTH - 1);
        g_last_touch_point.y = clamp_coord(point.y, ST7796S_HEIGHT - 1);
        data->point = g_last_touch_point;
        data->state = LV_INDEV_STATE_PRESSED;
        osal_printk("touch points=%u x=%u y=%u event=%u\r\n", points, point.x, point.y, point.event);
    } else {
        data->point = g_last_touch_point;
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void lvgl_timer_init(void)
{
    uapi_timer_init();
    uapi_timer_adapter(ST7796S_FT6336U_TIMER_INDEX, TIMER_1_IRQN, ST7796S_FT6336U_TIMER_PRIORITY);
    uapi_timer_create(ST7796S_FT6336U_TIMER_INDEX, &g_timer_handle);
    uapi_timer_start(g_timer_handle, ST7796S_FT6336U_TICK_US, lvgl_tick_callback, 0);
}

static void create_demo_ui(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_t *label = lv_label_create(screen);
    lv_label_set_text(label, "ST7796S + FT6336U");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 32);

    lv_obj_t *slider = lv_slider_create(screen);
    lv_obj_set_width(slider, 240);
    lv_obj_align(slider, LV_ALIGN_CENTER, 0, -20);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, 45, LV_ANIM_OFF);

    lv_obj_t *button = lv_button_create(screen);
    lv_obj_set_size(button, 160, 56);
    lv_obj_align(button, LV_ALIGN_CENTER, 0, 70);

    lv_obj_t *button_label = lv_label_create(button);
    lv_label_set_text(button_label, "Touch");
    lv_obj_center(button_label);
}

static void *st7796s_ft6336u_task(const char *arg)
{
    (void)arg;

    osal_printk("st7796s_ft6336u task start\r\n");

    if (st7796s_init() != ERRCODE_SUCC) {
        osal_printk("st7796s init failed\r\n");
        return NULL;
    }
    osal_printk("st7796s init ok\r\n");

    osal_printk("st7796s color test start\r\n");
    st7796s_fill_color(0xF800);
    osal_msleep(200);
    st7796s_fill_color(0x07E0);
    osal_msleep(200);
    st7796s_fill_color(0x001F);
    osal_msleep(200);
    st7796s_fill_color(0xFFFF);
    osal_msleep(200);
    st7796s_fill_color(0x0000);
    osal_printk("st7796s color test done\r\n");

    if (ft6336u_init() != ERRCODE_SUCC) {
        osal_printk("ft6336u init failed, continue without touch\r\n");
    } else {
        g_touch_ready = 1;
        osal_printk("ft6336u init ok\r\n");
    }

    lv_init();

    lv_display_t *disp = lv_display_create(ST7796S_WIDTH, ST7796S_HEIGHT);
#if defined(CONFIG_ST7796S_LVGL_RGB565_SWAPPED) && (CONFIG_ST7796S_LVGL_RGB565_SWAPPED == 1)
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565_SWAPPED);
#else
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
#endif
    lv_display_set_flush_cb(disp, display_flush);
    lv_display_set_buffers(disp, g_lvgl_buf, NULL, sizeof(g_lvgl_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(indev, disp);
    lv_indev_set_read_cb(indev, touch_read);

    lvgl_timer_init();
    create_demo_ui();
    osal_printk("lvgl display ready\r\n");

    while (1) {
        lv_timer_handler();
        osal_msleep(ST7796S_FT6336U_TASK_DURATION_MS);
    }

    return NULL;
}

static void st7796s_ft6336u_entry(void)
{
    osal_task *task_handle = NULL;
    osal_printk("st7796s_ft6336u entry\r\n");
    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)st7796s_ft6336u_task, 0, "St7796sFt6336uTask",
                                      ST7796S_FT6336U_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, ST7796S_FT6336U_TASK_PRIORITY);
        osal_kfree(task_handle);
    } else {
        osal_printk("st7796s_ft6336u task create failed\r\n");
    }
    osal_kthread_unlock();
}

app_run(st7796s_ft6336u_entry);
