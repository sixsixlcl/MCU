/**
 * @file    gpio.c
 * @brief   GPIO 初始化（LED / 调试指示引脚）
 *
 * 设计说明:
 *   - PA2、PA3 配置为推挽输出，默认低电平（LED 灭 / 外设未使能）。
 *   - 速度设为 LOW：仅驱动 LED 等低频负载，降低边沿辐射噪声，
 *     避免对 CAN 总线及模拟信号造成 EMI 干扰。
 *   - 无上下拉：外部已有确定电平或驱动源，内部电阻反而增加功耗。
 */

#include "gpio.h"

/**
 * @brief  初始化所有应用使用的 GPIO 引脚
 * @note   该函数由 main() 在 HAL_Init() 之后、RTOS 启动之前调用。
 *         若后续新增引脚，请在此函数中追加配置，保持集中管理。
 */
void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* ---- 端口时钟使能 ----
   * GPIOA: PA2, PA3（当前使用）
   * GPIOC/GPIOD: 预留，CubeMX 根据 .ioc 自动生成；
   *              若确认未使用可移除以节省动态功耗。
   */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /* ---- 输出初始电平 ----
   * 先写寄存器再初始化方向，避免上电瞬间出现不确定毛刺。
   * PA2/PA3 = RESET → LED 默认熄灭 / 外部器件默认未选通。
   */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2 | GPIO_PIN_3, GPIO_PIN_RESET);

  /* ---- PA2 & PA3: 推挽输出 ---- */
  GPIO_InitStruct.Pin   = GPIO_PIN_2 | GPIO_PIN_3;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;   /* 推挽，可主动驱动高低电平 */
  GPIO_InitStruct.Pull  = GPIO_NOPULL;           /* 无内部上下拉 */
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;   /* ≤2 MHz 翻转，满足 LED 需求且 EMI 最优 */
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}