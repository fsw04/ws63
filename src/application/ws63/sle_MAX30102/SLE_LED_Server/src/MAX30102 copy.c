#include "../inc/MAX30102.h"

#include "pinctrl.h"
#include "i2c.h"
#include "osal_debug.h"
#include "cmsis_os2.h"

/* 改为 uint32_t，18bit 数据最大 0x3FFFF = 262143，uint16_t 会溢出 */
uint32_t fifo_red;
uint32_t fifo_ir;
uint8_t ach_i2c_data[6];

uint8_t max30102_write_reg(uint8_t addr, uint8_t data)
{
    uint8_t Cmd[2] = {addr, data};
    i2c_data_t data0 = {0};
    data0.send_buf = Cmd;
    data0.send_len = sizeof(Cmd);
    uint32_t ret = uapi_i2c_master_write(1, MAX30102_w, &data0);
    if (ret != ERRCODE_SUCC) {
        osal_printk("i2c%d master send error %x !\r\n", 1, ret);
    }
    return 0;
}

uint32_t MAX_Read(uint8_t buffer)
{
    uint16_t dev_addr = MAX30102_w;
    i2c_data_t data = {0};
    data.send_buf = &buffer;
    data.send_len = sizeof(buffer);
    data.receive_buf = ach_i2c_data;
    data.receive_len = sizeof(ach_i2c_data);
    uint32_t ret = uapi_i2c_master_writeread(1, dev_addr, &data);
    if (ret != ERRCODE_SUCC) {
        osal_printk("i2c%d master read error %x!\r\n", 1, ret);
    }
    return 0;
}

uint32_t max30102_Read(uint8_t buffer)
{
    uint16_t dev_addr = MAX30102_w;
    i2c_data_t data = {0};
    uint8_t databuff[2] = {0};
    data.send_buf = &buffer;
    data.send_len = sizeof(buffer);
    data.receive_buf = databuff;
    data.receive_len = sizeof(databuff);
    uint32_t ret = uapi_i2c_master_writeread(1, dev_addr, &data);
    if (ret != ERRCODE_SUCC) {
        osal_printk("i2c%d master read error %x!\r\n", 1, ret);
    }
    return databuff[0];
}

uint8_t Max30102_reset(void)
{
    max30102_write_reg(REG_MODE_CONFIG, 0x40);
    return 0;
}

// void MAX30102_Config(void)
// {
//     max30102_write_reg(REG_INTR_ENABLE_1, 0xc0);
//     max30102_write_reg(REG_INTR_ENABLE_2, 0x00);
//     max30102_write_reg(REG_FIFO_WR_PTR, 0x00);
//     max30102_write_reg(REG_OVF_COUNTER, 0x00);
//     max30102_write_reg(REG_FIFO_RD_PTR, 0x00);
//     max30102_write_reg(REG_FIFO_CONFIG, 0x0f);
//     max30102_write_reg(REG_MODE_CONFIG, 0x03);
//     max30102_write_reg(REG_SPO2_CONFIG, 0x27);
//     max30102_write_reg(REG_LED1_PA, 0x32);
//     max30102_write_reg(REG_LED2_PA, 0x32);
//     max30102_write_reg(REG_PILOT_PA, 0x7f);
// }

void MAX30102_Config(void)
{
    max30102_write_reg(REG_INTR_ENABLE_1, 0xc0);  /* A_FULL + PPG_RDY */
    max30102_write_reg(REG_INTR_ENABLE_2, 0x00);
    max30102_write_reg(REG_FIFO_WR_PTR, 0x00);
    max30102_write_reg(REG_OVF_COUNTER, 0x00);
    max30102_write_reg(REG_FIFO_RD_PTR, 0x00);
    
    /* FIFO: SMP_AVE=0(无平均), Rollover=1(允许覆盖), A_FULL=15 */
    max30102_write_reg(REG_FIFO_CONFIG, 0x1f);
    
    max30102_write_reg(REG_MODE_CONFIG, 0x03);    /* SpO2模式 */
    
    /* 100sps + 411μs + 2048nA量程 (与手册Table 11一致) */
    max30102_write_reg(REG_SPO2_CONFIG, 0x07);
    
    max30102_write_reg(REG_LED1_PA, 0x32);        /* Red ~12.6mA */
    max30102_write_reg(REG_LED2_PA, 0x32);        /* IR ~12.6mA */
    
    /* 删除 REG_PILOT_PA，SpO2模式下无效 */
}

void max30102_read_fifo(void)
{
    fifo_red = 0;
    fifo_ir = 0;

    /* 读取并清除状态寄存器 */
    max30102_Read(REG_INTR_STATUS_1);
    max30102_Read(REG_INTR_STATUS_2);

    /* 从 FIFO 读取 6 字节（3字节红光 + 3字节红外） */
    MAX_Read(REG_FIFO_DATA);

    /* 标准 18bit 解析：MSB 在前，低 18 位有效 */
    fifo_red = ((uint32_t)ach_i2c_data[0] << 16)
             | ((uint32_t)ach_i2c_data[1] << 8)
             | (uint32_t)ach_i2c_data[2];
    fifo_red &= 0x3FFFF;  /* 只保留 18bit */

    fifo_ir = ((uint32_t)ach_i2c_data[3] << 16)
            | ((uint32_t)ach_i2c_data[4] << 8)
            | (uint32_t)ach_i2c_data[5];
    fifo_ir &= 0x3FFFF;   /* 只保留 18bit */

    /* 信号过弱时置零（避免噪声干扰） */
    if (fifo_ir <= 10000) {
        fifo_ir = 0;
    }
    if (fifo_red <= 10000) {
        fifo_red = 0;
    }

    /* 清空缓冲区 */
    ach_i2c_data[0] = 0;
    ach_i2c_data[1] = 0;
    ach_i2c_data[2] = 0;
    ach_i2c_data[3] = 0;
    ach_i2c_data[4] = 0;
    ach_i2c_data[5] = 0;
}