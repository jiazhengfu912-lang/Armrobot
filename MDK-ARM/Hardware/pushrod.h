#ifndef __PUSHROD_H__
#define __PUSHROD_H__

#include "main.h"

/* -------------------------
 * L298N 控制引脚定义
 * PA6 -> IN1
 * PA7 -> IN2
 * ------------------------- */
#define PUSHROD_IN1_GPIO_Port    GPIOA
#define PUSHROD_IN1_Pin          GPIO_PIN_6

#define PUSHROD_IN2_GPIO_Port    GPIOA
#define PUSHROD_IN2_Pin          GPIO_PIN_7

/* -------------------------
 * 推杆运动方向标记
 * 0 : 停止
 * 1 : 伸出
 * 2 : 缩回
 * ------------------------- */
#define PUSHROD_DIR_STOP         0
#define PUSHROD_DIR_EXTEND       1
#define PUSHROD_DIR_RETRACT      2

/* 当前推杆方向 */
extern volatile uint8_t g_pushrod_dir;

/* 编码器累计计数 */
extern volatile uint32_t g_encoder_count;

/* 到位停止标志 */
extern volatile uint8_t g_pushrod_stop_flag;

/**
 * @brief  推杆停止
 */
void Pushrod_Stop(void);

/**
 * @brief  推杆伸出
 */
void Pushrod_Extend(void);

/**
 * @brief  推杆缩回
 */
void Pushrod_Retract(void);

/**
 * @brief  编码器计数清零
 */
void Pushrod_ResetEncoder(void);

#endif