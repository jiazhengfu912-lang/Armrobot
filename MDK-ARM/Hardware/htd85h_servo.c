#include "htd85h_servo.h"

/**
 * @brief  计算协议校验和
 * @note   算法与 HTD-85H 官方示例一致：
 *         从 buf[2] 开始累加到 buf[3]+1，再按位取反
 */
uint8_t HTD85H_CheckSum(uint8_t buf[])
{
    uint8_t i;
    uint16_t temp = 0;

    for (i = 2; i < buf[3] + 2; i++)
    {
        temp += buf[i];
    }

    temp = ~temp;
    return (uint8_t)temp;
}

/**
 * @brief  发送原始协议帧
 */
HAL_StatusTypeDef HTD85H_SendFrame(UART_HandleTypeDef *huart, uint8_t *buf, uint16_t len)
{
    return HAL_UART_Transmit(huart, buf, len, 100);
}

/**
 * @brief  控制指定ID舵机转到目标位置
 */
HAL_StatusTypeDef HTD85H_Move(UART_HandleTypeDef *huart, uint8_t id, uint16_t position, uint16_t time_ms)
{
    uint8_t buf[10];

    /* 位置限幅，防止超范围 */
    if (position > 1000)
    {
        position = 1000;
    }

    buf[0] = HTD85H_FRAME_HEADER;
    buf[1] = HTD85H_FRAME_HEADER;
    buf[2] = id;
    buf[3] = 7;
    buf[4] = HTD85H_MOVE_TIME_WRITE;
    buf[5] = (uint8_t)(position & 0xFF);
    buf[6] = (uint8_t)((position >> 8) & 0xFF);
    buf[7] = (uint8_t)(time_ms & 0xFF);
    buf[8] = (uint8_t)((time_ms >> 8) & 0xFF);
    buf[9] = HTD85H_CheckSum(buf);

    return HTD85H_SendFrame(huart, buf, 10);
}