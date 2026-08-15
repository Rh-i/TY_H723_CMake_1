/**
 * @file menu.hpp
 * @author Rh
 * @brief LCD 菜单模块 —— 3 个菜单项，短按切换、长按触发对应 menu_sem。半成品、测试可用
 * @version 0.1
 * @date 2026-08-09
 *
 * @copyright Copyright (c) 2026
 *
 * @details 菜单逻辑全部在本模块：
 *
 *   - menu_init()         ：绘制首屏（LCD 硬件初始化在 device_init() 中完成）
 *   - menu_on_short_press()：短按切换选中项（1→2→3→1），蜂鸣提示
 *   - menu_on_long_press() ：长按触发当前选中项（give menu_sem[sel]），蜂鸣提示
 *
 *   menu_sem[0/1/2] 分别对应菜单项 1/2/3，
 *   由 msg_task.cpp 的 msg_task_task1/2/3 消费。
 *
 */

#ifndef __MENU_HPP__
#define __MENU_HPP__

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief 初始化 LCD 并绘制菜单（首次进入任务时调用一次）
   */
  void menu_init(void);

  /**
   * @brief 短按事件：切换选中项（循环 1→2→3→1）
   */
  void menu_on_short_press(void);

  /**
   * @brief 长按事件：触发当前选中项（give 对应 menu_sem）
   */
  void menu_on_long_press(void);

#ifdef __cplusplus
}
#endif

#endif // __MENU_HPP__
