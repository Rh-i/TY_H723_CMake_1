/**
 * @file BspUart.cpp
 * @author Rh
 * @brief 实现了一个简易的串口驱动（FreeRTOS）（只接收最新数据不能用FIFO）
 * @version 0.2
 * @date 2026-02-08
 *
 * @todo 1. 目前使用查表法存入回调函数中，可能查询速度会慢，希望后人处理。
 *       2. 接收到的数据,需要在服务层写分发处理
 *       3. 双缓冲区测试过，效果不理想
 *
 * @copyright Copyright (c) 2026
 *
 *
 * @details 使用示例：（必须要在freertos的任务中运行收发 中断中不行 中断不能阻塞）
           （使用的IDLE中断进行接收 发送也是同理）
 *
 * @note 模板实例化实现 以及类的实例化 第一个数字为缓冲区大小（uint8_t） 第二个数字为消息队列的长度（uint8_t）
 *
 *   // 全局实例化模板在bsp_uart.cpp中
 *   template class BspUart<64,8>;
 *
 *   // 全局实例化类 在bsp_cfg.cpp中
 *   __attribute__((section(".dma_buffer")))
 *   BspUart<64,8> bsp_usart6(&huart6, ReceiveMode::SINGLE_BUFFER, true);
 *
 *   bsp_usart6.init();                           // 需要freertos内核初始化成功之后使用
 *
 * @note extern好之后，在任务中使用
 *
 *    bsp_usart6.receive(buffer,8,osWaitForever); // 从自动中断接收的缓冲区里面接收，无需处理直接拿
 *    bsp_usart6.send(buffer,8);                  // 存入发送缓冲区，然后自动发送
 *
 */

#ifndef __BSP_UART_HPP__
#define __BSP_UART_HPP__

#include "FreeRTOS.h" // IWYU pragma: keep
#include "task.h"     // IWYU pragma: keep
#include "stream_buffer.h"
#include "queue.h"
#include "usart.h" // IWYU pragma: keep


/**
 * @brief 接收模式枚举
 *
 */
enum class ReceiveMode
{
  LATEST_ONLY   = 1, // 仅保留最新一次接收到的数据（使用消息邮箱）（不能开FIFO）
  SINGLE_BUFFER = 2, // 使用单个流缓冲区
  DOUBLE_BUFFER = 3  // 使用双流缓冲区机制
};


// 模板的第一个数字为缓冲区大小（单位uint8_t） 第二个数字为消息队列的长度（uint8_t）
template <size_t BUFFER_SIZE = 256, size_t MSG_SIZE = 8>
class BspUart
{

private:
  UART_HandleTypeDef  *_huart;                                     ///< UART句柄指针，指向底层硬件接口
  QueueHandle_t        _msg_queue_id         = nullptr;            ///< FreeRTOS消息队列句柄，用于LATEST_ONLY模式
  StreamBufferHandle_t _rx_stream_buffers[2] = {nullptr, nullptr}; ///< 接收流缓冲区数组，[0]为单缓冲或双缓冲第一个，[1]为双缓冲第二个
  StreamBufferHandle_t _tx_stream_buffer     = nullptr;            ///< FreeRTOS发送流缓冲区句柄
  ReceiveMode          _receive_mode;                              ///< 接收模式，指定数据接收策略
  bool                 _rx_active = false;                         ///< 接收状态标志，指示是否正在接收数据
  uint8_t              _rx_dma_buffer[BUFFER_SIZE];                ///< DMA接收缓冲区，用于多字节接收
  uint8_t              _tx_dma_buffer[BUFFER_SIZE];                ///< DMA发送缓冲区，用于多字节发送
  bool                 _current_buffer = false;                    ///< 当前使用的流缓冲区标识，true表示使用buffer2，false表示buffer1
  size_t               _buffer_size    = BUFFER_SIZE;              ///< 缓冲区大小，单位字节
  size_t               _msg_item_size  = MSG_SIZE;                 ///< 消息队列中每个项目的大小
  bool                 _transmit_enable;                           ///< 是否启用发送
  uint32_t             _last_received_length = 0;                  ///< 最后一次接收的数据长度
  int                  _instance_id;                               ///< 实例ID，用于生成唯一资源名称
  char                 msgq_name[32];                              ///< 实例消息队列的名字，用于调试时看到名字

