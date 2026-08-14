/**
 * @file Lcd.hpp
 * @author Rh
 * @brief LCD 驱动 —— 纯 C++ 类（2.8：不保留任何 C 层，LCD_* 全局函数全部消失）
 * @version 0.1
 * @date 2026-08-13
 *
 * @copyright Copyright (c) 2026
 *
 * @details 由原 lcd.c / lcd.h / lcdfont.h 重构而来：
 *          - 全部函数收敛进 Lcd 类，业务层只允许通过全局实例 lcd 访问；
 *          - 字库数据移入 namespace lcd_font（LcdFont.hpp），仅 Lcd.cpp 使用；
 *          - 常量（宽高、颜色）收敛为类静态常量 / 枚举。
 *
 * @note 使用示例：
 * @code
 *   lcd.init();
 *   lcd.fill(0, 0, Lcd::WIDTH, Lcd::HEIGHT, Lcd::BLACK);
 *   lcd.show_string(88, 10, "TASK_1", Lcd::WHITE, Lcd::BLACK, 24, 1);
 * @endcode
 */

#ifndef __LCD_HPP__
#define __LCD_HPP__

#include <stdint.h>
#include <stddef.h>

#include "status.hpp" // 统一状态码


/**
 * @brief LCD 驱动类（ST7789/ILI9341 类 240x280 竖屏，硬件 SPI）
 */
class Lcd
{
public:
  static constexpr uint16_t WIDTH  = 240; ///< 屏幕宽度（像素，竖屏）
  static constexpr uint16_t HEIGHT = 280; ///< 屏幕高度（像素，竖屏）

  /** 常用颜色（RGB565） */
  enum Color : uint16_t
  {
    WHITE       = 0xFFFF,
    BLACK       = 0x0000,
    BLUE        = 0x001F,
    BRED        = 0xF81F,
    GRED        = 0xFFE0,
    GBLUE       = 0x07FF,
    RED         = 0xF800,
    MAGENTA     = 0xF81F,
    GREEN       = 0x07E0,
    CYAN        = 0x7FFF,
    YELLOW      = 0xFFE0,
    BROWN       = 0xBC40,
    BRRED       = 0xFC07,
    GRAY        = 0x8430,
    DARKBLUE    = 0x01CF,
    LIGHTBLUE   = 0x7D7C,
    GRAYBLUE    = 0x5458,
    LIGHTGREEN  = 0x841F,
    LGRAY       = 0xC618,
    LGRAYBLUE   = 0xA651,
    LBBLUE      = 0x2B12,
  };

  /**
   * @brief LCD 初始化（复位、背光、寄存器配置）
   *
   * @return Status OK=初始化完成
   */
  Status init();

  /**
   * @brief 指定区域填充颜色
   * @param x0 区域左上角 x 坐标
   * @param y0 区域左上角 y 坐标
   * @param x1 区域右下角 x 坐标
   * @param y1 区域右下角 y 坐标
   * @param color 填充颜色（RGB565，可用 Lcd::WHITE 等枚举）
   */
  void fill(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);

  /**
   * @brief 画点
   * @param x 点 x 坐标
   * @param y 点 y 坐标
   * @param color 点的颜色（RGB565）
   */
  void draw_point(uint16_t x, uint16_t y, uint16_t color);

