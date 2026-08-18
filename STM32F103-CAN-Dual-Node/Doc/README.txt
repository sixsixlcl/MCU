STM32F103 CAN 双节点通信示例
============================

本工程基于 STM32F103C8T6、STM32 HAL 和 FreeRTOS，实现两个 CAN 节点之间的通信。
发送节点和接收节点使用同一套工程，通过编译宏选择角色。

一、已实现功能
--------------
1. CAN1 正常模式，波特率 1 Mbps。
   参数：APB1 = 36 MHz，Prescaler = 4，BS1 = 5 TQ，BS2 = 3 TQ。
2. CAN 引脚：PA11 为 CAN_RX，PA12 为 CAN_TX，必须经过外部 CAN 收发器。
3. USART1 日志串口：PA9 为 TX，PA10 为 RX，参数为 115200、8-N-1。
4. 发送节点每隔 1 秒发送一帧标准数据帧：ID 为 0x006，数据为 ASCII 字符串 "abc"，DLC 为 3。
5. 接收节点采用全通滤波器，通过 FIFO0 接收中断读取 CAN 数据，再通过 FreeRTOS 队列交给接收任务处理。
6. UART 日志由独立 FreeRTOS 任务输出；CAN 中断中不执行字符串格式化和串口发送。

二、工程目录
------------
Project/   Keil MDK 工程文件 Fire_F103.uvprojx
User/      应用程序、CAN、UART、GPIO、中断和 FreeRTOS 配置文件
Libraries/ CMSIS、STM32F1 HAL 和 FreeRTOS 源码
Doc/       硬件接线图、串口日志截图和运行视频

三、编译和烧录
--------------
1. 使用 Keil MDK-ARM 5 打开 Project/Fire_F103.uvprojx。
2. 在 User/main.h 中将 CAN_NODE_ROLE_TX 设置为 1，编译并烧录一块开发板，作为发送节点。
3. 将另一份工程中的 CAN_NODE_ROLE_TX 设置为 0（默认值），编译并烧录另一块开发板，作为接收节点。
4. 两块开发板分别连接 CAN 收发器，再将两个收发器接入同一条 CAN 总线。
5. 打开接收节点 USART1，设置为 115200、8-N-1，即可看到每秒一条接收日志。

两块开发板必须使用相同的 CAN 波特率。本工程适用于外部 8 MHz 晶振和 STM32F103C8T6 芯片。

四、硬件接线
------------
- STM32 PA12（CAN1_TX）连接 CAN 收发器 TXD。
- STM32 PA11（CAN1_RX）连接 CAN 收发器 RXD。
- 两个收发器的 CANH 相连，CANL 相连，GND 相连。
- CAN 总线物理两端各安装一个 120 欧姆终端电阻，中间节点不要额外安装终端电阻。
- 使用与 3.3 V 逻辑电平兼容的 CAN 收发器，并按其数据手册连接待机或使能引脚。

PA11 和 PA12 不能直接连接到另一块 STM32 的 CAN 引脚，必须经过 CAN 收发器。

五、注意事项
------------
- CAN_NODE_ROLE_TX 是编译时配置，不能在运行时切换。
- 当前接收滤波器为全通配置，适合测试；正式项目应根据需要设置指定 ID 和掩码。
- CAN_Send() 发送标准数据帧，数据长度最大为 8 字节。

六、许可说明
------------
本仓库中的应用代码以及随工程提供的 STM32 HAL、FreeRTOS 组件可能分别适用不同的许可协议，请以对应源文件和厂商软件包中的许可文件为准。
