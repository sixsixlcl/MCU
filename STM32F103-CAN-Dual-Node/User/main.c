/**
 * @file    main.c
 * @brief   FreeRTOS + CAN 收发演示（STM32F1）
 *
 * 架构说明:
 *   - CAN 中断仅做最小化入队操作，帧解析与日志格式化全部推迟到任务上下文，
 *     避免 ISR 执行时间过长导致总线错误。
 *   - UART 打印通过独立低优先级任务 + 互斥锁串行化，确保多条日志不会交错，
 *     同时保证 CAN 任务永远不会因串口阻塞而被挂起。
 *   - 编译宏 CAN_NODE_ROLE_TX 在预处理阶段选择 TX 或 RX 角色，
 *     同一份源码可分别构建发送节点与接收节点固件。
 */

#include "main.h"
#include "can.h"
#include "usart.h"
#include "gpio.h"

#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/*----------------------------- 类型定义 ------------------------------------*/

/**
 * @brief  日志消息载体
 * @note   96 字节足以容纳一行 CAN 帧打印（含换行），
 *         按值传递避免动态内存分配，适合嵌入式环境。
 */
typedef struct
{
  char text[96];
} LogMessage_t;

/*----------------------------- 模块私有变量 --------------------------------*/

#if (CAN_NODE_ROLE_TX == 0)
/** CAN 接收队列：ISR → CanRxTask 的无锁单生产者-单消费者通道 */
static QueueHandle_t s_canRxQueue;
#endif

/** 日志队列：解耦 CAN 任务与 UART 输出速率 */
static QueueHandle_t s_logQueue;

/** UART1 互斥锁：保证每条日志原子发送，防止多任务输出 interleaving */
static SemaphoreHandle_t s_uart1Mutex;

/*----------------------------- 函数声明 ------------------------------------*/

void SystemClock_Config(void);

#if (CAN_NODE_ROLE_TX == 0)
static void CanRxTask(void *argument);
#endif
#if (CAN_NODE_ROLE_TX == 1)
static void CanTxTask(void *argument);
#endif
static void UartLogTask(void *argument);
static void LogMessage(const char *text);

/*----------------------------- 主函数 --------------------------------------*/

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART1_UART_Init();

  /* ---- RTOS 对象创建 ---- */
#if (CAN_NODE_ROLE_TX == 0)
  /* 深度 8：可吸收短时 CAN 突发，同时限制 RAM 占用 */
  s_canRxQueue = xQueueCreate(8, sizeof(CanFrame_t));
#endif
  /* 深度 12：UART 波特率较低时提供足够缓冲，避免 CAN 任务被反压 */
  s_logQueue   = xQueueCreate(12, sizeof(LogMessage_t));
  s_uart1Mutex = xSemaphoreCreateMutex();

  if ((s_logQueue == NULL) || (s_uart1Mutex == NULL)
#if (CAN_NODE_ROLE_TX == 0)

      || (s_canRxQueue == NULL)
#endif
     )
  {
    Error_Handler();
  }

  /* ---- CAN 外设初始化（必须在队列创建之后，ISR 可能立即入队） ---- */
#if (CAN_NODE_ROLE_TX == 0)
  CAN_SetRxQueue(s_canRxQueue);
#endif
  CAN_Init();

  /* ---- 任务创建 ----
   * 优先级策略：CAN_RX(3) > CAN_TX(2) > UART_LOG(1)
   * - RX 最高：避免丢帧
   * - TX 次之：保证发送周期抖动可控
   * - LOG 最低：诊断输出永远不抢占通信任务
   */
  if (
#if (CAN_NODE_ROLE_TX == 0)
      xTaskCreate(CanRxTask, "can_rx",   256, NULL, tskIDLE_PRIORITY + 3, NULL) != pdPASS ||
#else
      xTaskCreate(CanTxTask, "can_tx",   192, NULL, tskIDLE_PRIORITY + 2, NULL) != pdPASS ||
#endif
      xTaskCreate(UartLogTask, "uart_log", 256, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS)
  {
    Error_Handler();
  }

  /* 启动调度器；以下代码仅在调度器启动失败时执行 */
  vTaskStartScheduler();

  while (1) {}
}

/*----------------------------- 时钟配置 ------------------------------------*/

