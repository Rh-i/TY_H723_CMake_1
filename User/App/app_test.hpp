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

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef __cplusplus
}
#endif

#endif // __APP_TEST_HPP__
