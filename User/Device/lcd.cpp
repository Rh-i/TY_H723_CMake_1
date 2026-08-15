#include "lcd.hpp"

#include "lcd_font.hpp" // 字库数据（namespace lcd_font）
#include "main.h"       // GPIO 引脚宏
#include "spi.h"        // hspi1

/* ==================== 编译期配置（原 lcd.h 宏） ==================== */

///< 0/1 = 竖屏，2/3 = 横屏（文件内静态配置；LCD 无构造参数，屏幕方向如需运行时切换可改 Config 传入）
static constexpr int k_use_horizontal = 1;


/* ==================== 底层读写 ==================== */

/**
 * @brief 写 1 字节（普通硬件 SPI，阻塞发送）
 */
void Lcd::writ_bus(uint8_t dat)
{
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(&hspi1, &dat, 1, 0xffff);
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
}

/**
 * @brief 写 8 位数据
 */
void Lcd::wr_data8(uint8_t dat)
{
  writ_bus(dat);
}

/**
 * @brief 写 16 位数据（大端）
 */
void Lcd::wr_data(uint16_t dat)
{
  writ_bus(uint8_t(dat >> 8));
  writ_bus(uint8_t(dat));
}

/**
 * @brief 写寄存器地址
 */
void Lcd::wr_reg(uint8_t dat)
{
  HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_RESET); // 写命令
  writ_bus(dat);
  HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET); // 写数据
}

/**
 * @brief 设置显示区域地址范围（根据屏幕方向）
 */
void Lcd::address_set(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
  if (k_use_horizontal == 0 || k_use_horizontal == 1)
  {
    wr_reg(0x2a); // 列地址设置
    wr_data(x1);
    wr_data(x2);
    wr_reg(0x2b); // 行地址设置
    wr_data(y1 + 20);
    wr_data(y2 + 20);
    wr_reg(0x2c); // 储存器写
  }
  else
  {
    wr_reg(0x2a); // 列地址设置
    wr_data(x1 + 20);
    wr_data(x2 + 20);
    wr_reg(0x2b); // 行地址设置
    wr_data(y1);
    wr_data(y2);
    wr_reg(0x2c); // 储存器写
  }
}


/* ==================== 绘图 ==================== */

/**
 * @brief 指定区域填充颜色
 */
void Lcd::fill(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, Color color)
{
  uint16_t i, j;
  address_set(x0, y0, x1 - 1, y1 - 1); // 设置显示范围
  for (i = y0; i < y1; i++)
  {
    for (j = x0; j < x1; j++)
    {
      wr_data(static_cast<uint16_t>(color));
    }
  }
}

/**
 * @brief 画点
 */
void Lcd::draw_point(uint16_t x, uint16_t y, Color color)
{
  address_set(x, y, x, y); // 设置光标位置
  wr_data(static_cast<uint16_t>(color));
}

/**
 * @brief 画线（Bresenham）
 */
void Lcd::draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, Color color)
{
  uint16_t t;
  int      xerr = 0, yerr = 0, delta_x, delta_y, distance;
  int      incx, incy, uRow, uCol;

  delta_x = x2 - x1; // 计算坐标增量
  delta_y = y2 - y1;
  uRow    = x1; // 画线起点坐标
  uCol    = y1;
  if (delta_x > 0)
    incx = 1; // 设置单步方向
  else if (delta_x == 0)
    incx = 0; // 垂直线
  else
  {
    incx    = -1;
    delta_x = -delta_x;
  }
  if (delta_y > 0)
    incy = 1;
  else if (delta_y == 0)
    incy = 0; // 水平线
  else
  {
    incy    = -1;
    delta_y = -delta_y;
  }
  if (delta_x > delta_y)
    distance = delta_x; // 选取基本增量坐标轴
  else
    distance = delta_y;
  for (t = 0; t < distance + 1; t++)
  {
    draw_point(uRow, uCol, color); // 画点
    xerr += delta_x;
    yerr += delta_y;
    if (xerr > distance)
    {
      xerr -= distance;
      uRow += incx;
    }
    if (yerr > distance)
    {
      yerr -= distance;
      uCol += incy;
    }
  }
}

/**
 * @brief 画矩形（四条边）
 */
void Lcd::draw_rectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, Color color)
{
  draw_line(x1, y1, x2, y1, color);
  draw_line(x1, y1, x1, y2, color);
  draw_line(x1, y2, x2, y2, color);
  draw_line(x2, y1, x2, y2, color);
}

/**
 * @brief 画圆（中点画圆法）
 */
void Lcd::draw_circle(uint16_t x0, uint16_t y0, uint8_t r, Color color)
{
  int a = 0, b = r;
  while (a <= b)
  {
    draw_point(x0 - b, y0 - a, color); // 3
    draw_point(x0 + b, y0 - a, color); // 0
    draw_point(x0 - a, y0 + b, color); // 1
    draw_point(x0 - a, y0 - b, color); // 2
    draw_point(x0 + b, y0 + a, color); // 4
    draw_point(x0 + a, y0 - b, color); // 5
    draw_point(x0 + a, y0 + b, color); // 6
    draw_point(x0 - b, y0 + a, color); // 7
    a++;
    if ((a * a + b * b) > (r * r)) // 判断要画的点是否过远
    {
      b--;
    }
  }
}


