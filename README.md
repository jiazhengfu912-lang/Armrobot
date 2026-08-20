# Armrobot

English | [简体中文](README.zh-CN.md)

STM32F103C8 bare-metal firmware for an automated arm mechanism with two HTD-85H serial servos and an encoder-equipped linear actuator driven through an L298N. The active application performs a fixed startup homing routine and then repeats an automatic servo/actuator sequence.

> **Safety status:** the current generated GPIO/interrupt code does not connect the configured PB14 encoder input to the callback used by the motion logic. Treat this firmware as source-level prototype code until the issues in [Known limitations](#known-limitations) are resolved and the complete mechanism is tested safely.

## Current behavior

After reset, the active `main.c` path:

1. Initializes HAL, the 72 MHz system clock, GPIO, USART1, and USART2.
2. Waits 1 second, then commands two blocking retract attempts to establish a linear-actuator HOME position.
3. Moves both HTD-85H servos to the configured HOME targets.
4. Runs three automatic synchronized action repetitions.
5. Waits 6 seconds and starts the three-repetition cycle again indefinitely.

USART2 is currently used for debug output. A separate Bluetooth command module is compiled into the Keil target, but the active startup and main loop do not start UART reception or call that module, so Bluetooth commands are not a working runtime interface.

The application is blocking and uses the STM32 HAL directly; no RTOS is present.

## Hardware and toolchain

| Item | Configuration evidenced by the project |
| --- | --- |
| MCU | STM32F103C8T6, Cortex-M3, LQFP48 |
| Firmware package | STM32Cube FW_F1 V1.8.7 |
| Configuration | `Armrobot.ioc` |
| IDE project | Keil MDK-ARM project `MDK-ARM/Armrobot.uvprojx` |
| Configured toolchain | MDK-ARM V5.32 with Arm Compiler 6 |
| Compiler in the existing build log | ArmClang 6.24 |
| Runtime | STM32 HAL, bare metal |

### Pin and serial map

| MCU pin | Function | Application role |
| --- | --- | --- |
| PA9 | USART1 TX, 115200 8N1 | HTD-85H servo command output |
| PA10 | USART1 RX, 115200 8N1 | Configured but not consumed by the active application |
| PA2 | USART2 TX, 9600 8N1 | Debug text output |
| PA3 | USART2 RX, 9600 8N1 | Configured, but active code does not start reception |
| PA6 | Push-pull output | L298N IN1 |
| PA7 | Push-pull output | L298N IN2 |
| PB14 | Intended EXTI input | Linear-actuator encoder A phase; not initialized by the current generated source |
| PA13 / PA14 | SWDIO / SWCLK | SWD programming and debugging |
| PD0 / PD1 | HSE oscillator | External clock input/output |

The actuator motor must not be powered from the STM32. Use a motor supply and driver appropriate for the actuator, and connect the controller, L298N logic, servo interface, servo supply, and encoder grounds as required by their datasheets. Confirm voltage levels, motor current, end-stop behavior, and emergency-stop arrangements before applying power.

## Automatic sequence

### Startup preparation

Each startup retract attempt follows this logic:

1. Stop the actuator and clear the encoder counter.
2. Drive in the retract direction for at least 1200 ms.
3. After the minimum time, treat more than 300 ms with no new encoder pulse as the HOME end.
4. Stop, clear the count, and mark HOME as zero, or stop with failure after an 8000 ms timeout.

The firmware performs this routine twice. Either reported timeout locks the application in a loop with the actuator stopped. After both attempts report success, the servos move to HOME.

### Repeating action

| Stage | Servo targets `(ID1, ID2)` | Servo move time | Actuator action |
| --- | --- | --- | --- |
| HOME -> STEP1 | `(700, 590)` | 1500 ms | Extend toward 1750 encoder pulses |
| STEP1 -> STEP2 | `(900, 533)` | 1500 ms | Retract toward 1750 encoder pulses |
| STEP2 -> HOME | `(900, 533)` | 1500 ms | No actuator movement |

The code waits up to 5000 ms for each actuator target, then stops the motor. It maintains at least 1800 ms of elapsed time for each combined servo/actuator stage, inserts 500 ms action gaps, repeats the sequence three times, waits 6000 ms, and repeats forever.

### Key configuration values

The active values are compile-time macros in `Core/Src/main.c`.

| Macro | Current value | Purpose |
| --- | --- | --- |
| `SERVO_ID_1`, `SERVO_ID_2` | `1`, `2` | HTD-85H bus IDs |
| `SERVO_MOVE_TIME_MS` | `1500` | Commanded servo move time |
| `SERVO_WAIT_MS` | `1800` | Minimum stage duration |
| `PUSHROD_TARGET_PULSE` | `1750` | Extend/retract target count |
| `PUSHROD_HOME_MIN_RETRACT_MS` | `1200` | Minimum startup retract drive time |
| `PUSHROD_HOME_QUIET_MS` | `300` | No-pulse interval interpreted as HOME |
| `PUSHROD_HOME_TIMEOUT_MS` | `8000` | Startup retract timeout |
| `SERVO_REPEAT_TIMES` | `3` | Actions per cycle |
| `SERVO_CYCLE_DELAY_MS` | `6000` | Delay between cycles |

## Software structure

| Path | Responsibility |
| --- | --- |
| `Core/Src/main.c` | Active startup homing, automatic sequence, debug output, actuator target loops, and encoder callback |
| `MDK-ARM/Hardware/htd85h_servo.c` | HTD-85H frame encoding, checksum, position limiting, and UART transmission |
| `MDK-ARM/Hardware/pushrod.c` | L298N direction outputs and shared actuator/encoder state |
| `MDK-ARM/Hardware/app_control.c` | Legacy/dormant Bluetooth action interface; compiled but not called by the active path |
| `Core/Src/usart.c` | USART1 and USART2 HAL configuration |
| `Core/Src/gpio.c` | Current generated PA6/PA7 output configuration |
| `Drivers/` | STM32F1 HAL and CMSIS sources supplied with the project |
| `MDK-ARM/` | Keil project, startup code, hardware modules, settings, and generated build artifacts |

Representative active flow:

`main() -> Run_Startup_Prepare() -> Pushrod_Startup_Retract_Blocking() -> servo HOME -> Run_ServoPushrod_Action_ThreeTimes() -> HTD85H_Move()/Pushrod_RunToTarget()`

The intended encoder flow is:

`PB14 edge -> EXTI15_10 IRQ -> HAL_GPIO_EXTI_Callback() -> g_encoder_count -> target/quiet-time decision`

The middle IRQ path is missing from the current generated source; see the limitations below.

## Build

The repository contains a Keil project but no command-line build script.

1. Install Keil MDK with Arm Compiler 6 support and the STM32F1 device pack.
2. Open `MDK-ARM/Armrobot.uvprojx` in µVision.
3. Select the `Armrobot` target.
4. Build the target with **Project > Build Target** (`F7`).
5. Expected outputs are `MDK-ARM/Armrobot/Armrobot.axf` and `MDK-ARM/Armrobot/Armrobot.hex`.

These GUI steps are inferred from the checked project file. No verified CLI command is supplied because µVision was not available on PATH or at the default local installation path during this documentation pass.

### Existing build evidence

The checked-in/generated build log dated 2026-04-24 records:

- ArmClang 6.24
- `Code=14468`, `RO-data=1728`, `RW-data=12`, `ZI-data=1924`
- AXF and HEX generation
- 0 errors and 0 warnings

This is an existing build record, not a rebuild performed while creating this README. A clean build does not validate interrupt routing or physical actuator behavior.

## Known limitations

1. **Encoder EXTI path is not generated:** `Armrobot.ioc` declares PB14 as `GPXTI14` and enables `EXTI15_10_IRQn`, but `Core/Src/gpio.c` does not enable GPIOB or configure PB14, and `Core/Src/stm32f1xx_it.c` has no `EXTI15_10_IRQHandler()`. Therefore the `HAL_GPIO_EXTI_Callback()` in `main.c` cannot receive encoder edges through the current firmware path.
2. **Disconnected encoder can be accepted as HOME:** startup homing interprets encoder silence after the minimum retract time as reaching HOME. If the encoder is disconnected or never produces pulses, the same condition can report success instead of a sensor failure. There is no independent HOME limit switch or current/stall detection in the code.
3. **Target timeout is not propagated:** `Pushrod_RunToTarget()` logs a 5000 ms timeout and stops the motor but returns no success/failure result. The caller continues to the next stage as if the requested travel completed.
4. **STEP2 and HOME are identical:** both use servo targets `(900, 533)`, so the documented STEP2-to-HOME stage sends the same positions and does not command a different servo pose.
5. **Bluetooth control is dormant:** `app_control.c` provides `A/B/E/R/S/H` commands and is listed in the Keil project, but `main.c` neither calls its API nor starts USART2 reception. `bt_rx_data` is unused in the active application.
6. **CubeMX clock conflict:** `Armrobot.ioc` records an AHB divide-by-2 configuration and 36 MHz HCLK, while the active `SystemClock_Config()` uses an AHB divide-by-1 configuration and therefore runs HCLK at 72 MHz from the 8 MHz HSE and PLL x9. Resolve this before regenerating code with CubeMX.
7. **Blocking control loops:** actuator target loops and motion delays block the main loop. Interrupts are intended to update the HAL tick and encoder count, but there is no supervisory scheduler or watchdog recovery path in the application.
8. **No actuator or servo confirmation:** HTD-85H return statuses are ignored, servo responses are not parsed, and the system has no independent confirmation that a commanded pose or actuator position was reached.
9. **No automated tests:** no host-side or target-side test suite is included.

## Validation status

| Acceptance layer | Status |
| --- | --- |
| Source and project configuration inspected | Completed during README creation |
| Firmware artifact generated | Existing 2026-04-24 AXF/HEX and zero-error build log found; not rebuilt in this pass |
| Programmer/debugger detected | Not verified |
| Firmware flashed to the MCU | Not verified |
| UART, encoder, servo, or actuator communication observed | Not verified |
| Physical motion and complete system safety accepted | Not verified |

Do not run the mechanism unattended. Resolve the encoder interrupt and homing-failure behavior first, then validate direction, end stops, travel limits, current, mechanical clearance, emergency stopping, and fault recovery on secured hardware.
