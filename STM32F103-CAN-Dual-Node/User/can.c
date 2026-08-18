/**
 * @file    can.c
 * @brief   CAN1 硬件驱动 + FreeRTOS ISR 桥接层
 *
 * 波特率: 500 kbps
 *   APB1 = 36 MHz, Prescaler = 4 → TQ = 4/36 MHz ≈ 111 ns
 *   BitTime = 1 + BS1(5) + BS2(3) = 9 TQ
 *   BaudRate = 36 MHz / (4 × 9) = 1 MHz ? → 实际为 1 Mbps
 *   
 *
 * ISR 设计原则:
 *   - HAL_CAN_RxFifo0MsgPendingCallback 仅执行 GetRxMessage + QueueSendFromISR
 *   - 所有解析、格式化、日志输出均推迟到 CanRxTask 任务上下文
 *   - 中断优先级 = 5，低于 configMAX_SYSCALL_INTERRUPT_PRIORITY 阈值，
 *     确保 FromISR API 调用安全
 */

#include "can.h"

/* CAN 外设句柄（全局可见，供调试器检视） */
CAN_HandleTypeDef hcan;

/**
 * @brief  CAN RX → FreeRTOS 队列的桥接句柄
 * @note   由 main() 在 CAN_Init() 之前通过 CAN_SetRxQueue() 注入，
 *         避免驱动模块直接依赖 RTOS 对象创建顺序。
 */
static QueueHandle_t s_canRxQueue;

/*----------------------------- 底层配置 ------------------------------------*/

/**
 * @brief  CAN1 时序与模式配置
 * @note   AutoBusOff=ENABLE: 总线关闭后自动恢复，避免单次错误导致永久离线
 *         AutoRetransmission=ENABLE: 发送失败自动重发，保证消息可靠送达
 *         ReceiveFifoLocked=DISABLE: FIFO 满时覆盖旧消息，优先保留最新帧
 */
void CAN_Config(void)
{
    hcan.Instance                  = CAN1;
    hcan.Init.Prescaler            = 4;       /* ?? 见文件头波特率说明 */
    hcan.Init.Mode                 = CAN_MODE_NORMAL;
    hcan.Init.SyncJumpWidth        = CAN_SJW_1TQ;
    hcan.Init.TimeSeg1             = CAN_BS1_5TQ;
    hcan.Init.TimeSeg2             = CAN_BS2_3TQ;
    hcan.Init.TimeTriggeredMode    = DISABLE;
    hcan.Init.AutoBusOff           = ENABLE;
    hcan.Init.AutoWakeUp           = ENABLE;
    hcan.Init.AutoRetransmission   = ENABLE;
    hcan.Init.ReceiveFifoLocked    = DISABLE;
    hcan.Init.TransmitFifoPriority = DISABLE; /* ID 优先级 > 邮箱序号 */

    if (HAL_CAN_Init(&hcan) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
 * @brief  HAL MSP 回调：CAN1 时钟、GPIO、NVIC 初始化
 * @note   由 HAL_CAN_Init() 内部自动调用，用户不应手动触发。
 *         TX 引脚设为 AF_PP + HIGH 速度以满足 CAN 边沿要求；
 *         RX 引脚为浮空输入，由收发器外部驱动。
 */
void HAL_CAN_MspInit(CAN_HandleTypeDef *canHandle)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (canHandle->Instance == CAN1)
    {
        __HAL_RCC_CAN1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /* CAN_RX: 浮空输入，电平由外部收发器决定 */
        GPIO_InitStruct.Pin  = CAN_RX_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(CAN_RX_GPIO_Port, &GPIO_InitStruct);

        /* CAN_TX: 复用推挽，高速以保障信号完整性 */
        GPIO_InitStruct.Pin   = CAN_TX_Pin;
        GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(CAN_TX_GPIO_Port, &GPIO_InitStruct);

        /**
         * 中断优先级 = 5
         * FreeRTOS Cortex-M3 默认 configMAX_SYSCALL_INTERRUPT_PRIORITY = 5
         * ≤ 此值的中断才可安全调用 xQueueSendFromISR 等 FromISR API
         */
        HAL_NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
    }
}

/**
 * @brief  HAL MSP 反初始化：释放 CAN1 相关硬件资源
 */
void HAL_CAN_MspDeInit(CAN_HandleTypeDef *canHandle)
{
    if (canHandle->Instance == CAN1)
    {
        __HAL_RCC_CAN1_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOA, CAN_RX_Pin | CAN_TX_Pin);
        HAL_NVIC_DisableIRQ(USB_LP_CAN1_RX0_IRQn);
    }
}