/* ==================== 字符与字符串 ==================== */

/**
 * @brief 显示单个字符
 */
void Lcd::show_char(uint16_t x, uint16_t y, uint8_t num, Color fc, Color bc, uint8_t sizey, uint8_t mode)
{
  uint8_t  temp, sizex, t, m = 0;
  uint16_t i, TypefaceNum; // 一个字符所占字节大小
  uint16_t x0 = x;
  sizex       = sizey / 2;
  TypefaceNum = (sizex / 8 + ((sizex % 8) ? 1 : 0)) * sizey;
  num         = num - ' ';                         // 得到偏移后的值
  address_set(x, y, x + sizex - 1, y + sizey - 1); // 设置光标位置
  for (i = 0; i < TypefaceNum; i++)
  {
    if (sizey == 12)
      temp = lcd_font::ascii_1206[num][i]; // 调用6x12字体
    else if (sizey == 16)
      temp = lcd_font::ascii_1608[num][i]; // 调用8x16字体
    else if (sizey == 24)
      temp = lcd_font::ascii_2412[num][i]; // 调用12x24字体
    else if (sizey == 32)
      temp = lcd_font::ascii_3216[num][i]; // 调用16x32字体
    else
      return;
    for (t = 0; t < 8; t++)
    {
      if (!mode) // 非叠加模式
      {
        if (temp & (0x01 << t))
          wr_data(static_cast<uint16_t>(fc));
        else
          wr_data(static_cast<uint16_t>(bc));
        m++;
        if (m % sizex == 0)
        {
          m = 0;
          break;
        }
      }
      else // 叠加模式
      {
        if (temp & (0x01 << t))
          draw_point(x, y, fc); // 画一个点
        x++;
        if ((x - x0) == sizex)
        {
          x = x0;
          y++;
          break;
        }
      }
    }
  }
}

/**
 * @brief 显示字符串
 */
void Lcd::show_string(uint16_t x, uint16_t y, const char *s, Color fc, Color bc, uint8_t sizey, uint8_t mode)
{
  while (*s != '\0')
  {
    show_char(x, y, static_cast<uint8_t>(*s), fc, bc, sizey, mode);
    x += sizey / 2;
    s++;
  }
}

/**
 * @brief 求幂
 */
uint32_t Lcd::mypow(uint8_t m, uint8_t n)
{
  uint32_t result = 1;
  while (n--)
    result *= m;
  return result;
}

/**
 * @brief 显示整数
 */
void Lcd::show_int_num(uint16_t x, uint16_t y, uint16_t num, uint8_t len, Color fc, Color bc, uint8_t sizey)
{
  uint8_t t, temp;
  uint8_t enshow = 0;
  uint8_t sizex  = sizey / 2;
  for (t = 0; t < len; t++)
  {
    temp = (num / mypow(10, len - t - 1)) % 10;
    if (enshow == 0 && t < (len - 1))
    {
      if (temp == 0)
      {
        show_char(x + t * sizex, y, ' ', fc, bc, sizey, 0);
        continue;
      }
      else
        enshow = 1;
    }
    show_char(x + t * sizex, y, temp + 48, fc, bc, sizey, 0);
  }
}

/**
 * @brief 显示十六进制数字
 */
void Lcd::show_hex_num(uint16_t x, uint16_t y, uint16_t num, uint8_t len, Color fc, Color bc, uint8_t sizey)
{
  uint8_t    t, temp;
  uint8_t    enshow      = 0;
  uint8_t    sizex       = sizey / 2;
  const char hex_chars[] = "0123456789ABCDEF";
  for (t = 0; t < len; t++)
  {
    temp = (num / mypow(16, len - t - 1)) % 16;
    if (enshow == 0 && t < (len - 1))
    {
      if (temp == 0)
      {
        show_char(x + t * sizex, y, ' ', fc, bc, sizey, 0);
        continue;
      }
      else
        enshow = 1;
    }
    show_char(x + t * sizex, y, static_cast<uint8_t>(hex_chars[temp]), fc, bc, sizey, 0);
  }
}

/**
 * @brief 显示带符号浮点数
 */
void Lcd::show_float_num(uint16_t x, uint16_t y, float num, uint8_t len, uint8_t decimal, Color fc, Color bc, uint8_t sizey)
{
  int16_t num_int;
  uint8_t t, temp, sizex;
  sizex   = sizey / 2;
  num_int = static_cast<int16_t>(num * mypow(10, decimal));

  if (num < 0)
  {
    show_char(x, y, '-', fc, bc, sizey, 0);
    num_int = -num_int;
    x += sizex;
    len++;
  }
  else
  {
    show_char(x, y, ' ', fc, bc, sizey, 0);
    x += sizex;
    len++;
  }

  // 在更新数字时刷新显示的位置
  fill(x, y, x + len * sizex + decimal + 1, y + sizey + 1, bc);

  for (t = 0; t < len; t++)
  {
    if (t == (len - decimal))
    {
      show_char(x + (len - decimal) * sizex, y, '.', fc, bc, sizey, 0);
      t++;
      len += 1;
    }
    temp = static_cast<uint8_t>(((num_int / mypow(10, len - t - 1)) % 10) + '0');
    show_char(x + t * sizex, y, temp, fc, bc, sizey, 0);
  }
}

