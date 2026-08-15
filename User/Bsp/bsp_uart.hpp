/**
 * @file bsp_uart.hpp
 * @author Rh
 * @brief 实现了一个简易的串口驱动（FreeRTOS）（只接收最新数据不能用FIFO）
 * @version 0.2
 * @date 2026-08-09
 *
 * @todo 1. 接收到的数据,需要在应用层的app_message写分发处理
 *
 * @copyright Copyright (c) 2026
 *
 * @details 使用示例：（必须要在freertos的任务中运行收发 中断中不行 中断不能阻塞）
           （使用的IDLE中断进行接收 发送也是同理）
 *
 * @note 模板实例化实现 以及类的实例化 第一个数字为缓冲区大小（uint8_t） 第二个数字为消息队列的长度（uint8_t）
 *
 *   // 全局实例化模板在bsp_uart.cpp中
 *   template class BspUart<128,8>;
 *
 *   // 全局实例化类 在bsp_cfg.cpp中
 *   __attribute__((section(".dma_buffer")))
 *   BspUart<128,8> bsp_uart1({&huart1, ReceiveMode::SING *   bsp_uart1.init();                           // 放到bsp_init中初始化串口   // 需要freerto使用
 *。串口设备只有流缓冲区/消息邮箱有内置的锁，没有主动写的mutex,需要自己处理冲突情况
 * @ *    bsp_uart1.receive(buffer,8);               // 从接收的缓冲区里面读数据，读取后对应数据会被清空
 * // 从自动中断接收的缓冲区里面接收，无需处理直接拿
 *    bsp_uart1.send(buffer,8);                  // 存入发送缓冲区，然后自动发送
 *    bsp_uart1.printf("val=%d\r\n", 42);        // 格式化输出（非阻塞，DMA发送）
 *
 */

#ifndef __BSP_UART_HPP__
#define __BSP_UART_HPP__

#include "FreeRTOS.h" // IWYU pragma: keep
#include "queue.h"
#include "stream_buffer.h"
#include "task.h"  // IWYU pragma: keep
#include "usart.h" // IWYU pragma: keep

#include "status.hpp" // 统一状态码


/**
 * @brief 接收模式枚举
 *
 */
enum class ReceiveMode
{
  LATEST_ONLY   = 1, // 仅保留最新一次接收到的数据（使用消息邮箱）（不能开FIFO）
  SINGLE_BUFFER = 2, // 使用单个流缓冲区
};


// 模板的第一个数字为缓冲区大小（单位uint8_t） 第二个数字为消息队列的长度（uint8_t）
template <size_t BUFFER_SIZE = 256, size_t MSG_SIZE = 8>
class BspUart
{

private:
  UART_HandleTypeDef *_huart;                  ///< UART句柄指针，指向底层硬件接口
  QueueHandle_t       _msg_queue_id = nullptr; ///< FreeRTOS消息队列句柄，用于LATEST_ONLY模式

  StreamBufferHandle_t _rx_stream_buffer = nullptr; ///< 接收流缓冲区（SINGLE_BUFFER 模式）

  StreamBufferHandle_t _tx_stream_buffer = nullptr; ///< FreeRTOS发送流缓冲区句柄

  ReceiveMode _receive_mode;                 ///< 接收模式，指定数据接收策略
  bool        _rx_active = false;            ///< 接收状态标志，指示是否正在接收数据
  uint8_t     _rx_dma_buffer[BUFFER_SIZE];   ///< DMA接收缓冲区，用于多字节接收
  uint8_t     _tx_dma_buffer[BUFFER_SIZE];   ///< DMA发送缓冲区，用于多字节发送
  char        _printf_buffer[BUFFER_SIZE];   ///< printf 格式化缓冲区（vsnprintf 输出到此处）
  size_t      _buffer_size    = BUFFER_SIZE; ///< 缓冲区大小，单位字节
  size_t      _msg_item_size  = MSG_SIZE;    ///< 消息队列中每个项目的大小
  bool        _transmit_enable;              ///< 是否启用发送
  uint32_t    _last_received_length = 0;     ///< 最后一次接收的数据长度
  int         _instance_id;                  ///< 实例ID，用于生成唯一资源名称
  char        _msgq_name[32];                ///< 实例消息队列的名字，用于调试时看到名字


public:
  /**
   * @brief 串口配置结构体（可匿名按序传入）
   */
  struct Config
  {
    /**
     * @brief 按序构造配置（参数顺序 = 字段顺序）
     */
    Config(UART_HandleTypeDef *huart = nullptr, ReceiveMode rx_mode = ReceiveMode::SINGLE_BUFFER,
           bool transmit_enable = true, int instance_id = 0)
      : huart(huart),
        rx_mode(rx_mode),
        transmit_enable(transmit_enable),
        instance_id(instance_id)
    {
    }

    UART_HandleTypeDef *huart;           ///< UART 句柄
    ReceiveMode         rx_mode;         ///< 接收模式
    bool                transmit_enable; ///< 是否启用发送
    int                 instance_id;     ///< 实例 ID（生成唯一资源名称）
  };

