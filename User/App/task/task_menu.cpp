#include "FreeRTOS.h" // IWYU pragma: keep
#include "semphr.h"
#include "task.h"

#include "api_main.h"  // IWYU pragma: keep
#include "bsp_cfg.hpp" // key_user / bsp_uart1
#include "device_cfg.hpp" // lcd
#include "menu.hpp"    // 菜单模块接口

/**
 * @brief 菜单任务（由 api_main 的 all_init() 创建）
 *
 * 首次运行初始化菜单（LCD）；之后轮询按键：
 *   短按 → menu_on_short_press() 切换选中项
 *   长按 → menu_on_long_press() 触发当前选中项
 *
 * @param argument 未使用（NULL）
 */
extern "C" void task_menu(void *argument)
{
  (void)argument;

  // 初始化 LCD 并绘制菜单
  menu_init();

  for (;;)
  {
    BspKey::Event e = key_user.poll();

    switch (e)
    {
      case BspKey::Event::PRESSED:
        // 按下时不做处理，等释放计数
        break;

      case BspKey::Event::RELEASED:
        // 短按：切换选中项
        menu_on_short_press();
        break;

      case BspKey::Event::LONG_PRESS:
        // 长按：触发当前选中项
        menu_on_long_press();
        break;

      case BspKey::Event::NONE:
      default:
        break;
    }

    vTaskDelay(pdMS_TO_TICKS(200));
  }
}
