#ifndef __BSP_LED_H__
#define __BSP_LED_H__

#include "main.h"

/* ======================== LED 硬件引脚定义 ======================== */
#define LED_GPIO_Port   GPIOA       /* LED 连接端口 */
#define LED_Pin         GPIO_PIN_7  /* LED 连接引脚, 低电平点亮 */

/* ======================== 全局状态变量声明 ======================== */
extern uint8_t LED_Status;          /* LED 当前实际物理状态: 0=灭, 1=亮 */
extern uint8_t LED_ManualStatus;    /* 云端/按键手动下发的目标控制状态 */

/* ======================== LED 驱动接口 ======================== */
void LED_GPIO_Config(void);         /* GPIO 初始化 (推挽输出, 默认灭) */
void LED_ON(void);                  /* 点亮 LED */
void LED_OFF(void);                 /* 熄灭 LED */

#endif /* __BSP_LED_H__ */