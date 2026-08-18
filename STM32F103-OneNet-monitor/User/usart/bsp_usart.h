/**
  * @file    usart.h
  * @brief   USART 串口驱动接口声明
  */

#ifndef __USART_H__
#define __USART_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdarg.h>
#include <string.h>

/* 串口句柄外部引用 */
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;

/* 初始化函数 */
void MX_USART1_UART_Init(void);
void MX_USART2_UART_Init(void);

/**
  * @brief  向指定串口发送格式化字符串
  * @param  huart  目标串口句柄
  * @param  fmt    格式化字符串
  * @note   内部缓冲 256 字节, 超长截断
  */
void UsartPrintf(UART_HandleTypeDef *huart, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* __USART_H__ */