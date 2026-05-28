#include "max30102.h"
#include "i2c.h"
#include "gpio.h"
#include "hal_gpio.h"
#include "pinctrl.h"
#include "osal_debug.h"
#include "soc_osal.h"

#define MAX30102_TASK_STACK_SIZE    0x5000
#define MAX30102_TASK_PRIO          31

#define MAX30102_SCL_PIN            GPIO_16
#define MAX30102_SDA_PIN            GPIO_15
#define MAX30102_INT_GPIO           GPIO_12

#define MAX30102_LOG(fmt, ...)    osal_printk("[MAX30102] " fmt "\r\n", ##__VA_ARGS__)

static uint32_t g_i2c_bus = MAX30102_I2C_BUS;
static max30102_int_callback_t g_int_callback = NULL;

/* ========== 延时函数：启动早期用忙等，之后用 osal_msleep ========== */

static void max30102_delay_ms_early(uint32_t ms)
{
    volatile uint32_t i, j;
    volatile uint32_t sum = 0;
    
    for (i = 0; i < ms; i++) {
        for (j = 0; j < 5000000; j++) {
            sum += j;
            sum ^= j >> 3;
        }
    }
    (void)sum;
}

/* ========== I2C 基础操作 ========== */

static int max30102_i2c_init(void)
{
    uint32_t ret;
    
    uapi_pin_set_mode(MAX30102_SCL_PIN, PIN_MODE_0);
    uapi_pin_set_mode(MAX30102_SDA_PIN, PIN_MODE_0);
    
    ret = uapi_i2c_master_init(g_i2c_bus, 400000, 0);
    if (ret != 0) {
        MAX30102_LOG("I2C master init failed, ret=%d", ret);
        return -1;
    }
    MAX30102_LOG("I2C init success");
    return 0;
}

int max30102_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t write_buf[2];
    i2c_data_t i2c_data = {0};
    uint32_t ret;
    
    write_buf[0] = reg;
    write_buf[1] = value;
    
    i2c_data.send_buf = write_buf;
    i2c_data.send_len = 2;
    i2c_data.receive_buf = NULL;
    i2c_data.receive_len = 0;
    
    ret = uapi_i2c_master_write(g_i2c_bus, MAX30102_I2C_ADDR, &i2c_data);
    if (ret != 0) {
        MAX30102_LOG("Write reg 0x%02X failed, ret=%d", reg, ret);
        return -1;
    }
    return 0;
}

int max30102_read_reg(uint8_t reg, uint8_t *value)
{
    i2c_data_t i2c_data = {0};
    uint32_t ret;
    
    i2c_data.send_buf = &reg;
    i2c_data.send_len = 1;
    i2c_data.receive_buf = value;
    i2c_data.receive_len = 1;
    
    ret = uapi_i2c_master_writeread(g_i2c_bus, MAX30102_I2C_ADDR, &i2c_data);
    if (ret != 0) {
        MAX30102_LOG("Read reg 0x%02X failed, ret=%d", reg, ret);
        return -1;
    }
    return 0;
}

int max30102_read_regs(uint8_t reg, uint8_t *buf, uint8_t len)
{
    i2c_data_t i2c_data = {0};
    uint32_t ret;
    
    i2c_data.send_buf = &reg;
    i2c_data.send_len = 1;
    i2c_data.receive_buf = buf;
    i2c_data.receive_len = len;
    
    ret = uapi_i2c_master_writeread(g_i2c_bus, MAX30102_I2C_ADDR, &i2c_data);
    if (ret != 0) {
        MAX30102_LOG("Read regs from 0x%02X failed, ret=%d", reg, ret);
        return -1;
    }
    return 0;
}

/* ========== FIFO / 温度 / 控制 ========== */

int max30102_fifo_read(max30102_fifo_t *data, uint8_t *sample_count)
{
    uint8_t fifo_wr_ptr, fifo_rd_ptr;
    uint8_t fifo_data[6];
    uint8_t samples_to_read;
    uint32_t ret;
    int i;
    
    ret = max30102_read_reg(MAX30102_REG_FIFO_WR_PTR, &fifo_wr_ptr);
    if (ret != 0) return -1;
    fifo_wr_ptr &= 0x1F;
    
    ret = max30102_read_reg(MAX30102_REG_FIFO_RD_PTR, &fifo_rd_ptr);
    if (ret != 0) return -1;
    fifo_rd_ptr &= 0x1F;
    
    if (fifo_wr_ptr >= fifo_rd_ptr) {
        samples_to_read = fifo_wr_ptr - fifo_rd_ptr;
    } else {
        samples_to_read = 32 - fifo_rd_ptr + fifo_wr_ptr;
    }
    
    if (samples_to_read == 0) {
        *sample_count = 0;
        return 0;
    }
    
    if (samples_to_read > *sample_count) {
        samples_to_read = *sample_count;
    }
    
    for (i = 0; i < samples_to_read; i++) {
        ret = max30102_read_regs(MAX30102_REG_FIFO_DATA, fifo_data, 6);
        if (ret != 0) return -1;
        
        data[i].ir  = ((uint32_t)fifo_data[0] << 16) | ((uint32_t)fifo_data[1] << 8) | fifo_data[2];
        data[i].ir  &= 0x3FFFF;
        
        data[i].red = ((uint32_t)fifo_data[3] << 16) | ((uint32_t)fifo_data[4] << 8) | fifo_data[5];
        data[i].red &= 0x3FFFF;
    }
    
    *sample_count = samples_to_read;
    return 0;
}

