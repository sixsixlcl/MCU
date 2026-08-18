# STM32F103 OneNET 温湿度监测系统

这是一个基于 STM32F103C8、DHT11、OLED 和 ESP8266 的物联网实验项目。设备读取温湿度，在 OLED 上显示，并通过 MQTT 上报到 OneNET；云端可以控制 LED 和蜂鸣器。

## 功能

- 每 3 秒采集一次 DHT11 温湿度。
- OLED 显示温度、湿度和传感器错误状态。
- OneNET 上报 `Temp`、`Hum`、`Led`、`Alarm` 属性。
- 接收 `LED` 和 `Alarm` 云端控制命令。
- 温度超过 28 C/30 C 时执行两档本地声光告警。

## 硬件接线

| 模块 | STM32F103 引脚 |
| --- | --- |
| DHT11 DATA | PA11 |
| OLED SCL / SDA | PB6 / PB7 |
| ESP8266 TX / RX | PA3 / PA2（USART2） |
| 调试串口 TX / RX | PA9 / PA10（USART1） |
| LED | PA7，低电平点亮 |
| 蜂鸣器 | PA5，高电平开启 |

接线图片见 [`Doc/硬件接线图`](Doc/硬件接线图)，详细说明见 [`Doc/readme.txt`](Doc/readme.txt)。

## 编译与配置

1. 将 `User/config.example.h` 复制为 `User/config.h`。
2. 在 `User/config.h` 中填写自己的 Wi-Fi、OneNET 产品 ID、设备 ID 和 Token。示例设备 ID 为 `stm32f103_dht11_demo`，不是真实账号。
3. 用 Keil MDK 打开 `Project/Fire_F103.uvprojx`，选择 `HAL` target 后 Build/Rebuild。
4. 使用 ST-Link 或兼容下载器烧录生成的固件。

`User/config.h` 已加入 `.gitignore`，不要将真实密码或 Token 提交到公开仓库。工程使用 ARM Compiler 5.06 update 7 和 STM32F1xx Device Pack 2.3.0。

## 目录

- `APP/`：应用层逻辑
- `User/`：传感器、显示、串口、ESP8266 和 OneNET 驱动
- `Libraries/`：STM32 CMSIS/HAL
- `Project/`：Keil 工程文件
- `Doc/`：接线图和补充说明

## 已知限制

- 当前 ESP8266 使用 TCP 1883 端口，未实现 TLS 加密。
- Wi-Fi 和 OneNET 初始化失败时会重试，重试过程是阻塞的。
- `Project/Objects`、`Project/Listings` 和 Keil 个人配置不会提交到 Git。

## 第三方代码

项目包含 STMicroelectronics HAL/CMSIS、cJSON 以及正点原子/野火相关代码。发布时请保留源文件中的版权和许可证声明，并根据实际授权情况选择仓库许可证。
