/**
 * @file bsp_usb.hpp
 * @author ChoseB (ChoseB@cumt.edu.cn)
 * @brief TinyUSB 的 BSP 封装接口。
 * @version 0.1
 * @date 2026-08-13
 *
 * @copyright Copyright (c) 2026
 *
 * @details
 *
 * 该文件将 USB 设备抽象为一个类对象，向上层提供：
 * - 初始化与周期任务驱动
 * - CDC 收发接口
 * - 连接状态查询
 * - CDC 接收回调注册
 */

#ifndef __BSP_USB_HPP__
#define __BSP_USB_HPP__

#include <stdbool.h>
#include <stdint.h>


/**
 * @class BspUsb
 * @brief USB BSP 单例类，负责 TinyUSB Device CDC 的初始化与数据收发。
 */
class BspUsb
{
public:
  /**
   * @brief CDC 接收回调函数类型。
   * @param data 本次接收的数据指针。
   * @param len 本次接收的数据长度（字节）。
   * @param user_ctx 用户上下文指针，由 set_rx_callback 传入。
   */
  typedef void (*RxCallback)(const uint8_t* data, uint32_t len, void* user_ctx);

  /**
   * @brief 获取 USB BSP 单例对象。
   * @return BspUsb& 单例引用。
   */
  static BspUsb& instance();

  /**
   * @brief 初始化 TinyUSB Device 栈。
   *
   * @note 建议在 RTOS 启动后、USB 任务开始前调用一次。
   */
  void init();

  /**
   * @brief USB 周期任务函数。
   *
   * @details
   * 该函数应在独立任务中高频调用（例如 1ms 周期），用于：
   * - 驱动 TinyUSB 状态机
   * - 轮询 CDC 接收数据
   * - 将接收数据分发给已注册回调
   */
  void task();

  /**
   * @brief 查询 USB CDC 是否就绪可通信。
   * @return true 已挂载且满足发送门控条件；false 否则。
   */
  bool is_ready() const;

  /**
   * @brief 设置发送前是否要求 CDC DTR 置位。
   * @param enable true 要求 DTR；false 不要求 DTR，仅要求已挂载。
   *
   * @note
   * 某些串口调试助手不会主动拉高 DTR。若遇到已枚举但收不到数据，
   * 建议关闭该限制（默认已关闭）。
   */
  void set_require_dtr(bool enable);

  /**
   * @brief 查询设备是否已被主机枚举挂载。
   * @return true 已挂载；false 未挂载。
   */
  bool mounted() const;

  /**
   * @brief 通过 CDC 发送数据。
   * @param data 待发送数据指针。
   * @param len 待发送长度（字节）。
   * @return true 全部发送成功；false 未连接或未全部写入。
   */
  bool cdc_write(const uint8_t* data, uint32_t len);

  /**
   * @brief 通过 CDC 读取数据。
   * @param data 输出缓冲区指针。
   * @param len 最多读取长度（字节）。
   * @return uint32_t 实际读取字节数。
   */
  uint32_t cdc_read(uint8_t* data, uint32_t len);

  /**
   * @brief 获取 CDC 接收缓冲区内可读字节数。
   * @return uint32_t 可读字节数。
   */
  uint32_t cdc_available() const;

  /**
   * @brief 注册 CDC 接收回调。
   * @param cb 回调函数指针，传入 nullptr 可注销回调。
   * @param user_ctx 用户上下文指针，将原样传回回调。
   */
  void set_rx_callback(RxCallback cb, void* user_ctx);

private:
  /**
   * @brief 处理 CDC 接收轮询并分发回调。
   */
  void process_rx();

  BspUsb() = default;

  RxCallback _rx_callback = nullptr;
  void*      _rx_user_ctx = nullptr;
  bool       _require_dtr = false;
};

#endif