float max30102_read_temperature(void)
{
    uint8_t temp_int, temp_frac;
    int8_t signed_temp;
    float temp;
    uint32_t ret;
    
    ret = max30102_write_reg(MAX30102_REG_TEMP_CONFIG, 0x01);
    if (ret != 0) return -999.0f;
    
    osal_msleep(30);
    
    ret = max30102_read_reg(MAX30102_REG_TEMP_INT, &temp_int);
    if (ret != 0) return -999.0f;
    
    ret = max30102_read_reg(MAX30102_REG_TEMP_FRAC, &temp_frac);
    if (ret != 0) return -999.0f;
    
    signed_temp = (int8_t)temp_int;
    temp = (float)signed_temp + ((float)temp_frac * 0.0625f);
    
    return temp;
}

int max30102_start_temp_conversion(void)
{
    return max30102_write_reg(MAX30102_REG_TEMP_CONFIG, 0x01);
}

bool max30102_check_new_data(void)
{
    uint8_t int_status;
    
    if (max30102_read_reg(MAX30102_REG_INT_STATUS1, &int_status) != 0) {
        return false;
    }
    return (int_status & 0x40) != 0;
}

int max30102_reset(void)
{
    int ret;
    
    ret = max30102_write_reg(MAX30102_REG_MODE_CONFIG, 0x40);
    if (ret != 0) return -1;
    
    max30102_delay_ms_early(10);
    return 0;
}

int max30102_shutdown(void)
{
    uint8_t mode;
    
    if (max30102_read_reg(MAX30102_REG_MODE_CONFIG, &mode) != 0) {
        return -1;
    }
    mode |= 0x80;
    return max30102_write_reg(MAX30102_REG_MODE_CONFIG, mode);
}

int max30102_wakeup(void)
{
    uint8_t mode;
    
    if (max30102_read_reg(MAX30102_REG_MODE_CONFIG, &mode) != 0) {
        return -1;
    }
    mode &= ~0x80;
    return max30102_write_reg(MAX30102_REG_MODE_CONFIG, mode);
}

int max30102_set_led_current(uint8_t red_current, uint8_t ir_current)
{
    int ret;
    
    ret = max30102_write_reg(MAX30102_REG_LED1_PA, red_current);
    if (ret != 0) return -1;
    
    ret = max30102_write_reg(MAX30102_REG_LED2_PA, ir_current);
    if (ret != 0) return -1;
    
    return 0;
}

int max30102_set_sampling(uint8_t sample_rate, uint8_t pulse_width, uint8_t adc_range)
{
    uint8_t spo2_config;
    
    spo2_config = (adc_range << 5) | (sample_rate << 2) | pulse_width;
    return max30102_write_reg(MAX30102_REG_SPO2_CONFIG, spo2_config);
}

/* ========== 初始化 ========== */

int max30102_init(void)
{
    uint8_t part_id, rev_id;
    int ret;
    
    ret = max30102_i2c_init();
    if (ret != 0) {
        MAX30102_LOG("I2C init failed");
        return -1;
    }
    
    max30102_delay_ms_early(10);
    
    ret = max30102_read_reg(MAX30102_REG_PART_ID, &part_id);
    if (ret != 0) {
        MAX30102_LOG("Read PART_ID failed");
        return -1;
    }
    
    ret = max30102_read_reg(MAX30102_REG_REV_ID, &rev_id);
    if (ret != 0) {
        MAX30102_LOG("Read REV_ID failed");
        return -1;
    }
    
    MAX30102_LOG("PART_ID=0x%02X, REV_ID=0x%02X", part_id, rev_id);
    
    if (part_id != 0x15) {
        MAX30102_LOG("Warning: Unknown PART_ID, expected 0x15");
    }
    
    ret = max30102_reset();
    if (ret != 0) {
        MAX30102_LOG("Reset failed");
        return -1;
    }
    
    max30102_delay_ms_early(50);
    
    ret = max30102_write_reg(MAX30102_REG_FIFO_CONFIG, 0x4F);
    if (ret != 0) return -1;
    
    ret = max30102_write_reg(MAX30102_REG_MODE_CONFIG, MAX30102_MODE_SPO2);
    if (ret != 0) return -1;
    
    ret = max30102_set_sampling(MAX30102_SAMPRATE_100HZ,
                                 MAX30102_PULSE_WIDTH_411US,
                                 MAX30102_ADC_RANGE_4096);
    if (ret != 0) return -1;
    
    ret = max30102_set_led_current(MAX30102_LED_CURRENT_6_2MA,
                                    MAX30102_LED_CURRENT_6_2MA);
    if (ret != 0) return -1;
    
    ret = max30102_write_reg(MAX30102_REG_INT_ENABLE1, 0x40);
    if (ret != 0) return -1;
    
    ret = max30102_write_reg(MAX30102_REG_INT_ENABLE2, 0x00);
    if (ret != 0) return -1;
    
    max30102_read_reg(MAX30102_REG_INT_STATUS1, &part_id);
    max30102_read_reg(MAX30102_REG_INT_STATUS2, &part_id);
    
    MAX30102_LOG("MAX30102 init success");
    return 0;
}