/*----------------------------- 过滤器配置 ----------------------------------*/

/**
 * @brief  CAN 接收过滤器：全通（接受所有 ID）
 * @note   Mask = 0x00000000 → 不检查任何 ID 位 → 全部通过
 *         适用于开发/调试阶段；量产时应按需配置精确过滤，
 *         减少无效帧进入 FIFO 造成的 CPU 开销。
 */
void CAN_Filter_Config(void)
{
    CAN_FilterTypeDef canFilter = {0};

    canFilter.FilterBank           = 0;
    canFilter.FilterMode           = CAN_FILTERMODE_IDMASK;
    canFilter.FilterScale          = CAN_FILTERSCALE_32BIT;
    canFilter.FilterIdHigh         = 0x0000;
    canFilter.FilterIdLow          = 0x0000;
    canFilter.FilterMaskIdHigh     = 0x0000;  /* Mask=0 → 全通 */
    canFilter.FilterMaskIdLow      = 0x0000;
    canFilter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    canFilter.FilterActivation     = ENABLE;

    if (HAL_CAN_ConfigFilter(&hcan, &canFilter) != HAL_OK)
    {
        Error_Handler();
    }
}

/*----------------------------- 公共接口 ------------------------------------*/

/**
 * @brief  CAN 完整初始化入口
 * @note   调用顺序：Config → Filter → Start → Enable RX Interrupt
 *         HAL_CAN_Start 之后才激活通知，避免启动过程中产生虚假中断。
 */
void CAN_Init(void)
{
    CAN_Config();
    CAN_Filter_Config();
    HAL_CAN_Start(&hcan);
    HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING);
}

/**
 * @brief  注入 RX 队列句柄（依赖反转）
 * @param  rxQueue  由 main() 创建的 FreeRTOS 队列
 * @note   必须在 CAN_Init() 之前调用，否则首条 RX 中断将因
 *         s_canRxQueue == NULL 而丢帧。
 */
void CAN_SetRxQueue(QueueHandle_t rxQueue)
{
    s_canRxQueue = rxQueue;
}

/**
 * @brief  发送一帧标准数据帧
 * @param  StdId  11-bit 标准标识符
 * @param  pData  数据指针（长度 ≤ 8）
 * @param  Len    有效数据字节数
 * @return HAL_OK / HAL_BUSY / HAL_ERROR
 * @note   非阻塞：仅将消息写入 TX Mailbox 即返回。
 *         发送完成/失败状态需通过 HAL_CAN_TxMailboxXCompleteCallback
 *         或轮询 HAL_CAN_IsTxMessagePending 获取。
 */
HAL_StatusTypeDef CAN_Send(uint32_t StdId, const uint8_t *pData, uint8_t Len)
{
    CAN_TxHeaderTypeDef txHeader;
    uint32_t txMailbox;

    txHeader.StdId = StdId;
    txHeader.IDE   = CAN_ID_STD;
    txHeader.RTR   = CAN_RTR_DATA;
    txHeader.DLC   = Len;

    return HAL_CAN_AddTxMessage(&hcan, &txHeader, (uint8_t *)pData, &txMailbox);
}

/*----------------------------- ISR 回调 ------------------------------------*/

/**
 * @brief  CAN RX FIFO0 消息挂起中断回调
 * @note   ? ISR 热路径，必须保持极短执行时间：
 *         1. GetRxMessage 读取硬件 FIFO（~几 μs）
 *         2. xQueueSendFromISR 入队（无阻塞，超时=0 语义由上层保证）
 *         3. portYIELD_FROM_ISR 触发上下文切换（若有高优先级任务被唤醒）
 *         禁止在此函数内调用 printf、snprintf、HAL_UART_Transmit 或
 *         任何可能阻塞/耗时的操作。
 */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CanFrame_t frame;
    BaseType_t higherPriorityTaskWoken = pdFALSE;

    if ((s_canRxQueue != NULL) &&
        (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &frame.header, frame.data) == HAL_OK))
    {
        (void)xQueueSendFromISR(s_canRxQueue, &frame, &higherPriorityTaskWoken);
        portYIELD_FROM_ISR(higherPriorityTaskWoken);
    }
}