#include "dht11/bsp_dht11.h"
#include "dwt/bsp_dwt.h"

/**
  * @brief  DHT11 DATA 引脚初始化 (默认推挽输出, 高电平空闲)
  */
void DHT11_GPIO_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    HAL_GPIO_WritePin(DHT11_DATA_GPIO_Port, DHT11_DATA_Pin, GPIO_PIN_SET);

    GPIO_InitStruct.Pin   = DHT11_DATA_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DHT11_DATA_GPIO_Port, &GPIO_InitStruct);
}

/**
  * @brief  运行时切换 DHT11 DATA 引脚工作模式
  * @param  mode  GPIO_MODE_OUTPUT_PP / GPIO_MODE_INPUT
  * @param  pull  GPIO_NOPULL / GPIO_PULLUP
  * @note   单总线协议需在输出(发送起始信号)与输入(读取响应/数据)间动态切换
  */
void DHT11_SetGPIOMode(uint32_t mode, uint32_t pull)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin   = DHT11_DATA_Pin;
    GPIO_InitStruct.Mode  = mode;
    GPIO_InitStruct.Pull  = pull;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DHT11_DATA_GPIO_Port, &GPIO_InitStruct);
}

/**
  * @brief  从 DHT11 读取一个字节 (MSB first)
  * @retval 8位数据
  * @note   每位数据以50us低电平开始, 高电平持续26-28us表示'0', 70us表示'1'
  *         此处采样点选在40us处, 可可靠区分两种脉宽
  */
uint8_t DHT11_ReadByte(void)
{
    uint8_t value = 0;

    for (uint8_t i = 0; i < 8; i++)
    {
        /* 等待当前位起始低电平结束 */
        while (HAL_GPIO_ReadPin(DHT11_DATA_GPIO_Port, DHT11_DATA_Pin) == GPIO_PIN_RESET);

        DWT_DelayUs(40);

        if (HAL_GPIO_ReadPin(DHT11_DATA_GPIO_Port, DHT11_DATA_Pin) == GPIO_PIN_SET)
        {
            value |= (1 << (7 - i));
            /* 等待高电平结束, 进入下一位的起始低电平 */
            while (HAL_GPIO_ReadPin(DHT11_DATA_GPIO_Port, DHT11_DATA_Pin) == GPIO_PIN_SET);
        }
    }

    return value;
}

/**
  * @brief  完整读取一次 DHT11 温湿度数据
  * @param  data  输出参数, 存放解析后的温湿度及校验和
  * @retval HAL_OK: 读取成功且校验通过 | HAL_ERROR: 超时或校验失败
  */
HAL_StatusTypeDef DHT11_ReadData(DHT11_DATA_TYPEDEF *data)
{
    uint8_t retry = 0;

    /* ---- 1. 主机发送起始信号: 拉低≥18ms → 释放并等待30us ---- */
    DHT11_SetGPIOMode(GPIO_MODE_OUTPUT_PP, GPIO_NOPULL);
    HAL_GPIO_WritePin(DHT11_DATA_GPIO_Port, DHT11_DATA_Pin, GPIO_PIN_RESET);
    DWT_DelayMs(20);
    HAL_GPIO_WritePin(DHT11_DATA_GPIO_Port, DHT11_DATA_Pin, GPIO_PIN_SET);
    DWT_DelayUs(30);

    /* ---- 2. 切换为输入, 等待 DHT11 应答脉冲 (低→高→低) ---- */
    DHT11_SetGPIOMode(GPIO_MODE_INPUT, GPIO_PULLUP);

    /* 应答低电平 (80us) */
    retry = 0;
    while (HAL_GPIO_ReadPin(DHT11_DATA_GPIO_Port, DHT11_DATA_Pin) == GPIO_PIN_SET)
    {
        if (++retry > 100) return HAL_ERROR;
        DWT_DelayUs(1);
    }

    /* 应答高电平 (80us) */
    retry = 0;
    while (HAL_GPIO_ReadPin(DHT11_DATA_GPIO_Port, DHT11_DATA_Pin) == GPIO_PIN_RESET)
    {
        if (++retry > 100) return HAL_ERROR;
        DWT_DelayUs(1);
    }

    /* 数据开始前的高电平间隔 (50us) */
    retry = 0;
    while (HAL_GPIO_ReadPin(DHT11_DATA_GPIO_Port, DHT11_DATA_Pin) == GPIO_PIN_SET)
    {
        if (++retry > 100) return HAL_ERROR;
        DWT_DelayUs(1);
    }

    /* ---- 3. 连续读取5字节: 湿度整数/小数 + 温度整数/小数 + 校验和 ---- */
    data->humi_int  = DHT11_ReadByte();
    data->humi_deci = DHT11_ReadByte();
    data->temp_int  = DHT11_ReadByte();
    data->temp_deci = DHT11_ReadByte();
    data->check_sum = DHT11_ReadByte();

    /* ---- 4. 校验: 前4字节之和 == 第5字节 ---- */
    uint8_t sum = data->humi_int + data->humi_deci + data->temp_int + data->temp_deci;
    return (sum == data->check_sum) ? HAL_OK : HAL_ERROR;
}
