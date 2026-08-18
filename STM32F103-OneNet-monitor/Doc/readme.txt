# STM32F103 OneNET 联网温湿度监测系统

## 1. 项目解决什么问题

本项目将 DHT11 的环境温湿度采集、OLED 本地显示、超温声光提示和 OneNET 云端上报整合到 STM32F103C8 平台中。它适用于需要在设备端查看温湿度、在云端查看当前数据，并能从云端控制 LED 和蜂鸣器的简单物联网监测场景。

设备通过 ESP8266 接入 Wi-Fi，再使用 MQTT 连接 OneNET。温湿度读取失败时，OLED 显示 `DHT11 ERROR`；网络未连接时，设备仍会进行本地采集、显示和阈值告警，但不会上传数据。

## 2. 主要功能

- 每 3 秒读取一次 DHT11 温度和湿度，并在 I2C OLED 上显示温度与湿度。
- 通过 OneNET MQTT 属性上报 `Temp`、`Hum`、`Led` 和 `Alarm`。
- 订阅 OneNET 属性设置主题，可接收 `LED` 与 `Alarm` 布尔控制命令。
- 温度高于 28.0 C 时 LED 与蜂鸣器以 500 ms 闪烁；温度高于 30.0 C 时以 150 ms 闪烁。温度不高于 28.0 C 时，恢复云端下发的手动状态。
- 通过 USART1 输出启动、联网和 MQTT 调试信息；USART2 用于 ESP8266 AT 指令及 MQTT 数据通信。

## 硬件与接线

目标芯片为 STM32F103C8，系统时钟为 72 MHz。请按下表连接模块；电源电平应与所用模块和开发板兼容。

| 模块/用途 | STM32 引脚 | 说明 |
| --- | --- | --- |
| DHT11 DATA | PA11 | 温湿度数据线 |
| OLED SCL | PB6 | I2C1 时钟，400 kHz |
| OLED SDA | PB7 | I2C1 数据，OLED 地址为 `0x3C` |
| ESP8266 TX | PA3 / USART2_RX | ESP8266 到 STM32 |
| ESP8266 RX | PA2 / USART2_TX | STM32 到 ESP8266 |
| 调试串口 TX/RX | PA9 / PA10（USART1） | 115200, 8-N-1 |
| LED | PA7 | 低电平点亮 |
| 蜂鸣器/告警器 | PA5 | 高电平开启 |

## 3. 安装方法

1. 准备 STM32F103C8 开发板、DHT11、I2C OLED、ESP8266、LED 和蜂鸣器，并按上述接线连接。接线图片见 `Doc/硬件接线图（含面包板+扩展板）`。
2. 使用 Keil MDK 打开 `Project/Fire_F103.uvprojx`。工程已包含 STM32F1 HAL、CMSIS、应用代码和启动文件，无需额外安装 C 语言包。
3. 在 OneNET 中创建或使用已有产品和设备，并为物模型建立 `Temp`、`Hum`、`Led`、`Alarm` 属性。属性名和大小写必须与本项目一致。
4. 复制 `User/config.example.h` 为 `User/config.h`，填写目标 Wi-Fi 的 SSID、密码以及 OneNET 产品 ID、设备 ID 和 Token。示例设备 ID `stm32f103_dht11_demo` 仅用于说明格式；不要将真实 Wi-Fi 密码或 Token 提交到公开仓库。
5. 在 Keil 中选择目标 `HAL`，执行 Build；使用 ST-Link 或兼容下载器将生成的 `Fire_F103` 固件烧录至开发板并复位。

## 4. 使用方法

1. 上电后，USART1 调试终端按 `115200, 8-N-1` 打开。设备依次初始化 OLED、DHT11、ESP8266，并尝试连接 Wi-Fi、OneNET。
2. 连接成功时 OLED 短暂显示 `NET CONNECTED`，调试串口输出 `OneNET CONNECTED`；随后 OLED 显示温度和湿度。
3. 云端在设备属性页查看每 3 秒更新的温湿度和设备状态。
4. 向属性设置主题下发 `LED` 或 `Alarm` 布尔值以控制 LED 或蜂鸣器。当温度超过阈值时，自动告警会暂时覆盖手动输出；温度恢复到 28.0 C 及以下后，手动状态重新生效。

## 5. 输入输出实例

### 传感器输入

假设 DHT11 读取到温度 `26.5 C`、湿度 `61.0 %`，并且 LED、蜂鸣器均关闭。程序在 `DHT11_ReadData()` 成功后显示并上报数据。

### OneNET 上报输出

发布主题：

```text
$sys/PRODUCT_ID_EXAMPLE/stm32f103_dht11_demo/thing/property/post
```

消息体格式如下，其中 `Temp` 和 `Hum` 仅上报整数部分：

```json
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
```

对应 OLED 显示内容为：

```text
温度: 26.5C
湿度: 61.0%
```

### 云端控制输入

设备订阅以下主题：

```text
$sys/PRODUCT_ID_EXAMPLE/stm32f103_dht11_demo/thing/property/set
```

开启 LED 和蜂鸣器的下发示例：

```json
{
  "params": {
    "LED": true,
    "Alarm": true
  }
}
```



## 目录说明

- `Project/`：Keil 工程与调试配置。
- `User/`：主循环、DHT11、OLED、ESP8266、OneNET、LED、蜂鸣器和串口驱动。
- `APP/`：DHT11/OLED 应用层代码。
- `Libraries/`：STM32 CMSIS 与 HAL 库。
- `Doc/`：项目说明和硬件接线图。
