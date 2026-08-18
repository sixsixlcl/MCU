/**
  ******************************************************************************
  * @file       main.c
  * @author     embedfire
  * @version    V1.0
  * @date       2025
  * @brief      DHT11温湿度采集 + OLED显示 + OneNET物联网上报 + 超温声光报警
  * @note       平台: 野火 STM32F103C8T6
  ******************************************************************************
  */

#include "main.h"
#include "led/bsp_led.h"
#include "dwt/bsp_dwt.h"
#include "usart/bsp_usart.h"
#include "dht11/bsp_dht11.h"
#include "i2c/bsp_i2c.h"
#include "oled/bsp_i2c_oled.h"
#include "dht11_oled/app_dht11_oled.h"
#include "esp8266.h"
#include "onenet.h"
#include "alarm/bsp_alarm.h"
#include "config.h"
#include <stdbool.h>
#include <stdio.h>

/* ======================== OneNET MQTT Topic 定义 ======================== */
#define ONENET_PROPERTY_POST_TOPIC        "$sys/" ONENET_PRODUCT_ID "/" ONENET_DEVICE_ID "/thing/property/post"
#define ONENET_PROPERTY_POST_REPLY_TOPIC  "$sys/" ONENET_PRODUCT_ID "/" ONENET_DEVICE_ID "/thing/property/post/reply"
#define ONENET_PROPERTY_SET_TOPIC         "$sys/" ONENET_PRODUCT_ID "/" ONENET_DEVICE_ID "/thing/property/set"

/* ======================== 主循环与任务调度参数 ======================== */
#define MAIN_LOOP_DELAY_MS          50U     /* 主循环基础时基 */
#define SENSOR_PUBLISH_TICKS        (3000U / MAIN_LOOP_DELAY_MS)  /* 传感器采集/上报周期 = 3s */
#define LED_LOW_FLASH_INTERVAL_MS   500U    /* 温度 >28°C 慢闪间隔 */
#define LED_HIGH_FLASH_INTERVAL_MS  150U    /* 温度 >30°C 快闪间隔 */

extern uint8_t LED_Status;

void SystemClock_Config(void);
static bool onenet_connected = false;

/**
  * @brief  OLED显示"网络连接中"提示
  */
void OLED_Show()
{
    OLED_ShowChinese_F16X16(1, 0, 5);   // 网
    OLED_ShowChinese_F16X16(1, 1, 6);   // 络
    OLED_ShowChinese_F16X16(1, 2, 7);   // 连
    OLED_ShowChinese_F16X16(1, 3, 8);   // 接
    OLED_ShowChinese_F16X16(1, 4, 9);   // 中
}

/**
  * @brief  系统外设与网络初始化
  */
void Function_Init()
{
    HAL_Init();
    SystemClock_Config();       // SYSCLK=72MHz, APB1=36MHz, APB2=72MHz

    MX_USART1_UART_Init();      // 调试串口
    printf("BOOT: USART1 OK\r\n");
    MX_USART2_UART_Init();      // ESP8266 AT指令串口
    printf("BOOT: USART2 OK\r\n");

    DWT_Init();                 // 高精度计时(用于DHT11时序)
    MX_I2C1_Init();             // OLED通信总线
    OLED_Init();
    DHT11_GPIO_Config();
    printf("BOOT: OLED/DHT OK\r\n");

    LED_GPIO_Config();
    Alarm_GPIO_Config();

    OLED_CLS();
    OLED_Show();                // 显示"网络连接中"
    printf("BOOT: ESP INIT\r\n");
    ESP8266_Init();

    /* 尝试连接OneNET并订阅属性设置/回复主题 */
    onenet_connected = (OneNet_DevLink() == 0);
    printf(onenet_connected ? "OneNET CONNECTED\r\n" : "OneNET CONNECT FAILED\r\n");
    if (onenet_connected)
    {
        const char *subscribe_topics[] = {
            ONENET_PROPERTY_SET_TOPIC,
            ONENET_PROPERTY_POST_REPLY_TOPIC
        };
        OneNet_Subscribe(subscribe_topics, 2);
    }
}

/**
  * @brief  构建JSON并发布传感器数据到OneNET
  * @param  sensor: DHT11读数指针
  */
static void PublishSensorValue(const DHT11_DATA_TYPEDEF *sensor)
{
    char publish_buf[128];
    int length;

    length = snprintf(publish_buf, sizeof(publish_buf),
                      "{\"id\":\"123\",\"version\":\"1.0\",\"params\":{"
                      "\"Temp\":{\"value\":%u},"
                      "\"Hum\":{\"value\":%u},"
                      "\"Led\":{\"value\":%s},"
                      "\"Alarm\":{\"value\":%s}}}",
                      sensor->temp_int, sensor->humi_int,
                      LED_Status ? "true" : "false",
                      Alarm_Status ? "true" : "false");

    if (length < 0 || (uint32_t)length >= sizeof(publish_buf))
    {
        printf("SENSOR JSON FAILED\r\n");
        return;
    }
    OneNet_Publish(ONENET_PROPERTY_POST_TOPIC, publish_buf);
}

/**
  * @brief  读取DHT11并在OLED上刷新温湿度显示
  * @retval true=读取成功, false=读取失败
  */
