/**
 * @file emm_frame.hpp
 * @author Rh
 * @brief 无状态帧构建助手（2.4）—— 编译期内联，零成本
 * @version 0.1
 * @date 2026-08-13
 *
 * @copyright Copyright (c) 2026
 *
 * @details 每条命令变成 3~5 行、一眼可对协议手册：
 *
 * @code
 *   EmmFrame f;
 *   f.u8(_addr); f.u8(0xF6); f.u8(dir); f.u16(vel); f.u8(acc); f.u8(snF); f.finish();
 *   _send_cmd(f.buf, f.len);
 * @endcode
 */

#ifndef __EMM_FRAME_HPP__
#define __EMM_FRAME_HPP__

#include <stddef.h>
#include <stdint.h>

#include "device_emmv5.hpp" // EMMV5_CHECKSUM

/**
 * @brief Emm_V5 帧构建助手（无状态，编译期内联）
 */
struct EmmFrame
{
  uint8_t buf[32]; ///< 帧缓冲区（最大命令约 20 字节）
  size_t  len = 0; ///< 当前帧长度

  ///< 写入 1 字节（超界保护）
  void u8(uint8_t v)
  {
    if (len < sizeof(buf)) buf[len++] = v;
  }

  ///< 写入 2 字节（大端）
  void u16(uint16_t v)
  {
    u8(uint8_t(v >> 8));
    u8(uint8_t(v));
  }

  ///< 写入 4 字节（大端）
  void u32(uint32_t v)
  {
    u8(uint8_t(v >> 24));
    u8(uint8_t(v >> 16));
    u8(uint8_t(v >> 8));
    u8(uint8_t(v));
  }

  ///< 追加固定校验字节
  void finish()
  {
    u8(EMMV5_CHECKSUM);
  }
};

#endif // __EMM_FRAME_HPP__