  /**
   * @brief 画线（Bresenham 直线）
   * @param x1 起点 x 坐标
   * @param y1 起点 y 坐标
   * @param x2 终点 x 坐标
   * @param y2 终点 y 坐标
   * @param color 线条颜色（RGB565）
   */
  void draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);

  /**
   * @brief 画矩形（四条边）
   * @param x1 左上角 x 坐标
   * @param y1 左上角 y 坐标
   * @param x2 右下角 x 坐标
   * @param y2 右下角 y 坐标
   * @param color 边框颜色（RGB565）
   */
  void draw_rectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);

  /**
   * @brief 画圆（中点画圆法，仅描边不填充）
   * @param x0 圆心 x 坐标
   * @param y0 圆心 y 坐标
   * @param r  半径（像素）
   * @param color 圆周颜色（RGB565）
   */
  void draw_circle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color);

  /**
   * @brief 显示单个 ASCII 字符
   * @param x 字符左上角 x 坐标
   * @param y 字符左上角 y 坐标
   * @param num 字符（ASCII 码，如 'A'、'1'）
   * @param fc  前景色（字符笔画颜色，RGB565）
   * @param bc  背景色（RGB565）
   * @param sizey 字高，仅支持 12 / 16 / 24 / 32（字宽 = 字高一半）
   * @param mode 0=非叠加（画前景也画背景），1=叠加（只画前景点，保留原背景）
   */
  void show_char(uint16_t x, uint16_t y, uint8_t num, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode);

  /**
   * @brief 显示字符串（'\0' 结尾，自动横向排开）
   * @param x 首字符左上角 x 坐标
   * @param y 首字符左上角 y 坐标
   * @param s 字符串指针
   * @param fc 前景色（RGB565）
   * @param bc 背景色（RGB565）
   * @param sizey 字高，仅支持 12 / 16 / 24 / 32
   * @param mode 0=非叠加，1=叠加
   */
  void show_string(uint16_t x, uint16_t y, const char *s, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode);

  /**
   * @brief 显示无符号整数（高位不足补空格）
   * @param x 首字符左上角 x 坐标
   * @param y 首字符左上角 y 坐标
   * @param num 要显示的数字
   * @param len 显示总位数（超过实际位数时高位补空格）
   * @param fc 前景色（RGB565）
   * @param bc 背景色（RGB565）
   * @param sizey 字高，仅支持 12 / 16 / 24 / 32
   */
  void show_int_num(uint16_t x, uint16_t y, uint16_t num, uint8_t len, uint16_t fc, uint16_t bc, uint8_t sizey);

  /**
   * @brief 显示带符号浮点数（负数带 '-'，正数前导空格对齐）
   * @param x 首字符左上角 x 坐标
   * @param y 首字符左上角 y 坐标
   * @param num 要显示的浮点数
   * @param len 整数部分显示位数（不足补空格）
   * @param decimal 小数位数
   * @param fc 前景色（RGB565）
   * @param bc 背景色（RGB565）
   * @param sizey 字高，仅支持 12 / 16 / 24 / 32
   */
  void show_float_num(uint16_t x, uint16_t y, float num, uint8_t len, uint8_t decimal, uint16_t fc, uint16_t bc, uint8_t sizey);

  /**
   * @brief 显示非负浮点数（无符号位，右移一位对齐）
   * @param x 首字符左上角 x 坐标
   * @param y 首字符左上角 y 坐标
   * @param num 要显示的浮点数（应 >= 0）
   * @param len 整数部分显示位数（不足补空格）
   * @param decimal 小数位数
   * @param fc 前景色（RGB565）
   * @param bc 背景色（RGB565）
   * @param sizey 字高，仅支持 12 / 16 / 24 / 32
   */
  void show_float_num1(uint16_t x, uint16_t y, float num, uint8_t len, uint8_t decimal, uint16_t fc, uint16_t bc, uint8_t sizey);

  /**
   * @brief 显示十六进制数字（大写，高位不足补空格）
   * @param x 首字符左上角 x 坐标
   * @param y 首字符左上角 y 坐标
   * @param num 要显示的数值
   * @param len 显示总位数（hex 位数）
   * @param fc 前景色（RGB565）
   * @param bc 背景色（RGB565）
   * @param sizey 字高，仅支持 12 / 16 / 24 / 32
   */
  void show_hex_num(uint16_t x, uint16_t y, uint16_t num, uint8_t len, uint16_t fc, uint16_t bc, uint8_t sizey);

  /**
   * @brief 显示图片（RGB565 双字节像素）
   * @param x 图片左上角 x 坐标
   * @param y 图片左上角 y 坐标
   * @param length 图片长度（x 方向像素数）
   * @param width  图片宽度（y 方向像素数）
   * @param pic 图片数据（每像素 2 字节，大端 RGB565）
   */
  void show_picture(uint16_t x, uint16_t y, uint16_t length, uint16_t width, const uint8_t pic[]);

private:
  ///< 写 1 字节（硬件 SPI）
  void writ_bus(uint8_t dat);

  ///< 写 8 位数据
  void wr_data8(uint8_t dat);

  ///< 写 16 位数据（大端）
  void wr_data(uint16_t dat);

  ///< 写寄存器地址
  void wr_reg(uint8_t dat);

  ///< 设置显示区域地址范围
  void address_set(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);

  ///< 求幂
  static uint32_t mypow(uint8_t m, uint8_t n);
};

// 全局实例 lcd 在 device_cfg.hpp 中 extern 声明，在 device_cfg.cpp 中统一实例化。

#endif // __LCD_HPP__
