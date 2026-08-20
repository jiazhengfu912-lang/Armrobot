#ifndef __HTD85H_SERVO_H__
#define __HTD85H_SERVO_H__

#include "main.h"

/* HTD-85H 协议帧头 */
#define HTD85H_FRAME_HEADER        0x55

/* 写入目标位置和时间命令 */
#define HTD85H_MOVE_TIME_WRITE     0x01

/**
 * @brief  计算 HTD-85H 协议校验和
 * @param  buf 协议缓冲区
 * @retval 8位校验值
 */
uint8_t HTD85H_CheckSum(uint8_t buf[]);

/**
 * @brief  发送一帧原始数据
 * @param  huart 串口句柄
 * @param  buf   数据缓冲区
 * @param  len   数据长度
 * @retval HAL状态
 */
HAL_StatusTypeDef HTD85H_SendFrame(UART_HandleTypeDef *huart, uint8_t *buf, uint16_t len);

/**
 * @brief  控制指定ID舵机运动到指定位置
 * @param  huart    串口句柄，当前工程里应传 &huart1
 * @param  id       舵机ID
 * @param  position 目标位置，范围 0~1000
 * @param  time_ms  运行时间，单位 ms
 * @retval HAL状态
 */
HAL_StatusTypeDef HTD85H_Move(UART_HandleTypeDef *huart, uint8_t id, uint16_t position, uint16_t time_ms);

#endif