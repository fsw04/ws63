#include "../inc/MAX30102.h"

#include "pinctrl.h"
#include "i2c.h"
#include "osal_debug.h"
#include "cmsis_os2.h"

uint16_t fifo_red;  //定义FIFO中的红光数据
uint16_t fifo_ir;   //定义FIFO中的红外光数据
uint8_t ach_i2c_data[6];

/**
 * ************************************************************************
 * @brief 向MAX30102寄存器写入一个值
 * 
 * @param[in] addr  寄存器地址
 * @param[in] data  传输数据
 * 
 * @return 
 * ************************************************************************
 */



/**
 * ************************************************************************
 * @brief 读取MAX30102寄存器的一个值
 * 
 * @param[in] addr  寄存器地址
 * 
 * @return 
 * ************************************************************************
 */
// uint8_t max30102_read_reg(uint8_t addr )
// {
// //   uint8_t data=0;
// //   HAL_I2C_Mem_Read(&hi2c1, MAX30102_Device_address, addr, 1, &data, 1, HAL_MAX_DELAY);
// //   return data;
//     // uint8_t Cmd[] = {addr};
//     // uint8_t x ;
//     // i2c_data_t data0 = {0};
//     // data0.receive_buf = Cmd;
//     // data0.receive_len = 1;
//     // uint32_t retval = uapi_i2c_master_read(1, MAX30102_Device_address, &data0);
//     // if (retval != 0) {
//     //     printf("I2cRead() failed, %0X!\n", retval);
//     //     return retval;
//     // }
//     // x = Cmd[0];
//     // return x;  

//     uint8_t buffer[] = {addr};

//     MAX_Read(buffer, sizeof(buffer));
//     printf("%d",buffer[0]);

//     return buffer[0] ;



// }


//  uint32_t MAX_Read(uint8_t *buffer, uint32_t buffLen)
// {
//     uint16_t dev_addr = MAX30102_w;
//     i2c_data_t data = {0};

//     data.receive_buf = buffer;
//     data.receive_len = buffLen;
//     uint32_t retval = uapi_i2c_master_read(1, dev_addr, &data);
//     if (retval != 0) {
//         printf("I2cRead() failed, %0X!\n", retval);
//         return retval;
//     }
//     return 0;
// }

uint8_t max30102_write_reg(uint8_t addr, uint8_t data)
{
//   HAL_I2C_Mem_Write(&hi2c1, MAX30102_Device_address,	addr,	1,	&data,1,HAL_MAX_DELAY);
//   return 1;
  uint8_t Cmd[2] = {addr, data};
  i2c_data_t data0 = {0};
  data0.send_buf = Cmd;
  data0.send_len = sizeof(Cmd);
  uint32_t ret = uapi_i2c_master_write(1, MAX30102_w, &data0);
    if (ret != ERRCODE_SUCC)
    {
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
    // uint32_t ret = uapi_i2c_master_read(1, dev_addr, &data);
    if (ret != ERRCODE_SUCC)
    {
        osal_printk("i2c%d master read error %x!\r\n", 1, ret);
    }
    return 0;

}


	
// max30102_Read(ach_i2c_data, sizeof(ach_i2c_data));

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
    // uint32_t ret = uapi_i2c_master_read(1, dev_addr, &data);
    if (ret != ERRCODE_SUCC)
    {
        osal_printk("i2c%d master read error %x!\r\n", 1, ret);
    }

    return databuff[0];
}

/**
 * ************************************************************************
 * @brief MAX30102传感器复位
 * 
 * 
 * @return 
 * ************************************************************************
 */
uint8_t Max30102_reset(void)
{
	max30102_write_reg(REG_MODE_CONFIG, 0x40);
  return 0;

}

/**
 * ************************************************************************
 * @brief MAX30102传感器模式配置
 * 
 * 
 * ************************************************************************
 */
void MAX30102_Config(void)
{
  // max30102_write_reg(REG_MODE_CONFIG, 0x40);
  
	max30102_write_reg(REG_INTR_ENABLE_1,0xc0); //INTR setting
	max30102_write_reg(REG_INTR_ENABLE_2,0x00);//
	max30102_write_reg(REG_FIFO_WR_PTR,0x00);//FIFO_WR_PTR[4:0]
	max30102_write_reg(REG_OVF_COUNTER,0x00);//OVF_COUNTER[4:0]
	max30102_write_reg(REG_FIFO_RD_PTR,0x00);//FIFO_RD_PTR[4:0]
	
	max30102_write_reg(REG_FIFO_CONFIG,0x0f);//sample avg = 1, fifo rollover=false, fifo almost full = 17
	max30102_write_reg(REG_MODE_CONFIG,0x03);//0x02 for Red only, 0x03 for SpO2 mode 0x07 multimode LED
	max30102_write_reg(REG_SPO2_CONFIG,0x27);	// SPO2_ADC range = 4096nA, SPO2 sample rate (50 Hz), LED pulseWidth (400uS)  
	max30102_write_reg(REG_LED1_PA,0x32);//Choose value for ~ 10mA for LED1
	max30102_write_reg(REG_LED2_PA,0x32);// Choose value for ~ 10mA for LED2
	max30102_write_reg(REG_PILOT_PA,0x7f);// Choose value for ~ 25mA for Pilot LED
}

/**
 * ************************************************************************
 * @brief 读取FIFO寄存器的数据
 * 
 * 
 * ************************************************************************
 */
void max30102_read_fifo(void)
{
  uint16_t un_temp;
  fifo_red=0;
  fifo_ir=0;
  // uint8_t ach_i2c_data[6];
 

  // read and clear status register
  max30102_Read(REG_INTR_STATUS_1);
  max30102_Read(REG_INTR_STATUS_2);





  
  // ach_i2c_data[0]=REG_FIFO_DATA;

   MAX_Read(REG_FIFO_DATA);



	// HAL_I2C_Mem_Read(1,MAX30102_Device_address,REG_FIFO_DATA,1,ach_i2c_data,6,HAL_MAX_DELAY);
	
  un_temp=ach_i2c_data[0];
  un_temp<<=14;
  fifo_red+=un_temp;
  un_temp=ach_i2c_data[1];
  un_temp<<=6;
  fifo_red+=un_temp;
  un_temp=ach_i2c_data[2];
	un_temp>>=2;
  fifo_red+=un_temp;
  
  un_temp=ach_i2c_data[3];
  un_temp<<=14;
  fifo_ir+=un_temp;
  un_temp=ach_i2c_data[4];
  un_temp<<=6;
  fifo_ir+=un_temp;
  un_temp=ach_i2c_data[5];
	un_temp>>=2;
  fifo_ir+=un_temp;
	
	if(fifo_ir<=10000)
	{
		fifo_ir=0;
	}
	if(fifo_red<=10000)
	{
		fifo_red=0;
	}

    // printf("%d %d\r\n",fifo_ir,fifo_red);
  ach_i2c_data[0]=0;
  ach_i2c_data[1]=0;
  ach_i2c_data[2]=0;
  ach_i2c_data[3]=0;
  ach_i2c_data[4]=0;
  ach_i2c_data[5]=0;

}
