# Armrobot

[English](README.md) | 简体中文

这是一个面向自动机械臂机构的 STM32F103C8 裸机固件，控制两个 HTD-85H 串口舵机，以及一个通过 L298N 驱动、带编码器的直线推杆。当前应用上电后执行固定的回零流程，然后持续运行舵机与推杆联动动作。

> **安全状态：**当前生成的 GPIO/中断代码没有把已配置的 PB14 编码器输入接入运动逻辑使用的回调。在解决[已知限制](#已知限制)并对完整机构进行安全测试前，应将该固件视为源码级原型。

## 当前行为

复位后，当前 `main.c` 执行链路如下：

1. 初始化 HAL、72 MHz 系统时钟、GPIO、USART1 和 USART2。
2. 等待 1 秒，然后执行两次阻塞式回缩，尝试建立推杆 HOME 位置。
3. 将两个 HTD-85H 舵机移动到配置的 HOME 目标值。
4. 连续执行三次自动联动动作。
5. 等待 6 秒，然后重新开始三次联动，永久循环。

USART2 当前只用于调试输出。Keil Target 中还编译了一个独立的蓝牙命令模块，但当前启动流程和主循环没有开启串口接收，也没有调用该模块，因此蓝牙命令不是可用的运行接口。

应用直接使用 STM32 HAL，采用阻塞式执行方式，没有使用 RTOS。

## 硬件与工具链

| 项目 | 工程中可确认的配置 |
| --- | --- |
| MCU | STM32F103C8T6，Cortex-M3，LQFP48 |
| 固件包 | STM32Cube FW_F1 V1.8.7 |
| 配置文件 | `Armrobot.ioc` |
| IDE 工程 | Keil MDK-ARM 工程 `MDK-ARM/Armrobot.uvprojx` |
| 配置的工具链 | MDK-ARM V5.32，Arm Compiler 6 |
| 现有构建日志中的编译器 | ArmClang 6.24 |
| 运行方式 | STM32 HAL，裸机 |

### 引脚与串口映射

| MCU 引脚 | 功能 | 应用用途 |
| --- | --- | --- |
| PA9 | USART1 TX，115200 8N1 | HTD-85H 舵机命令输出 |
| PA10 | USART1 RX，115200 8N1 | 已配置，但当前应用不读取 |
| PA2 | USART2 TX，9600 8N1 | 调试文本输出 |
| PA3 | USART2 RX，9600 8N1 | 已配置，但当前代码没有启动接收 |
| PA6 | 推挽输出 | L298N IN1 |
| PA7 | 推挽输出 | L298N IN2 |
| PB14 | 预期的 EXTI 输入 | 推杆编码器 A 相；当前生成源码没有初始化 |
| PA13 / PA14 | SWDIO / SWCLK | SWD 下载与调试 |
| PD0 / PD1 | HSE 振荡器 | 外部时钟输入/输出 |

不能使用 STM32 为推杆电机供电。应根据推杆规格选择电机电源和驱动器，并按照各器件数据手册连接控制器、L298N 逻辑、舵机接口、舵机电源和编码器所需的公共地。上电前必须确认电平、电机电流、端点停止和紧急停止方案。

## 自动动作流程

### 启动准备

每次启动回缩按以下逻辑执行：

1. 停止推杆并清零编码器计数。
2. 让推杆沿回缩方向至少运行 1200 ms。
3. 超过最短时间后，如果连续 300 ms 没有新编码器脉冲，则判定到达 HOME 端。
4. 停止、清零计数并将当前位置标记为 HOME=0；如果超过 8000 ms，则按失败停止。

固件连续执行两次该流程。任意一次报告超时后，应用都会进入保持推杆停止的死循环。两次均报告成功后，舵机才移动到 HOME。

### 循环动作

| 阶段 | 舵机目标值 `(ID1, ID2)` | 舵机运动时间 | 推杆动作 |
| --- | --- | --- | --- |
| HOME -> STEP1 | `(700, 590)` | 1500 ms | 伸出至 1750 个编码器脉冲 |
| STEP1 -> STEP2 | `(900, 533)` | 1500 ms | 回缩至 1750 个编码器脉冲 |
| STEP2 -> HOME | `(900, 533)` | 1500 ms | 推杆不动作 |

每次推杆目标最多等待 5000 ms，之后停止电机。每个舵机/推杆联动阶段保证至少经过 1800 ms，动作间隔 500 ms；整套动作连续执行三次，等待 6000 ms 后永久重复。

### 关键配置值

当前有效参数是 `Core/Src/main.c` 中的编译期宏。

| 宏 | 当前值 | 用途 |
| --- | --- | --- |
| `SERVO_ID_1`、`SERVO_ID_2` | `1`、`2` | HTD-85H 总线 ID |
| `SERVO_MOVE_TIME_MS` | `1500` | 舵机命令运动时间 |
| `SERVO_WAIT_MS` | `1800` | 每阶段最短持续时间 |
| `PUSHROD_TARGET_PULSE` | `1750` | 伸出/回缩目标计数 |
| `PUSHROD_HOME_MIN_RETRACT_MS` | `1200` | 启动时最短回缩驱动时间 |
| `PUSHROD_HOME_QUIET_MS` | `300` | 判定到达 HOME 的无脉冲时间 |
| `PUSHROD_HOME_TIMEOUT_MS` | `8000` | 启动回缩超时 |
| `SERVO_REPEAT_TIMES` | `3` | 每轮动作次数 |
| `SERVO_CYCLE_DELAY_MS` | `6000` | 每轮之间的等待时间 |

## 软件结构

| 路径 | 职责 |
| --- | --- |
| `Core/Src/main.c` | 当前启动回零、自动动作、调试输出、推杆目标循环和编码器回调 |
| `MDK-ARM/Hardware/htd85h_servo.c` | HTD-85H 组帧、校验、位置限幅和 UART 发送 |
| `MDK-ARM/Hardware/pushrod.c` | L298N 方向输出及共享的推杆/编码器状态 |
| `MDK-ARM/Hardware/app_control.c` | 旧的/未启用的蓝牙动作接口；参与编译但没有被当前流程调用 |
| `Core/Src/usart.c` | USART1 和 USART2 HAL 配置 |
| `Core/Src/gpio.c` | 当前生成的 PA6/PA7 输出配置 |
| `Drivers/` | 工程附带的 STM32F1 HAL 和 CMSIS 源码 |
| `MDK-ARM/` | Keil 工程、启动代码、硬件模块、设置和生成的构建产物 |

当前实际执行链路：

`main() -> Run_Startup_Prepare() -> Pushrod_Startup_Retract_Blocking() -> 舵机 HOME -> Run_ServoPushrod_Action_ThreeTimes() -> HTD85H_Move()/Pushrod_RunToTarget()`

预期的编码器链路：

`PB14 边沿 -> EXTI15_10 中断 -> HAL_GPIO_EXTI_Callback() -> g_encoder_count -> 目标/静默时间判定`

当前生成源码缺少中间的 IRQ 链路，详见下方限制。

## 构建

工程包含 Keil 项目，但没有命令行构建脚本。

1. 安装支持 Arm Compiler 6 的 Keil MDK 和 STM32F1 Device Pack。
2. 使用 µVision 打开 `MDK-ARM/Armrobot.uvprojx`。
3. 选择 `Armrobot` Target。
4. 通过 **Project > Build Target**（`F7`）构建。
5. 预期生成 `MDK-ARM/Armrobot/Armrobot.axf` 和 `MDK-ARM/Armrobot/Armrobot.hex`。

以上 GUI 步骤根据工程文件推导。本次文档编写期间，PATH 和默认本地安装路径中均未找到 µVision，因此没有提供未经验证的 CLI 构建命令。

### 现有构建证据

工程中日期为 2026-04-24 的现有构建日志记录：

- ArmClang 6.24
- `Code=14468`、`RO-data=1728`、`RW-data=12`、`ZI-data=1924`
- 已生成 AXF 和 HEX
- 0 个错误，0 个警告

这是工程内已有的构建记录，不是创建本 README 时重新构建得到的结果。构建通过不能验证中断链路或推杆实际行为。

## 已知限制

1. **编码器 EXTI 链路没有生成：**`Armrobot.ioc` 将 PB14 声明为 `GPXTI14` 并启用 `EXTI15_10_IRQn`，但 `Core/Src/gpio.c` 没有开启 GPIOB 或配置 PB14，`Core/Src/stm32f1xx_it.c` 也没有 `EXTI15_10_IRQHandler()`。因此 `main.c` 中的 `HAL_GPIO_EXTI_Callback()` 无法通过当前固件链路收到编码器边沿。
2. **编码器断线可能被判定为 HOME：**启动回零在超过最短回缩时间后把编码器静默解释为已到达 HOME。如果编码器断线或从未产生脉冲，也可能报告成功，而不是报告传感器故障。代码中没有独立 HOME 限位开关，也没有电流/堵转检测。
3. **目标超时没有向上传递：**`Pushrod_RunToTarget()` 在 5000 ms 超时后只记录日志并停止电机，没有返回成功/失败结果。调用者会继续进入下一阶段，效果等同于目标行程已经完成。
4. **STEP2 与 HOME 相同：**两者的舵机目标值均为 `(900, 533)`，因此注释中的 STEP2 到 HOME 阶段只会重复发送相同位置，不会切换到另一个舵机姿态。
5. **蓝牙控制未启用：**`app_control.c` 提供 `A/B/E/R/S/H` 命令并被列入 Keil 工程，但 `main.c` 既没有调用其 API，也没有启动 USART2 接收。`bt_rx_data` 在当前应用中未被使用。
6. **CubeMX 时钟冲突：**`Armrobot.ioc` 记录了 AHB 2 分频和 36 MHz HCLK，但当前执行的 `SystemClock_Config()` 使用 AHB 1 分频，因此在 8 MHz HSE、PLL 9 倍频下实际 HCLK 为 72 MHz。使用 CubeMX 重新生成代码前必须先消除该冲突。
7. **阻塞式控制循环：**推杆目标循环和动作延时会阻塞主循环。代码依赖中断更新 HAL Tick 和编码器计数，但应用中没有监督调度器或看门狗恢复路径。
8. **没有推杆或舵机到位确认：**HTD-85H 返回状态被忽略，程序不解析舵机应答，也没有独立证据确认命令姿态或推杆位置已经到达。
9. **没有自动化测试：**工程没有包含主机侧或目标板侧测试套件。

## 验证状态

| 验收层级 | 状态 |
| --- | --- |
| 源码与工程配置检查 | 创建 README 时已完成 |
| 固件构建产物 | 找到 2026-04-24 的 AXF/HEX 和零错误构建日志；本次未重新构建 |
| 编程器/调试器识别 | 未验证 |
| 固件烧录到 MCU | 未验证 |
| UART、编码器、舵机或推杆通信 | 未验证 |
| 实际运动与整机安全验收 | 未验证 |

不要让机构无人值守运行。必须先修复编码器中断和回零失败判定，再在固定可靠的实物上验证方向、端点、行程、电流、机械间隙、紧急停止和故障恢复。