/**
 * @brief 显示正浮点数
 */
void Lcd::show_float_num1(uint16_t x, uint16_t y, float num, uint8_t len, uint8_t decimal, Color fc, Color bc, uint8_t sizey)
{
  int16_t num_int;
  uint8_t t, temp, sizex;
  sizex   = sizey / 2;
  num_int = static_cast<int16_t>(num * mypow(10, decimal));

  x += sizex;
  len++;

  // 在更新数字时刷新显示的位置
  fill(x, y, x + len * sizex + decimal + 1, y + sizey + 1, bc);

  for (t = 0; t < len; t++)
  {
    if (t == (len - decimal))
    {
      show_char(x + (len - decimal) * sizex, y, '.', fc, bc, sizey, 0);
      t++;
      len += 1;
    }
    temp = static_cast<uint8_t>(((num_int / mypow(10, len - t - 1)) % 10) + '0');
    show_char(x + t * sizex, y, temp, fc, bc, sizey, 0);
  }
}

/**
 * @brief 显示图片
 */
void Lcd::show_picture(uint16_t x, uint16_t y, uint16_t length, uint16_t width, const uint8_t pic[])
{
  uint16_t i, j;
  uint32_t k = 0;
  address_set(x, y, x + length - 1, y + width - 1);
  for (i = 0; i < length; i++)
  {
    for (j = 0; j < width; j++)
    {
      wr_data8(pic[k * 2]);
      wr_data8(pic[k * 2 + 1]);
      k++;
    }
  }
}


/* ==================== 初始化 ==================== */

/**
 * @brief 短忙延时（100 万次空循环，约 10ms 量级，具体取决于主频；原 lcd.c 时序）
 */
static inline void lcd_delay()
{
  for (volatile uint32_t i = 0; i < 1000000U; i++)
  {
  }
}

/**
 * @brief LCD 初始化（复位、背光、寄存器配置）
 */
Status Lcd::init()
{
  HAL_GPIO_WritePin(LCD_RES_GPIO_Port, LCD_RES_Pin, GPIO_PIN_RESET); // 复位
  lcd_delay();
  HAL_GPIO_WritePin(LCD_RES_GPIO_Port, LCD_RES_Pin, GPIO_PIN_SET);
  lcd_delay();

  HAL_GPIO_WritePin(LCD_BLK_GPIO_Port, LCD_BLK_Pin, GPIO_PIN_SET); // 打开背光
  lcd_delay();

  // ************* Start Initial Sequence **********//
  wr_reg(0x11); // Sleep out
  lcd_delay();

  wr_reg(0x36);
  if (k_use_horizontal == 0)
  {
    wr_data8(0x00);
  }
  else if (k_use_horizontal == 1)
  {
    wr_data8(0xC0);
  }
  else if (k_use_horizontal == 2)
  {
    wr_data8(0x70);
  }
  else
  {
    wr_data8(0xA0);
  }

  wr_reg(0x3A);
  wr_data8(0x05);

  wr_reg(0xB2);
  wr_data8(0x0C);
  wr_data8(0x0C);
  wr_data8(0x00);
  wr_data8(0x33);
  wr_data8(0x33);

  wr_reg(0xB7);
  wr_data8(0x35);

  wr_reg(0xBB);
  wr_data8(0x32); // Vcom=1.35V

  wr_reg(0xC2);
  wr_data8(0x01);

  wr_reg(0xC3);
  wr_data8(0x15); // GVDD=4.8V  颜色深度

  wr_reg(0xC4);
  wr_data8(0x20); // VDV, 0x20:0v

  wr_reg(0xC6);
  wr_data8(0x0F); // 0x0F:60Hz

  wr_reg(0xD0);
  wr_data8(0xA4);
  wr_data8(0xA1);

  wr_reg(0xE0);
  wr_data8(0xD0);
  wr_data8(0x08);
  wr_data8(0x0E);
  wr_data8(0x09);
  wr_data8(0x09);
  wr_data8(0x05);
  wr_data8(0x31);
  wr_data8(0x33);
  wr_data8(0x48);
  wr_data8(0x17);
  wr_data8(0x14);
  wr_data8(0x15);
  wr_data8(0x31);
  wr_data8(0x34);

  wr_reg(0xE1);
  wr_data8(0xD0);
  wr_data8(0x08);
  wr_data8(0x0E);
  wr_data8(0x09);
  wr_data8(0x09);
  wr_data8(0x15);
  wr_data8(0x31);
  wr_data8(0x33);
  wr_data8(0x48);
  wr_data8(0x17);
  wr_data8(0x14);
  wr_data8(0x15);
  wr_data8(0x31);
  wr_data8(0x34);

  wr_reg(0x21);

  wr_reg(0x29);

  return Status::OK;
}
