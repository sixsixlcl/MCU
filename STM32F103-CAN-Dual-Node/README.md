STM32F103 CAN 双节点通信（HAL + FreeRTOS）
==========================================

功能简述
--------
基于 STM32F103C8T6 + HAL + FreeRTOS 的 CAN 收发示例。
发送/接收共用一套工程，通过编译宏 CAN_NODE_ROLE_TX 切换角色。

CAN1 正常模式 1Mbps（APB1=36M, PSC=4, BS1=5TQ, BS2=3TQ）。
发送节点每 1s 发一帧标准数据帧：StdID=0x006, DLC=3, Data="abc"。
接收节点全通滤波，FIFO0 中断接收 -> FreeRTOS 队列 -> 任务处理打印。
UART 日志在独立任务输出，CAN 中断中不做格式化和串口发送。


硬件接线（STM32F103C8T6，外部 8MHz 晶振）
------------------------------------------
STM32 PA12 (CAN1_TX)  -> 收发器 TXD
STM32 PA11 (CAN1_RX)  <- 收发器 RXD
USART1 TX/RX          -> PA9 / PA10       （调试串口，115200 8-N-1）

!! PA11/PA12 禁止直连另一块板 CAN 引脚，必须经收发器 !!
两收发器 CANH/CANL/GND 对应相连。
总线两端各挂 120Ω 终端电阻，中间节点不加。
收发器需兼容 3.3V 逻辑，STBY/EN 引脚按手册接。


使用前准备
----------
1. 硬件按上表接好，确认终端电阻和收发器电平匹配。
2. Keil MDK 打开 Project/Fire_F103.uvprojx，HAL/FreeRTOS/启动文件已包含。
3. User/main.h 中 CAN_NODE_ROLE_TX=1，编译烧录作为发送节点。
4. 另开一份工程 CAN_NODE_ROLE_TX=0（默认），编译烧录作为接收节点。
5. 两板经收发器接入同一 CAN 总线，波特率必须一致。


上电运行
--------
调试串口 115200 可查看初始化及运行日志。
接收节点每秒打印一条接收日志，含帧类型、ID、RTR、DLC、数据内容。
发送节点无额外日志输出，仅周期性发送 CAN 帧。

当前滤波器为全通配置，仅用于测试；量产项目务必按需配 ID+掩码。
CAN_Send() 仅支持标准数据帧，最大 8 字节。
CAN_NODE_ROLE_TX 为编译期开关，运行时不可切换。


数据示例
--------
发送节点每 1s 发出：
StdID: 0x006
DLC:   3
Data:  61 62 63 ("abc")

接收节点串口输出：
[CAN] StdFrame ID=0x006 RTR=Data DLC=3 Data=61 62 63


目录结构
--------
Project/          Keil 工程文件
User/             应用代码、CAN/UART/GPIO 驱动、FreeRTOS 配置
Libraries/        CMSIS + STM32F1 HAL + FreeRTOS
Doc/              接线图、串口日志截图、演示视频