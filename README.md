# MCU Projects

基于 STM32F103 的嵌入式项目合集。目前包含物联网监测和 CAN 双节点通信两个独立工程。

## 项目列表

| 项目 | 简介 | 主要技术 |
| --- | --- | --- |
| [STM32F103-OneNet-monitor](STM32F103-OneNet-monitor/) | 温湿度采集、OLED 本地显示、ESP8266 MQTT 上报 OneNET，并支持 LED 与蜂鸣器云端控制。 | STM32F103C8、DHT11、OLED、ESP8266、MQTT |
| [STM32F103-CAN-Dual-Node](STM32F103-CAN-Dual-Node/) | 两块 STM32F103 通过 CAN 总线进行收发通信；发送与接收角色通过编译宏切换。 | STM32F103C8T6、CAN、FreeRTOS、UART |

## 开发环境

- Keil MDK
- ARM Compiler 5.06 update 7
- STM32F1xx Device Pack 2.3.0
- ST-Link 或兼容下载器

每个工程都是独立的 Keil 项目。进入对应目录后，用 Keil 打开 `Project/Fire_F103.uvprojx`，选择目标配置并编译、烧录即可。

## 硬件概览

### OneNET 温湿度监测

- STM32F103C8
- DHT11 温湿度传感器
- I2C OLED 显示屏
- ESP8266 Wi-Fi 模块
- LED、蜂鸣器

首次使用前，请将 `User/config.example.h` 复制为 `User/config.h`，填写 Wi-Fi 和 OneNET 配置。`config.h` 已被忽略，不能提交真实的账号、密码或 Token。

### CAN 双节点通信

- 两块 STM32F103C8T6 开发板
- 两个兼容 3.3V 逻辑的 CAN 收发器
- CANH、CANL、GND 连接线
- 总线两端各一个 120 ohm 终端电阻

使用 `User/main.h` 中的 `CAN_NODE_ROLE_TX` 宏设置发送或接收角色。CAN 引脚为 PA12（TX）与 PA11（RX）；两块开发板不能直接相连，必须通过 CAN 收发器接入总线。

## 仓库约定

- 提交源代码、工程配置、文档和接线图。
- 不提交 Keil 编译输出、调试配置或个人 IDE 设置。
- 各工程详情、引脚定义和测试说明见其目录内的 README 与 `Doc/`。

## 许可证与第三方代码

工程包含 STM32 HAL/CMSIS、FreeRTOS、cJSON 等第三方组件。使用或发布前，请保留原始版权与许可证声明，并根据实际授权情况选择仓库许可证。
