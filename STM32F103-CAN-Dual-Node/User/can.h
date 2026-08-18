/**
 * @file    bsp_can.h
 * @brief   CAN1 BSP 层公共接口与数据类型定义
 *
 * 职责边界:
 *   - 仅暴露 CAN 收发 API 与帧结构体
 *   - 硬件寄存器操作、ISR 实现均封装在 can.c 内部
 *   - 引脚宏定义集中于此，避免散落在多个源文件中
 *
 * ⚠️ 注意: CanFrame_t 按值传递（96 bit header + 64 bit data = 20 B），
 *          适合 FreeRTOS 队列拷贝语义；若后续扩展字段超过 32 B，
 *          应改为指针+内存池方案以避免栈/队列开销过大。
 */

#ifndef __BSP_CAN_H
#define __BSP_CAN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "FreeRTOS.h"
#include "queue.h"

/*----------------------------- 数据类型 ------------------------------------*/

/**
 * @brief  CAN 接收帧完整载体
 * @note   header + data 紧密排列，无填充字节（STM32 CAN_RxHeaderTypeDef
 *         本身已按 word 对齐）。该结构体同时用于 ISR → Task 队列传递
 *         和任务间消息转发，保持单一数据表示。
 */
typedef struct
{
    CAN_RxHeaderTypeDef header;  /**< HAL 标准 RX 头（IDE/RTR/DLC/Timestamp 等） */
    uint8_t             data[8]; /**< 有效载荷，未使用字节由发送方 DLC 界定 */
} CanFrame_t;

/*----------------------------- 引脚定义 ------------------------------------*/

/**
 * @brief  CAN1 物理引脚映射（PA11/PA12）
 * @note   STM32F1 CAN1 仅支持 PA11(RX)/PA12(TX) 或 PB8/PB9（重映射）。
 *         修改前请确认:
 *         1. 原理图实际连接
 *         2. AFIO_MAPR 中 CAN_REMAP 位是否匹配
 *         3. gpio.c 中对应端口时钟已使能
 */
#define CAN_RX_Pin        GPIO_PIN_11
#define CAN_RX_GPIO_Port  GPIOA
#define CAN_TX_Pin        GPIO_PIN_12
#define CAN_TX_GPIO_Port  GPIOA

/*----------------------------- 外部对象 ------------------------------------*/

/** CAN1 HAL 句柄（只读访问；配置修改请通过 CAN_Config / CAN_Init） */
extern CAN_HandleTypeDef hcan;

/*----------------------------- 公共 API ------------------------------------*/

/**
 * @brief  CAN 时序与工作模式配置
 * @note   内部调用 HAL_CAN_Init()，会触发 HAL_CAN_MspInit 回调。
 *         波特率参数详见 can.c 文件头注释。
 */
void CAN_Config(void);

/**
 * @brief  CAN 接收过滤器配置
 * @note   当前为全通过滤器；量产前务必按需收紧。
 */
void CAN_Filter_Config(void);

/**
 * @brief  CAN 完整初始化（Config → Filter → Start → Enable RX IRQ）
 * @pre    CAN_SetRxQueue() 必须在此之前调用，否则首帧丢失。
 */
void CAN_Init(void);

/**
 * @brief  注入 FreeRTOS RX 队列句柄（依赖反转）
 * @param  rxQueue  由应用层创建的 QueueHandle_t
 * @note   解耦驱动与 RTOS 对象生命周期，便于单元测试替换 mock 队列。
 */
void CAN_SetRxQueue(QueueHandle_t rxQueue);

/**
 * @brief  非阻塞发送一帧标准数据帧
 * @param  StdId  11-bit 标准标识符（0x000 ~ 0x7FF）
 * @param  pData  数据缓冲区指针
 * @param  Len    有效字节数（0~8）
 * @retval HAL_OK     消息已成功写入 TX Mailbox
 * @retval HAL_BUSY   三个邮箱均已占用
 * @retval HAL_ERROR  参数无效或外设未就绪
 * @note   返回 OK ≠ 发送完成；需配合 TX Complete 回调或轮询确认。
 */
HAL_StatusTypeDef CAN_Send(uint32_t StdId, const uint8_t *pData, uint8_t Len);

/**
 * @brief  CAN RX FIFO0 中断回调（HAL 弱函数重写）
 * @note   声明于此仅供查阅；实际实现在 can.c 中。
 *         用户代码不应直接调用此函数。
 */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_CAN_H */