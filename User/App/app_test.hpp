/**
 * @file app_test.hpp
 * @author Rh
 * @brief 应用层测试任务声明
 * @version 0.1
 * @date 2026-08-14
 *
 * @copyright Copyright (c) 2026
 *
 * @note 测试任务函数均以 extern "C" 声明（FreeRTOS 以 C 方式调用）。
 *       任务实现位于 User/App/test/，由 all_init() 统一创建。
 *       msg_task_task1/2/3：各阻塞等待 menu_sem[0/1/2]，
 *       由 menu 模块长按触发，触发后经 USART1 发送测试字节。
 */

#ifndef __APP_TEST_HPP__
#define __APP_TEST_HPP__

/** @brief 编译并创建 Online 实机自检任务；设为 0 可停用。 */
#define APP_TEST_ONLINE_CHECK_ENABLED 0

/** @brief 编译并创建 CAN1 ID2 M3508 低电流转动测试；设为 0 可停用。 */
#define APP_TEST_DJI_MOTOR_ENABLED 0

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Online 状态机自检任务
   * @param argument 任务参数（未使用，NULL）
   * @note 仅在 APP_TEST_ONLINE_CHECK_ENABLED 非零时创建。
   */
  void online_check_test_task(void *argument);

  /**
   * @brief CAN1 ID2 M3508 低电流实机测试任务
   * @param argument 任务参数（未使用，NULL）
   * @note 仅在 APP_TEST_DJI_MOTOR_ENABLED 非零时创建。
   * @warning 启用后每次复位都会在启动 2 秒后驱动电机 1 秒，测试时必须架空或固定电机。
   */
  void dji_motor_test_task(void *argument);

#ifdef __cplusplus
}
#endif

#endif // __APP_TEST_HPP__
