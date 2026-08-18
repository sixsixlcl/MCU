#ifndef __BSP_DHT11_H__
#define __BSP_DHT11_H__

#include "main.h"

/* ======================== DHT11 硬件引脚定义 ======================== */

#define DHT11_DATA_GPIO_Port   GPIOA        /* DATA 信号连接端口 */
#define DHT11_DATA_Pin         GPIO_PIN_11  /* DATA 信号连接引脚 */

/* ======================== DHT11 数据结构体 ======================== */
/**
  * @brief  DHT11 单次读取结果 (5字节原始数据)
  * @note   校验规则: check_sum == humi_int + humi_deci + temp_int + temp_deci
  */
typedef struct
{
    uint8_t humi_int;     /* 湿度整数部分 (%RH) */
    uint8_t humi_deci;    /* 湿度小数部分 (DHT11 恒为0) */
    uint8_t temp_int;     /* 温度整数部分 (℃) */
    uint8_t temp_deci;    /* 温度小数部分 (DHT11 恒为0) */
    uint8_t check_sum;    /* 校验和 (前4字节累加的低8位) */
} DHT11_DATA_TYPEDEF;

/* ======================== DHT11 驱动接口 ======================== */
void              DHT11_GPIO_Config(void);                          /* DATA引脚初始化 (推挽输出, 默认高) */
void              DHT11_SetGPIOMode(uint32_t mode, uint32_t pull);  /* 运行时切换引脚模式 (单总线收发切换) */
uint8_t           DHT11_ReadByte(void);                             /* 读取1字节 (MSB first, 内部调用) */
HAL_StatusTypeDef DHT11_ReadData(DHT11_DATA_TYPEDEF *dht11_data);   /* 完整读取一次温湿度 (含起始/应答/校验) */

#endif /* __BSP_DHT11_H__ */