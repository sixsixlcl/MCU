---

```
STM32F103 + OneNET 温湿度监测（DHT11 + OLED + ESP8266）
=========================================================

功能简述
--------
DHT11 读温湿度，本地 OLED 显示，同时通过 ESP8266 走 MQTT 上报到 OneNET。
云端可查看数据，也能下发指令控制板载 LED 和蜂鸣器。

超温逻辑：超过 28℃ 闪灯+蜂鸣（500ms 间隔），超过 30℃ 加快到 150ms。
温度回落后恢复云端最后一次下发的手动状态。

网络断开时本地显示和告警不受影响，只是不上传数据。
DHT11 读失败时 OLED 显示 "DHT11 ERROR"。


硬件接线（STM32F103C8，系统时钟 72MHz）
----------------------------------------
DHT11 DATA      -> PA11              （单总线数据线）
OLED SCL        -> PB6               （I2C1 时钟，400kHz）
OLED SDA        -> PB7               （I2C1 数据，地址 0x3C）
ESP8266 TX      -> PA3 (USART2_RX)
ESP8266 RX      -> PA2 (USART2_TX)
调试串口 TX/RX  -> PA9 / PA10        （USART1，115200 8-N-1）
LED             -> PA7               （低电平点亮）
蜂鸣器          -> PA5               （高电平响）


使用前准备
----------
1. 硬件按上表接好，注意电平匹配。
2. Keil MDK 打开 Project/Fire_F103.uvprojx，库和启动文件已包含。
3. OneNET 建好产品和设备，物模型加四个属性：Temp、Hum、Led、Alarm（大小写一致）。
4. 复制 User/config.example.h 为 User/config.h，填入 Wi-Fi SSID、密码、OneNET 产品ID、设备ID、Token。
5. Keil 选 HAL 目标编译，ST-Link 烧录后复位。


上电运行
--------
调试串口 115200 可看到初始化流程：OLED -> DHT11 -> ESP8266 -> Wi-Fi -> OneNET。
联网成功时 OLED 显示 "NET CONNECTED"，串口打印 "OneNET CONNECTED"。
之后每 3 秒上报一次数据，OLED 同步刷新。

云端下发的 LED / Alarm 会直接控制引脚。
超温告警时本地强制输出，温度回落至 28℃ 以下后恢复手动控制。


数据示例
--------
DHT11 读到 26.5℃、61.0%，LED 和蜂鸣器关闭。

上报主题：
$sys/PRODUCT_ID_EXAMPLE/stm32f103_dht11_demo/thing/property/post

上报消息：
{
  "id": "123",
  "version": "1.0",
  "params": {
    "Temp": { "value": 26 },
    "Hum": { "value": 61 },
    "Led": { "value": false },
    "Alarm": { "value": false }
  }
}

OLED 显示：
温度: 26.5C
湿度: 61.0%

云端下发控制（打开 LED 和蜂鸣器）：
主题：$sys/PRODUCT_ID_EXAMPLE/stm32f103_dht11_demo/thing/property/set
消息：
{
  "params": {
    "LED": true,
    "Alarm": true
  }
}


目录结构
--------
Project/          Keil 工程文件
User/             主循环、各模块驱动（DHT11/OLED/ESP8266/OneNET/LED/蜂鸣器/串口）
APP/              DHT11/OLED 应用层封装
Libraries/        STM32 CMSIS + HAL
Doc/              说明文档和接线图
```

---
