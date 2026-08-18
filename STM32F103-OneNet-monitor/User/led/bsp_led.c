/**
  ******************************************************************************
  * @file       bsp_led.c
  * @author     embedfire
  * @version    V1.0
  * @date       2025
  * @brief      LED状态指示灯底层驱动 (低电平点亮)
  ******************************************************************************
  */

#include "led/bsp_led.h"

uint8_t LED_ManualStatus = 0;   /* 云端/按键手动下发的目标状态 */
uint8_t LED_Status = 0;         /* LED当前实际物理状态: 0=灭, 1=亮 */
uint8_t Alarm_flag = 0;         /* 告警标志: 0=正常, 1=告警触发中 */

/**
  * @brief  LED GPIO初始化
  * @note   推挽输出, 默认SET(灭), 无上下拉
  */
void LED_GPIO_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* 先置高确保初始化过程中LED不会误亮 */
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);

    GPIO_InitStruct.Pin   = LED_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);
}

/** @brief 点亮LED (低电平有效) */
void LED_ON(void)
{
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
}

/** @brief 熄灭LED */
void LED_OFF(void)
{
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
}