/**
 * @brief  HSE 8 MHz × PLL9 = 72 MHz SYSCLK
 *         APB1 = 36 MHz（CAN 外设挂载于此，注意波特率预分频基准）
 *         APB2 = 72 MHz
 */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState       = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL     = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                     RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;   /* APB1 = 36 MHz */
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;   /* APB2 = 72 MHz */

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/*----------------------------- 任务实现 ------------------------------------*/

#if (CAN_NODE_ROLE_TX == 0)
/**
 * @brief  CAN 接收任务
 * @note   以 portMAX_DELAY 永久阻塞等待，零 CPU 空转。
 *         帧格式化使用 snprintf 而非 printf，避免隐式堆栈开销。
 */
static void CanRxTask(void *argument)
{
  CanFrame_t frame;
  uint32_t id;
  LogMessage_t message;
  (void)argument;

  for (;;)
  {
    if (xQueueReceive(s_canRxQueue, &frame, portMAX_DELAY) == pdPASS)
    {
      id = (frame.header.IDE == CAN_ID_STD) ? frame.header.StdId : frame.header.ExtId;
      (void)snprintf(message.text, sizeof(message.text),
                     "CAN RX %s ID=0x%lX %s DLC=%u DATA=%02X %02X %02X %02X %02X %02X %02X %02X\r\n",
                     (frame.header.IDE == CAN_ID_STD) ? "STD" : "EXT",
                     (unsigned long)id,
                     (frame.header.RTR == CAN_RTR_DATA) ? "DATA" : "REMOTE",
                     (unsigned int)frame.header.DLC,
                     frame.data[0], frame.data[1], frame.data[2], frame.data[3],
                     frame.data[4], frame.data[5], frame.data[6], frame.data[7]);
      LogMessage(message.text);
    }
  }
}
#endif

#if (CAN_NODE_ROLE_TX == 1)
/**
 * @brief  CAN 发送任务
 * @note   固定 1 s 周期发送测试帧；使用 vTaskDelay 而非 HAL_Delay，
 *         让出 CPU 给同优先级及以下任务，保持调度器响应性。
 */
static void CanTxTask(void *argument)
{
  uint32_t count = 0;
  const uint8_t data[] = "abc";
  LogMessage_t message;
  (void)argument;

  for (;;)
  {
    if (CAN_Send(0x006, data, sizeof(data) - 1U) == HAL_OK)
    {
      (void)snprintf(message.text, sizeof(message.text),
                     "CAN TX count=%lu ID=0x006 DLC=3\r\n", (unsigned long)++count);
    }
    else
    {
      (void)snprintf(message.text, sizeof(message.text), "CAN TX failed\r\n");
    }
    LogMessage(message.text);

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
#endif

/**
 * @brief  UART 日志输出任务
 * @note   互斥锁持有范围覆盖整条 HAL_UART_Transmit，
 *         确保一条完整日志不会被其他任务的输出打断。
 *         该任务优先级最低，即使串口忙也不会影响 CAN 通信。
 */
static void UartLogTask(void *argument)
{
  LogMessage_t message;
  (void)argument;

  for (;;)
  {
    if (xQueueReceive(s_logQueue, &message, portMAX_DELAY) == pdPASS)
    {
      if (xSemaphoreTake(s_uart1Mutex, portMAX_DELAY) == pdTRUE)
      {
        (void)HAL_UART_Transmit(&huart1, (uint8_t *)message.text,
                                (uint16_t)strlen(message.text), HAL_MAX_DELAY);
        xSemaphoreGive(s_uart1Mutex);
      }
    }
  }
}

/**
 * @brief  非阻塞日志入队接口
 * @param  text  以 '\0' 结尾的字符串
 * @note   xQueueSend 超时设为 0：队列满时直接丢弃，
 *         绝不让 CAN 任务因日志缓冲区满而阻塞。
 *         这是"诊断服从于通信"的核心设计决策。
 */
static void LogMessage(const char *text)
{
  LogMessage_t message;
  (void)snprintf(message.text, sizeof(message.text), "%s", text);
  (void)xQueueSend(s_logQueue, &message, 0);
}

/*----------------------------- 错误处理 ------------------------------------*/

void Error_Handler(void)
{
  __disable_irq();
  while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  (void)file;
  (void)line;
}
#endif