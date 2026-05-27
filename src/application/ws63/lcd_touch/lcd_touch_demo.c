#include "pinctrl.h"
#include "watchdog.h"
#include "soc_osal.h"
#include "app_init.h"
#include "gpio.h"
#include "systick.h"
#include "timer.h"
#include "tcxo.h"
#include "chip_core_irq.h"

#include "lcd.h"
#include "ft6336.h"

#define FRAME_RATE                  30
#define FRAME_TIME_MS               (uint32_t)(1000 / FRAME_RATE)

#define TASK_STACK_SIZE             0x1200
#define TASK_PRIO                   24
#define TIMER_TASK_PRIO             25

#define TIMER_INDEX                 1
#define TIMER_PRIO                  1
#define TIMER_DELAY_INT             5
#define TIMER1_DELAY_FPS            (FRAME_TIME_MS * 1000)
#define TIMER_MS_2_US               1000

typedef struct timer_info {
    uint32_t start_time;
    uint32_t end_time;
    uint32_t delay_time;
} timer_info_t;

static timer_info_t g_timers_info = {0, 0, 1000};
static timer_handle_t timer1 = {0};
lv_display_t *lv_display;
static lv_indev_t *lv_touch_indev;

static void timer_timeout_callback(uintptr_t data)
{
    lv_tick_inc(1);
    uapi_timer_start(timer1, g_timers_info.delay_time, timer_timeout_callback, (uint32_t)data);
}

static int *timer_task(const char *arg)
{
    unused(arg);
    uapi_timer_init();
    uapi_timer_adapter(TIMER_INDEX, TIMER_1_IRQN, TIMER_PRIO);

    uapi_timer_create(TIMER_INDEX, &timer1);
    g_timers_info.start_time = uapi_tcxo_get_ms();
    uapi_timer_start(timer1, g_timers_info.delay_time, timer_timeout_callback, 0);

    while (1) {
        osal_msleep(TIMER_DELAY_INT);
    }

    return 0;
}

static uint32_t g_flush_count = 0;

static void disp_flush(lv_display_t *disp_drv, const lv_area_t *area, uint8_t *px_map)
{
    if (g_flush_count < 5) {
        osal_printk("disp_flush: (%d,%d)-(%d,%d) pixel_bytes=%d\r\n",
            area->x1, area->y1, area->x2, area->y2,
            (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1) * 2);
    }
    g_flush_count++;

    lcd_set_windows(area->x1, area->y1, area->x2, area->y2);
    lcd_diplay(px_map, (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1) * 2);
    lv_display_flush_ready(disp_drv);
}

static void touch_read_callback(lv_indev_t *indev, lv_indev_data_t *data)
{
    unused(indev);
    static ft6336_touch_data_t touch_data = {0};

    if (ft6336_read_touch(&touch_data) && touch_data.touch_count > 0) {
        data->point.x = touch_data.points[0].x;
        data->point.y = touch_data.points[0].y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
    data->continue_reading = false;
}

static void create_demo_ui(void)
{
    lv_obj_t *label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "Hello World");
    lv_obj_set_style_text_color(label, lv_color_hex(0x0000FF), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_center(label);
}

static int lcd_touch_task(const char *arg)
{
    unused(arg);
    uapi_systick_init();
    uapi_tcxo_init();

    osal_printk("Initializing LCD...\r\n");
    lcd_init();
    osal_printk("LCD init done\r\n");

    osal_printk("Initializing FT6336 touch...\r\n");
    ft6336_init();
    osal_printk("FT6336 init done\r\n");

    osal_printk("Initializing LVGL...\r\n");
    lv_init();

    lv_display = lv_display_create(LCD_W, LCD_H);
    lv_display_set_flush_cb(lv_display, disp_flush);
    lv_display_set_buffers(lv_display, g_lcd_buf, g_lcd_buf2,
                           sizeof(g_lcd_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_touch_indev = lv_indev_create();
    lv_indev_set_type(lv_touch_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(lv_touch_indev, touch_read_callback);

    create_demo_ui();

    osal_printk("Direct LCD test: filling red rectangle at (0,0)-(159,99)...\r\n");
    lcd_set_windows(0, 0, 159, 99);
    uint16_t x, y;
    for (y = 0; y < 100; y++) {
        for (x = 0; x < 160; x++) {
            lcd_wr_data(0xF8);
            lcd_wr_data(0x00);
        }
    }
    osal_printk("Direct LCD test done.\r\n");

    osal_printk("ST7796 + FT6336 LCD Touch demo started!\r\n");

    uint32_t loop_count = 0;

    while (1) {
        lv_timer_handler();
        osal_msleep(1);
        loop_count++;
        if (loop_count % 3000 == 0) {
            osal_printk("LVGL running, flush_count=%d\r\n", g_flush_count);
        }
    }

    return 0;
}

static void lcd_touch_task_entry(void)
{
    osal_task *task_handle = NULL;

    osal_printk("================== ST7796 + FT6336 LCD Touch sample start =================\n");
    osal_kthread_lock();

    task_handle = osal_kthread_create((osal_kthread_handler)lcd_touch_task, 0,
                                      "LcdTouchTask", TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, TASK_PRIO);
        osal_kfree(task_handle);
    }

    task_handle = osal_kthread_create((osal_kthread_handler)timer_task, 0,
                                      "TimerTask", TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, TIMER_TASK_PRIO);
        osal_kfree(task_handle);
    }

    osal_kthread_unlock();
}

app_run(lcd_touch_task_entry);
