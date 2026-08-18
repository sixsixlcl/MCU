#include "alarm/bsp_alarm.h"

uint8_t Alarm_Status = 0;         /* 蜂鸣器当前实际物理状态: 0=静音, 1=鸣叫 */
uint8_t Alarm_ManualStatus = 0;   /* 云端/按键手动下发的目标控制状态 */

/**
  * @brief  蜂鸣器 GPIO 初始化
  * @note   推挽输出, 默认 RESET(静音), 高电平触发鸣叫
  */
void Alarm_GPIO_Config(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* 先置低确保初始化过程中蜂鸣器不会误鸣 */
    HAL_GPIO_WritePin(ALARM_GPIO_Port, ALARM_Pin, GPIO_PIN_RESET);

    gpio_init.Pin   = ALARM_Pin;
    gpio_init.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull  = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(ALARM_GPIO_Port, &gpio_init);
}

/** @brief 开启蜂鸣器 (高电平有效) */
void Alarm_ON(void)
{
    HAL_GPIO_WritePin(ALARM_GPIO_Port, ALARM_Pin, GPIO_PIN_SET);
    Alarm_Status = 1;
}

/** @brief 关闭蜂鸣器 */
void Alarm_OFF(void)
{
    HAL_GPIO_WritePin(ALARM_GPIO_Port, ALARM_Pin, GPIO_PIN_RESET);
    Alarm_Status = 0;
}