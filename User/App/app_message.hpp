/**
 * @file app_message.hpp
 * @author Rh
 * @brief 数据分发任务声明 —— 占位文件（当前无业务代码，仅任务壳）
 * @version 0.1
 * @date 2026-08-14
 *
 * @copyright Copyright (c) 2026
 *
 * @details 未来在此实现数据分发任务（接入业务后填充）：
 *          - CAN 分发任务：一路 CAN 挂多个设备（如 CAN1 上的多个电机 + DM IMU），
 *            按帧 ID 路由到对应设备回调
 *          - UART 分发任务：一个串口挂多个设备（如一个 UART 上的多个 EmmV5 电机），
 *            按帧首字节地址路由到对应设备回调
 *
 * @note 任务函数以 extern "C" 声明（FreeRTOS 以 C 方式调用）；
 *       任务实现位于 User/App/app_message.cpp，接入业务后在 all_init() 中创建。
 */

#ifndef __APP_MESSAGE_HPP__
#define __APP_MESSAGE_HPP__

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief CAN 数据分发任务
   * @param argument 任务参数（未使用，NULL）
   */
  void app_message_can_task(void *argument);

  /**
   * @brief UART 数据分发任务
   * @param argument 任务参数（未使用，NULL）
   */
  void app_message_uart_task(void *argument);

#ifdef __cplusplus
}
#endif

#endif // __APP_MESSAGE_HPP__
