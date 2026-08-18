/**
 * @file    usart.h
 * @brief   USART1 BSP 层公共接口声明
 * @note    仅暴露 UART 初始化入口与句柄引用；
 *          收发数据操作请使用 HAL_UART_Transmit / HAL_UART_Receive 等
 *          标准 HAL API，勿在本头文件中添加业务层封装。
 */

#ifndef __USART_H__
#define __USART_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/**
 * @brief  USART1 HAL 句柄（全局只读访问）
 * @note   - 配置修改请通过 MX_USART1_UART_Init()
 *         - 运行时收发请使用 &huart1 作为 HAL API 参数
 *         - 禁止在其他模块中直接赋值或重新初始化此句柄
 */
extern UART_HandleTypeDef huart1;

/**
 * @brief  USART1 硬件初始化（115200-8N1, PA9/PA10）
 * @note   - 必须在 HAL_Init() 之后、RTOS 调度器启动之前调用
 *         - 内部已包含端口时钟使能与 GPIO 配置
 *         - 若需启用中断/DMA 接收，请在调用本函数后另行开启
 */
void MX_USART1_UART_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __USART_H__ */