/* ========== GPIO 中断 ========== */

static void max30102_gpio_isr(pin_t pin, uint32_t param)
{
    unused(pin);
    unused(param);
    
    uapi_gpio_clear_interrupt(MAX30102_INT_GPIO);
    
    if (g_int_callback != NULL) {
        g_int_callback();
    }
}

int max30102_int_init(max30102_int_callback_t callback)
{
    uint32_t ret;
    
    if (callback == NULL) {
        return -1;
    }
    g_int_callback = callback;
    
    ret = uapi_pin_set_mode(MAX30102_INT_GPIO, PIN_MODE_0);
    if (ret != 0) {
        MAX30102_LOG("Pin mode set failed");
        return -1;
    }
    
    ret = uapi_gpio_set_dir(MAX30102_INT_GPIO, GPIO_DIRECTION_INPUT);
    if (ret != 0) {
        MAX30102_LOG("GPIO dir set failed");
        return -1;
    }
    
    ret = uapi_pin_set_pull(MAX30102_INT_GPIO, PIN_PULL_TYPE_UP);
    if (ret != 0) {
        MAX30102_LOG("Pin pull set failed");
        return -1;
    }
    
    ret = uapi_gpio_register_isr_func(MAX30102_INT_GPIO,
                                       GPIO_INTERRUPT_FALLING_EDGE,
                                       max30102_gpio_isr);
    if (ret != 0) {
        MAX30102_LOG("GPIO ISR register failed");
        return -1;
    }
    
    ret = uapi_gpio_enable_interrupt(MAX30102_INT_GPIO);
    if (ret != 0) {
        MAX30102_LOG("GPIO interrupt enable failed");
        return -1;
    }
    
    MAX30102_LOG("INT interrupt init success");
    return 0;
}

void max30102_int_clear(void)
{
    uint8_t status1, status2;
    max30102_read_reg(MAX30102_REG_INT_STATUS1, &status1);
    max30102_read_reg(MAX30102_REG_INT_STATUS2, &status2);
    uapi_gpio_clear_interrupt(MAX30102_INT_GPIO);
}

/* ========== 任务 ========== */

static void *max30102_task(void *arg)
{
    unused(arg);
    int i;

    max30102_fifo_t fifo_data[8];
    uint8_t sample_count;
    float temperature;
    int ret;

    osal_printk("======== MAX30102 Task Start ========\r\n");

    /* 启动早期用忙等，等系统 tick 初始化完成 */
    for (i = 0; i < 5; i++) {
        osal_printk("MAX30102 init delay %d\r\n", i);
        max30102_delay_ms_early(500);
    }

    osal_printk("MAX30102 init begin...\r\n");

    ret = max30102_init();
    if (ret != 0) {
        osal_printk("MAX30102 init failed! ret=%d\r\n", ret);
        return NULL;
    }
    osal_printk("MAX30102 init OK!\r\n");

    temperature = max30102_read_temperature();
    osal_printk("Temp = %.2f C\r\n", temperature);

    osal_printk("MAX30102 enter main loop\r\n");

    while (1) {
        sample_count = 8;
        ret = max30102_fifo_read(fifo_data, &sample_count);

        if (ret == 0 && sample_count > 0) {
            int j;
            for (j = 0; j < sample_count; j++) {
                osal_printk("RED=%lu IR=%lu\r\n",
                            fifo_data[j].red, fifo_data[j].ir);
            }
        }
        osal_msleep(100);
    }
    return NULL;
}

void max30102_task_start(void)
{
    osal_task *task_handle = NULL;
    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)max30102_task, 0, "MAX30102_Task", MAX30102_TASK_STACK_SIZE);
    if (task_handle != NULL) osal_kthread_set_priority(task_handle, MAX30102_TASK_PRIO);
    osal_kthread_unlock();
}