static bool UpdateSensorDisplay(DHT11_DATA_TYPEDEF *sensor)
{
    char text[8];

    OLED_CLS();
    /* 第0行: 标题栏(中文索引12~18) */
    for (uint8_t i = 0; i < 7; i++)
        OLED_ShowChinese_F16X16(0, i, 12 + i);

    if (DHT11_ReadData(sensor) == HAL_OK)
    {
        /* 第1行: 温度 */
        OLED_ShowChinese_F16X16(1, 0, 12);
        OLED_ShowChinese_F16X16(1, 1, 14);
        OLED_ShowString_F8X16(1, 4, (uint8_t *)":");
        snprintf(text, sizeof(text), "%2u.%uC", sensor->temp_int, sensor->temp_deci);
        OLED_ShowString_F8X16(1, 6, (uint8_t *)text);

        /* 第2行: 湿度 */
        OLED_ShowChinese_F16X16(2, 0, 13);
        OLED_ShowChinese_F16X16(2, 1, 14);
        OLED_ShowString_F8X16(2, 4, (uint8_t *)":");
        snprintf(text, sizeof(text), "%2u.%u%%", sensor->humi_int, sensor->humi_deci);
        OLED_ShowString_F8X16(2, 6, (uint8_t *)text);
        return true;
    }

    OLED_ShowString_F8X16(1, 0, (uint8_t *)"DHT11 ERROR");
    return false;
}

/**
  * @brief  根据温度区间控制LED/蜂鸣器闪烁频率
  *         >30°C: 150ms快闪 | >28°C: 500ms慢闪 | ≤28°C: 停止自动闪烁,恢复手动状态
  */
static void UpdateTemperatureLed(const DHT11_DATA_TYPEDEF *sensor)
{
    static uint32_t last_toggle_tick = 0U;
    static uint32_t active_interval_ms = 0U;
    uint32_t flash_interval_ms = 0U;

    /* 判定当前温度所属闪烁档位 */
    if ((sensor->temp_int > 30U) ||
        ((sensor->temp_int == 30U) && (sensor->temp_deci > 0U)))
    {
        flash_interval_ms = LED_HIGH_FLASH_INTERVAL_MS;
    }
    else if ((sensor->temp_int > 28U) ||
             ((sensor->temp_int == 28U) && (sensor->temp_deci > 0U)))
    {
        flash_interval_ms = LED_LOW_FLASH_INTERVAL_MS;
    }

    /* 未触发报警: 同步手动控制状态后退出 */
    if (flash_interval_ms == 0U)
    {
        active_interval_ms = 0U;
        if (Alarm_ManualStatus != Alarm_Status)
        {
            if (Alarm_ManualStatus) Alarm_ON(); else Alarm_OFF();
        }
        if (LED_ManualStatus != LED_Status)
        {
            if (LED_ManualStatus) LED_ON(); else LED_OFF();
            LED_Status = LED_ManualStatus;
        }
        return;
    }

    /* 切换闪烁档位时重置计时基准，避免相位跳变 */
    if (active_interval_ms != flash_interval_ms)
    {
        active_interval_ms = flash_interval_ms;
        last_toggle_tick = HAL_GetTick();
    }

    /* 非阻塞翻转LED+蜂鸣器 */
    if ((HAL_GetTick() - last_toggle_tick) >= active_interval_ms)
    {
        if (LED_Status) { LED_OFF(); Alarm_OFF(); }
        else            { LED_ON();  Alarm_ON();  }
        LED_Status = !LED_Status;
        last_toggle_tick = HAL_GetTick();
    }
}

int main(void)
{
    DHT11_DATA_TYPEDEF sensor = {0};
    uint16_t sensor_publish_ticks = SENSOR_PUBLISH_TICKS - 1U;
    bool sensor_valid = false;

    Function_Init();

    while (1)
    {
        /* ---- OneNET下行消息处理 ---- */
        if (onenet_connected)
        {
            unsigned char *received_data = ESP8266_GetIPD(1);
            if (received_data != NULL)
            {
                OneNet_RevPro(received_data);
                ESP8266_Clear();      // 消费已处理的+IPD帧
            }
            else if (esp8266_cnt > 400U)
            {
                ESP8266_Clear();      // 防止残留AT响应溢出RX缓冲
            }
        }

        /* ---- 周期性传感器采集/OLED刷新/云端上报 ---- */
        if (++sensor_publish_ticks >= SENSOR_PUBLISH_TICKS)
        {
            sensor_valid = UpdateSensorDisplay(&sensor);
            if (sensor_valid && onenet_connected)
                PublishSensorValue(&sensor);
            sensor_publish_ticks = 0;
        }

        /* ---- 超温声光报警(每轮都执行以保证响应实时性) ---- */
        if (sensor_valid)
            UpdateTemperatureLed(&sensor);

        HAL_Delay(MAIN_LOOP_DELAY_MS);
    }
}

/**
  * @brief  系统时钟配置: HSE(8MHz) → PLL×9 → SYSCLK=72MHz
  *         AHB=72MHz, APB2=72MHz, APB1=36MHz, Flash=2WS
  */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef oscinitstruct = {0};
    RCC_ClkInitTypeDef clkinitstruct = {0};

    oscinitstruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    oscinitstruct.HSEState       = RCC_HSE_ON;
    oscinitstruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    oscinitstruct.PLL.PLLState   = RCC_PLL_ON;
    oscinitstruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    oscinitstruct.PLL.PLLMUL     = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&oscinitstruct) != HAL_OK)
        while (1);

    clkinitstruct.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                                   RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clkinitstruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clkinitstruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clkinitstruct.APB2CLKDivider = RCC_HCLK_DIV1;
    clkinitstruct.APB1CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&clkinitstruct, FLASH_LATENCY_2) != HAL_OK)
        while (1);
}

void Error_Handler(void)
{
    __disable_irq();
    while (1);
}