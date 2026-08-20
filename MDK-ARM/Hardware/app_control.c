#include "app_control.h"
#include "usart.h"
#include "htd85h_servo.h"
#include "pushrod.h"
#include <string.h>

/* 内部静态函数声明 */
static void App_ActionGroup1(void);
static void App_ActionGroup2(void);
static void App_BluetoothPrint(const char *str);

/**
 * @brief  蓝牙串口打印字符串
 * @note   这里默认 huart2 为蓝牙串口
 */
static void App_BluetoothPrint(const char *str)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)str, (uint16_t)strlen(str), 100);
}

/**
 * @brief  动作组1
 * @note   这是上电/复位后默认执行的一套动作
 *         你后面可以按实际机械结构再改位置和时间
 */
static void App_ActionGroup1(void)
{
    /* 先控制两个舵机分别到不同位置 */
    HTD85H_Move(&huart1, SERVO_ID_1, 300, 600);
    HTD85H_Move(&huart1, SERVO_ID_2, 700, 600);
    HAL_Delay(900);

    /* 推杆伸出 */
    Pushrod_Extend();
    HAL_Delay(1500);
    Pushrod_Stop();
    HAL_Delay(300);

    /* 两个舵机执行第二步动作 */
    HTD85H_Move(&huart1, SERVO_ID_1, 550, 600);
    HTD85H_Move(&huart1, SERVO_ID_2, 450, 600);
    HAL_Delay(900);

    /* 推杆缩回 */
    Pushrod_Retract();
    HAL_Delay(1500);
    Pushrod_Stop();
    HAL_Delay(300);
}

/**
 * @brief  动作组2
 * @note   这是蓝牙可触发的另一套动作
 */
static void App_ActionGroup2(void)
{
    HTD85H_Move(&huart1, SERVO_ID_1, 150, 500);
    HTD85H_Move(&huart1, SERVO_ID_2, 850, 500);
    HAL_Delay(800);

    Pushrod_Extend();
    HAL_Delay(1000);
    Pushrod_Stop();
    HAL_Delay(300);

    HTD85H_Move(&huart1, SERVO_ID_1, 500, 500);
    HTD85H_Move(&huart1, SERVO_ID_2, 500, 500);
    HAL_Delay(800);

    Pushrod_Retract();
    HAL_Delay(1000);
    Pushrod_Stop();
    HAL_Delay(300);
}

/**
 * @brief  上电后 / 复位后 自动执行动作序列
 */
void App_RunStartupSequence(void)
{
    /* 上电延时：
     * 给舵机调试板、蓝牙模块、L298N 供电稳定预留时间
     */
    HAL_Delay(1000);

    /* 先确保推杆处于停止状态，避免上电时误动作 */
    Pushrod_Stop();

    /* 蓝牙提示信息，方便测试时观察 */
    App_BluetoothPrint("\r\nSystem Reset / Power On\r\n");
    App_BluetoothPrint("Running startup action...\r\n");

    /* 执行默认动作组 */
    App_ActionGroup1();

    App_BluetoothPrint("Startup action done.\r\n");
    App_SendBluetoothHelp();
}

/**
 * @brief  发送蓝牙帮助菜单
 */
void App_SendBluetoothHelp(void)
{
    App_BluetoothPrint("\r\n==== Bluetooth Command Menu ====\r\n");
    App_BluetoothPrint("A : Run Action Group 1\r\n");
    App_BluetoothPrint("B : Run Action Group 2\r\n");
    App_BluetoothPrint("E : Pushrod Extend\r\n");
    App_BluetoothPrint("R : Pushrod Retract\r\n");
    App_BluetoothPrint("S : Pushrod Stop\r\n");
    App_BluetoothPrint("H : Show Help\r\n");
    App_BluetoothPrint("================================\r\n");
}

/**
 * @brief  处理蓝牙单字节命令
 */
void App_ProcessBluetoothCommand(uint8_t cmd)
{
    switch (cmd)
    {
        case BT_CMD_ACTION1:
        case 'a':
            App_BluetoothPrint("Run Action Group 1\r\n");
            App_ActionGroup1();
            break;

        case BT_CMD_ACTION2:
        case 'b':
            App_BluetoothPrint("Run Action Group 2\r\n");
            App_ActionGroup2();
            break;

        case BT_CMD_EXTEND:
        case 'e':
            App_BluetoothPrint("Pushrod Extend\r\n");
            Pushrod_Extend();
            break;

        case BT_CMD_RETRACT:
        case 'r':
            App_BluetoothPrint("Pushrod Retract\r\n");
            Pushrod_Retract();
            break;

        case BT_CMD_STOP:
        case 's':
            App_BluetoothPrint("Pushrod Stop\r\n");
            Pushrod_Stop();
            break;

        case BT_CMD_HELP:
        case 'h':
            App_SendBluetoothHelp();
            break;

        default:
            App_BluetoothPrint("Unknown command. Press H for help.\r\n");
            break;
    }
}