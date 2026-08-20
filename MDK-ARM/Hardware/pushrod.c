#include "pushrod.h"

/* 当前推杆方向 */
volatile uint8_t g_pushrod_dir = PUSHROD_DIR_STOP;

/* 编码器累计计数 */
volatile uint32_t g_encoder_count = 0;

/* 到位停止标志 */
volatile uint8_t g_pushrod_stop_flag = 0;

/**
 * @brief  推杆停止
 * @note   IN1=0, IN2=0
 */
void Pushrod_Stop(void)
{
    HAL_GPIO_WritePin(PUSHROD_IN1_GPIO_Port, PUSHROD_IN1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(PUSHROD_IN2_GPIO_Port, PUSHROD_IN2_Pin, GPIO_PIN_RESET);

    g_pushrod_dir = PUSHROD_DIR_STOP;
}

/**
 * @brief  推杆伸出
 * @note   IN1=1, IN2=0
 *         如果实际方向反了，把 Extend / Retract 对调即可
 */
void Pushrod_Extend(void)
{
    HAL_GPIO_WritePin(PUSHROD_IN1_GPIO_Port, PUSHROD_IN1_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(PUSHROD_IN2_GPIO_Port, PUSHROD_IN2_Pin, GPIO_PIN_RESET);

    g_pushrod_dir = PUSHROD_DIR_EXTEND;
}

/**
 * @brief  推杆缩回
 * @note   IN1=0, IN2=1
 */
void Pushrod_Retract(void)
{
    HAL_GPIO_WritePin(PUSHROD_IN1_GPIO_Port, PUSHROD_IN1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(PUSHROD_IN2_GPIO_Port, PUSHROD_IN2_Pin, GPIO_PIN_SET);

    g_pushrod_dir = PUSHROD_DIR_RETRACT;
}

/**
 * @brief  编码器计数清零
 */
void Pushrod_ResetEncoder(void)
{
    g_encoder_count = 0;
    g_pushrod_stop_flag = 0;
}