  /**
   * @brief 构造函数
   *
   * @note 初始化串口驱动对象，配置必要的FreeRTOS对象
   *
   * @param cfg 串口配置（huart/接收模式/发送使能/实例ID，可匿名按序传入）
   */
  BspUart(const Config &cfg);

  /**
   * @brief 初始化函数 初始化串口驱动对象，配置必要的FreeRTOS对象
   *
   * @return Status OK=初始化成功，IO_ERROR=FreeRTOS资源创建失败
   */
  Status init();

  // 析构函数 释放所有分配的资源
  ~BspUart();

  /**
   * @brief 发送数据 将数据放入发送缓冲区，并启动DMA传输。
   *
   * @param data 要发送的数据指针
   * @param size 数据大小（不能超过缓冲区容量，否则返回 BAD_ARG）
   * @param written 实际写入的字节数（可为 nullptr）
   * @param timeout 超时时间（ticks）
   *
   * @return Status OK=全部写入，TIMEOUT=超时部分写入，
   *                BAD_ARG=参数非法或超长，IO_ERROR=未初始化
   */
  Status send(const uint8_t *data, size_t size, size_t *written = nullptr, uint32_t timeout = portMAX_DELAY);

  /**
   * @brief 格式化输出到本串口（printf 风格）
   *
   * @note 内部 vsnprintf 格式化后调用 send()，非阻塞（DMA 发送）；
   *       格式化结果超过缓冲区（BUFFER_SIZE）时自动截断。
   *       必须在任务上下文调用。
   *
   * @param fmt 格式化字符串
   * @param ... 可变参数
   * @return Status OK=发送成功，其余同 send()
   */
  Status printf(const char *fmt, ...);

  /**
   * @brief 接收数据 根据接收模式从相应的缓冲区读取数据
   *
   * @param buffer 接收数据的缓冲区
   * @param size 请求读取的数据大小
   * @param received 实际读取的字节数（可为 nullptr）
   * @param timeout 超时时间（ticks）
   * @return Status OK=读到数据，TIMEOUT=超时或无数据，
   *                BAD_ARG=参数非法，IO_ERROR=缓冲区未创建
   */
  Status receive(uint8_t *buffer, size_t size, size_t *received = nullptr, uint32_t timeout = portMAX_DELAY);

  /**
   * @brief 获取发送缓冲区剩余空间
   *
   * @return size_t 剩余空间大小
   */
  size_t get_tx_free_space();

  /**
   * @brief 获取接收缓冲区可用数据量
   *
   * @return size_t 可用数据量
   */
  size_t get_rx_available_data();

  /**
   * @brief DMA错误回调函数 由HAL库调用，处理DMA错误事件
   *
   * @param huart UART句柄
   */
  void dma_error_callback(UART_HandleTypeDef *huart);

  /**
   * @brief IDLE中断处理函数（ISR上下文）
   *
   * @param received_length 接收到的数据长度
   * @param pxHigherPriorityTaskWoken 需初始化为pdFALSE，若唤醒高优先级任务则置为pdTRUE
   */
  void handle_idle_interrupt_from_isr(uint32_t received_length, BaseType_t *pxHigherPriorityTaskWoken);

  /**
   * @brief 内部IDLE中断处理函数（ISR上下文）
   *
   * @param huart UART句柄
   * @param size 接收到的数据长度
   * @param pxHigherPriorityTaskWoken 需初始化为pdFALSE，若唤醒高优先级任务则置为pdTRUE
   */
  void handle_idle_interrupt_internal(UART_HandleTypeDef *huart, uint16_t size, BaseType_t *pxHigherPriorityTaskWoken);

  /**
   * @brief TX发送完成处理函数（ISR上下文）
   * @param pxHigherPriorityTaskWoken 需初始化为pdFALSE，若唤醒高优先级任务则置为pdTRUE
   */
  void handle_tx_complete_from_isr(BaseType_t *pxHigherPriorityTaskWoken);

  /**
   * @brief 重启 DMA 接收（任务上下文调用）
   *
   * @note 在接收任务中，读完流缓冲区数据后调用此函数重启 DMA。
   *       不能从 ISR 调用 —— ISR 只负责停 DMA + 投递数据，
   *       重启 DMA 推迟到任务上下文，避免 ISR 耗时过长。
   */
  void restart_rx();

private:
  // 开始接收数据 启动DMA接收
  void start_reception();

  // 清理资源 清理所有分配的资源
  void cleanup_resources();

  // 停止接收数据 停止DMA接收
  void stop_reception();

  // 开始传输数据 启动DMA发送（任务上下文）
  void start_transmission();

  // 开始传输数据 启动DMA发送（ISR上下文）
  void start_transmission_from_isr(BaseType_t *pxHigherPriorityTaskWoken);

  /**
   * @brief 检查是否正在传输
   *
   * @return true 正在传输
   * @return false 未在传输
   */
  bool is_transmitting();

  // 处理DMA错误 记录错误并尝试重新初始化
  void handle_dma_error();
};


#endif // __BSP_UART_HPP__
