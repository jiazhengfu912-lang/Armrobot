#ifndef __APP_CONTROL_H__
#define __APP_CONTROL_H__

#include "main.h"

/* =========================
 * 舵机ID定义
 * 请按你的实际舵机ID修改
 * ========================= */
#define SERVO_ID_1      1
#define SERVO_ID_2      2

/* =========================
 * 蓝牙命令字符定义
 * ========================= */
#define BT_CMD_ACTION1  'A'   /* 执行动作组1 */
#define BT_CMD_ACTION2  'B'   /* 执行动作组2 */
#define BT_CMD_EXTEND   'E'   /* 推杆伸出 */
#define BT_CMD_RETRACT  'R'   /* 推杆缩回 */
#define BT_CMD_STOP     'S'   /* 推杆停止 */
#define BT_CMD_HELP     'H'   /* 打印帮助信息 */

/**
 * @brief  应用初始化后的动作
 * @note   用于上电后 / 按复位键后 自动执行
 */
void App_RunStartupSequence(void);

/**
 * @brief  处理蓝牙接收到的单字节命令
 * @param  cmd: 接收到的命令字符
 */
void App_ProcessBluetoothCommand(uint8_t cmd);

/**
 * @brief  通过蓝牙串口输出帮助信息
 */
void App_SendBluetoothHelp(void);

#endif