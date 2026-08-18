#ifndef __BSP_ALARM_H__
#define __BSP_ALARM_H__

#include "stm32f1xx_hal.h"

/* ======================== 蜂鸣器硬件引脚定义 ======================== */
#define ALARM_GPIO_Port   GPIOA       /* 蜂鸣器连接端口 */
#define ALARM_Pin         GPIO_PIN_5  /* 蜂鸣器连接引脚, 高电平触发鸣叫 */

/* ======================== 全局状态变量声明 ======================== */
extern uint8_t Alarm_Status;          /* 蜂鸣器当前实际物理状态: 0=静音, 1=鸣叫 */
extern uint8_t Alarm_ManualStatus;    /* 云端/按键手动下发的目标控制状态 */

/* ======================== 蜂鸣器驱动接口 ======================== */
void Alarm_GPIO_Config(void);         /* GPIO初始化 (推挽输出, 默认静音) */
void Alarm_ON(void);                  /* 开启蜂鸣器 */
void Alarm_OFF(void);                 /* 关闭蜂鸣器 */

#endif /* __BSP_ALARM_H__ */