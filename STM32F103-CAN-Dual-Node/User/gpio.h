/**
 * @file    gpio.h
 * @brief   GPIO 初始化接口声明
 * @note    仅暴露板级 GPIO 配置入口；具体引脚定义与电气参数
 *          请参阅 gpio.c 中的实现注释。
 */

#ifndef __GPIO_H__
#define __GPIO_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/**
 * @brief  初始化所有应用使用的 GPIO 引脚
 * @note   - 必须在 HAL_Init() 之后、RTOS 调度器启动之前调用
 *         - 内部已包含端口时钟使能，调用方无需重复开启
 *         - 若需运行时修改引脚状态，请直接使用 HAL_GPIO_WritePin /
 *           HAL_GPIO_ReadPin，勿重新调用本函数
 */
void MX_GPIO_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __GPIO_H__ */