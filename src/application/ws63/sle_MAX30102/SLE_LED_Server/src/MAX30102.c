#include "../inc/MAX30102.h"
#include "i2c.h"
#include "osal_debug.h"
#include "errcode.h"
#include "soc_osal.h"

/* FIFO 解析结果 */
uint32_t fifo_red = 0;
uint32_t fifo_ir = 0;
uint8_t ach_i2c_data[6] = {0};

/**
 * @brief 向 MAX30102 寄存器写入一个字节 (WS63平台)
 */
uint8_t max30102_write_reg(uint8_t addr, uint8_t data)
{
    uint8_t cmd[2] = {addr, data};
    i2c_data_t i2c_data = {0};

    i2c_data.send_buf = cmd;
    i2c_data.send_len = sizeof(cmd);

    uint32_t ret = uapi_i2c_master_write(MAX30102_I2C_BUS, MAX30102_ADDR, &i2c_data);
    if (ret != ERRCODE_SUCC) {
        osal_printk("MAX30102 write err: 0x%x\r\n", ret);
        return 1;
    }
    return 0;
}

/**
 * @brief 从 MAX30102 寄存器读取一个字节 (WS63平台)
 */
uint8_t max30102_read_reg(uint8_t addr)
{
    i2c_data_t i2c_data = {0};
    uint8_t rx_buf[1] = {0};

    i2c_data.send_buf = &addr;
    i2c_data.send_len = 1;
    i2c_data.receive_buf = rx_buf;
    i2c_data.receive_len = 1;

    uint32_t ret = uapi_i2c_master_writeread(MAX30102_I2C_BUS, MAX30102_ADDR, &i2c_data);
    if (ret != ERRCODE_SUCC) {
        osal_printk("MAX30102 read err: 0x%x\r\n", ret);
        return 0;
    }
    return rx_buf[0];
}

/**
 * @brief 读取 FIFO 数据（6字节 = 3字节红光 + 3字节红外）
 *        标准 18bit 解析，屏蔽高 6 位
 *        
 *        【修复】参考STM32的maxim_max30102_read_fifo实现：
 *        1. 先读取并清除中断状态
 *        2. 读取FIFO_DATA寄存器6字节
 *        3. 组合成18bit数据
 *        4. 添加延时等待下一个样本（100Hz = 10ms间隔）
 */
void max30102_read_fifo(void)
{
    fifo_red = 0;
    fifo_ir = 0;

    /* 读取并清除中断状态寄存器 - 与STM32一致 */
    max30102_read_reg(REG_INTR_STATUS_1);
    max30102_read_reg(REG_INTR_STATUS_2);

    /* 发送 FIFO_DATA 寄存器地址，然后读取 6 字节 */
    uint8_t reg = REG_FIFO_DATA;
    i2c_data_t i2c_data = {0};

    i2c_data.send_buf = &reg;
    i2c_data.send_len = 1;
    i2c_data.receive_buf = ach_i2c_data;
    i2c_data.receive_len = 6;

    uint32_t ret = uapi_i2c_master_writeread(MAX30102_I2C_BUS, MAX30102_ADDR, &i2c_data);
    if (ret != ERRCODE_SUCC) {
        osal_printk("MAX30102 FIFO read err: 0x%x\r\n", ret);
        return;
    }

    /* 大端序组合 18bit 数据 - 与STM32逻辑一致 */
    fifo_red = ((uint32_t)ach_i2c_data[0] << 16)
             | ((uint32_t)ach_i2c_data[1] << 8)
             |  (uint32_t)ach_i2c_data[2];
    fifo_red &= 0x03FFFF;

    fifo_ir = ((uint32_t)ach_i2c_data[3] << 16)
            | ((uint32_t)ach_i2c_data[4] << 8)
            |  (uint32_t)ach_i2c_data[5];
    fifo_ir &= 0x03FFFF;

    /* 【修复】添加延时：100Hz采样率，每10ms一个样本
     * STM32的MAX30102_IIC_ReadBytes()末尾有DelayMs(10)
     * 确保不会读取过快导致数据重复或错误
     */
    osal_mdelay(10);
}

/**
 * @brief MAX30102 初始化配置（基于 STM32 移植参数）
 *        - SpO2 模式，100Hz 采样率，18bit 分辨率
 *        - ADC 量程 4096nA，LED 电流约 7mA（STM32原始配置）
 */
void MAX30102_Config(void)
{
    max30102_write_reg(REG_INTR_ENABLE_1, 0xc0);  /* A_FULL + PPG_RDY 中断 */
    max30102_write_reg(REG_INTR_ENABLE_2, 0x00);
    max30102_write_reg(REG_FIFO_WR_PTR, 0x00);
    max30102_write_reg(REG_OVF_COUNTER, 0x00);
    max30102_write_reg(REG_FIFO_RD_PTR, 0x00);

    /* FIFO: 样本平均=1, 不循环, 几乎满阈值=17 */
    max30102_write_reg(REG_FIFO_CONFIG, 0x0f);

    /* 模式: SpO2（红+红外） */
    max30102_write_reg(REG_MODE_CONFIG, 0x03);

    /* SpO2: ADC Range=4096nA, Sample Rate=100Hz, Pulse Width=400us(18bit) */
    max30102_write_reg(REG_SPO2_CONFIG, 0x27);

    /* LED 电流 ~7mA (0x24 * 0.2mA)，STM32原始配置 */
    max30102_write_reg(REG_LED1_PA, 0x24);
    max30102_write_reg(REG_LED2_PA, 0x24);
    max30102_write_reg(REG_PILOT_PA, 0x7f);  /* PILOT LED ~25mA */
}

void Max30102_reset(void)
{
    max30102_write_reg(REG_MODE_CONFIG, 0x40);
}