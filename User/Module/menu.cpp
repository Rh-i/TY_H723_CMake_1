#include "menu.hpp"

#include "FreeRTOS.h" // IWYU pragma: keep
#include "task.h"
#include "semphr.h"

#include "api_main.h"    // menu_sem 声明
#include "bsp_cfg.hpp"    // bsp_buzzer
#include "device_cfg.hpp" // lcd 全局实例（device_init 中已初始化）

/* ==================================================================
 *  菜单项定义
 * ================================================================== */

#define MENU_ITEMS 3         ///< 菜单项数量
#define MENU_ITEM_Y0 10      ///< 第一个任务文本顶部 y（贴紧屏幕顶部）
#define MENU_ITEM_SPACING 60 ///< 任务行间距 (px)

/* 文字尺寸（sizey=24，字符宽 12px） */
#define MENU_FONT 24
#define MENU_ARROW_X 64 ///< ">" 箭头 x
#define MENU_TEXT_X 88  ///< 任务文字 x

static const char *menu_items[MENU_ITEMS] = {"TASK_1", "TASK_2", "TASK_3"};
static uint8_t     menu_sel               = 0; ///< 当前选中项（0-2）

/* ==================================================================
 *  绘制（无背景填充：统一黑底 + 白字白箭头）
 * ================================================================== */

/**
 * @brief 画 / 擦 选中箭头（白色 ">"；擦除 = 将箭头区域填回底色黑）
 * @param idx  菜单项索引
 * @param show true=显示箭头，false=隐藏（擦除）
 */
static void menu_draw_arrow(uint8_t idx, bool show)
{
  uint16_t y = MENU_ITEM_Y0 + idx * MENU_ITEM_SPACING;

  // 先擦箭头区域（12×24 字符范围 +1px 余量，填回底色），避免残留
  lcd.fill(MENU_ARROW_X - 1, y - 1, MENU_ARROW_X + MENU_FONT / 2 + 1, y + MENU_FONT + 1, Lcd::BLACK);
  if (show)
    lcd.show_string(MENU_ARROW_X, y, ">", Lcd::WHITE, Lcd::BLACK, MENU_FONT, 1);
}

/**
 * @brief 绘制任务文字（始终白色，不反色、不改色；叠加模式只画笔画）
 * @param idx 菜单项索引
 */
static void menu_draw_item(uint8_t idx)
{
  uint16_t y = MENU_ITEM_Y0 + idx * MENU_ITEM_SPACING;
  lcd.show_string(MENU_TEXT_X, y, menu_items[idx], Lcd::WHITE, Lcd::BLACK, MENU_FONT, 1);
}

/**
 * @brief 全量绘制：清一次底色 + 3 个任务文字 + 当前选中箭头
 */
static void menu_draw_all(void)
{
  lcd.fill(0, 0, Lcd::WIDTH, Lcd::HEIGHT, Lcd::BLACK); // 统一深色底（无色块）
  for (uint8_t i = 0; i < MENU_ITEMS; i++)
    menu_draw_item(i);
  menu_draw_arrow(menu_sel, true); // 初始选中箭头
}

/* ==================================================================
 *  对外接口
 * ================================================================== */

/**
 * @brief 初始化菜单：绘制首屏（LCD 硬件初始化在 device_init() 中完成）
 */
void menu_init(void)
{
  menu_draw_all();
}

void menu_on_short_press(void)
{
  // 切换选中项（1→2→3→1），蜂鸣提示
  menu_sel = (menu_sel + 1) % MENU_ITEMS; // 新选中项
  bsp_buzzer.beep(3000, 80);

  // 先擦除所有箭头，再显示新选中箭头（文字始终白色，无需改动）
  for (uint8_t i = 0; i < MENU_ITEMS; i++)
    menu_draw_arrow(i, false);
  menu_draw_arrow(menu_sel, true);
}

void menu_on_long_press(void)
{
  // ── 确认动画：选中箭头闪两下（亮→灭→亮→灭→亮）──
  for (uint8_t i = 0; i < 2; i++)
  {
    menu_draw_arrow(menu_sel, true);
    vTaskDelay(pdMS_TO_TICKS(120));
    menu_draw_arrow(menu_sel, false);
    vTaskDelay(pdMS_TO_TICKS(120));
  }
  menu_draw_arrow(menu_sel, true); // 结束保持显示

  bsp_buzzer.beep(4000, 500); // 长提示音

  // ── 触发当前选中项（give 对应 menu_sem）──
  xSemaphoreGive(menu_sem[menu_sel]);
}