  // 静态成员：实例注册表，用于通过UART句柄查找对应的bsp_usart实例
  static constexpr size_t MAX_INSTANCES = 10;        ///< 最大支持的实例数量
  static BspUart         *_instances[MAX_INSTANCES]; ///< 静态实例指针数组
  static size_t           _instance_count;           ///< 当前已注册的实例数量


public:
  /**
   * @brief 构造函数
   *
   * @note 初始化串口驱动对象，配置必要的FreeRTOS对象
   *
   * @param huart UART句柄指针
   * @param rx_mode 接收模式
   * @param transmit_signal 是否启用发送功能
   * @param instance_id 实例ID，用于生成唯一资源名称
   */
  BspUart(UART_HandleTypeDef *huart, ReceiveMode rx_mode, bool transmit_signal, int instance_id = 0);

  /**
   * @brief 初始化函数 初始化串口驱动对象，配置必要的FreeRTOS对象
   *
   * @return true 初始化成功
   * @return false 初始化失败
   */
  bool init();

  // 析构函数 释放所有分配的资源
  ~BspUart();

  /**
   * @brief 发送数据 将数据放入发送缓冲区，并启动DMA传输。
   *
   * @param data 要发送的数据指针
   * @param size 数据大小
   * @param timeout 超时时间（ticks / ms）
   *
   * @return int 返回发送的数据字节数，负值表示错误
   */
  int send(const uint8_t *data, size_t size, uint32_t timeout = portMAX_DELAY);

  /**
   * @brief 接收数据 根据接收模式从相应的缓冲区读取数据
   *
   * @param buffer 接收数据的缓冲区
   * @param size 请求读取的数据大小
   * @param timeout 超时时间（ticks）
   * @return int 实际读取的数据字节数，-1表示超时或无数据
   */
  int receive(uint8_t *buffer, size_t size, uint32_t timeout = portMAX_DELAY);

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
   * @brief DMA传输完成回调函数 由HAL库调用，处理DMA传输完成事件
   *
   * @param huart UART句柄
   */
  void dmaTransferCompleteCallback(UART_HandleTypeDef *huart);

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
   * @param Size 接收到的数据长度
   * @param pxHigherPriorityTaskWoken 需初始化为pdFALSE，若唤醒高优先级任务则置为pdTRUE
   */
  void handle_idle_interrupt_internal(UART_HandleTypeDef *huart, uint16_t Size, BaseType_t *pxHigherPriorityTaskWoken);

  /**
   * @brief TX发送完成处理函数（ISR上下文）
   * @param pxHigherPriorityTaskWoken 需初始化为pdFALSE，若唤醒高优先级任务则置为pdTRUE
   */
  void handle_tx_complete_from_isr(BaseType_t *pxHigherPriorityTaskWoken);

  /**
   * @brief 通过UART句柄查找对应的bsp_usart实例
   *
   * @note 静态成员函数，用于在中断回调中通过UART句柄找到对应的类实例
   *
   * @param huart UART句柄指针
   * @return BspUart* 找到的实例指针，未找到返回nullptr
   */
  static BspUart *get_instance_by_handle(UART_HandleTypeDef *huart);

  /**
   * @brief 注册实例到静态注册表中
   *
   * @note 在构造函数中自动调用，将当前实例添加到注册表
   *
   * @return true 注册成功
   * @return false 注册失败（已满或其他错误）
   */
  bool register_instance();

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

  // 获取UART句柄指针（供静态函数使用）
  UART_HandleTypeDef *get_huart() const
  {
    return _huart;
  }
};


#endif // __BSP_UART_HPP__
