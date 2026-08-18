/**
 * @file    usart.c
 * @brief   USART1 初始化（调试串口 / 上位机通信）
 *
 * 配置摘要:
 *   - 115200 baud, 8N1, TX/RX, 无流控, 16x 过采样
 *   - PA9 (TX), PA10 (RX)
 *
 * ?? 波特率精度提示:
 *   STM32F1 APB2 = 72 MHz 时，115200 实际误差 ≈ 0.16%（可接受）。
 *   若系统时钟变更导致误差 > 2%，需调整 BaudRate 或切换 OVERSAMPLING_8。
 *
 * ?? MspInit/MspDeInit 调用契约:
 *   这两个函数由 HAL_UART_Init / HAL_UART_DeInit 内部自动回调，
 *   用户代码禁止直接调用，否则会导致 GPIO/时钟重复初始化或资源泄漏。
 */

#include "usart.h"

/* UART 句柄（全局可见，供 printf 重定向 / 调试器检视） */
UART_HandleTypeDef huart1;

/**
 * @brief  USART1 参数配置与外设初始化
 * @note   OverSampling = 16x: 标准模式，抗噪声能力优于 8x，
 *         适用于大多数 RS-232 / USB-CDC 场景。
 *         HwFlowCtl = NONE: 当前硬件未连接 RTS/CTS 引脚。
 */
void MX_USART1_UART_Init(void)
{
    huart1.Instance          = USART1;
    huart1.Init.BaudRate     = 115200;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
 * @brief  HAL MSP 回调：USART1 时钟、GPIO 底层初始化
 * @note   由 HAL_UART_Init() 自动触发。
 *         TX = AF_PP + HIGH: 满足 UART 边沿速率要求，避免信号畸变。
 *         RX = INPUT + NOPULL: 外部已有确定电平（USB 芯片 / 收发器），
 *              内部上拉会与外部驱动冲突，增加静态功耗。
 */
void HAL_UART_MspInit(UART_HandleTypeDef *uartHandle)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (uartHandle->Instance == USART1)
    {
        /* 外设时钟使能 */
        __HAL_RCC_USART1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /* PA9: USART1_TX — 复用推挽输出，高速 */
        GPIO_InitStruct.Pin   = GPIO_PIN_9;
        GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        /* PA10: USART1_RX — 浮空输入 */
        GPIO_InitStruct.Pin  = GPIO_PIN_10;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }
}

/**
 * @brief  HAL MSP 反初始化回调：释放 USART1 硬件资源
 * @note   由 HAL_UART_DeInit() 自动触发。
 *         关闭时钟前必须先完成 GPIO DeInit，避免悬空引脚漏电。
 */
void HAL_UART_MspDeInit(UART_HandleTypeDef *uartHandle)
{
    if (uartHandle->Instance == USART1)
    {
        __HAL_RCC_USART1_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9 | GPIO_PIN_10);
    }
}