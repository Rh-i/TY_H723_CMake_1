#include "bsp_uart.hpp"
#include "bsp_cfg.hpp" // 中断回调中直接引用 bsp_usartX / bsp_uartX 全局实例
#include "FreeRTOS.h" // IWYU pragma: keep
#include "string.h"
#include <stdarg.h>
#include <stdio.h>

/* ==================== 模板实例化 ==================== */

/**
 * @brief 模板实例化实现
 * @param 第一个数字为缓冲区大小（uint8_t）
 * @param 第二个数字为消息队列的长度（uint8_t）
 *
 */
template class BspUart<128, 8>;


extern "C"
{
  /**
   * @brief IDLE串口回调函数
   * @note 直接 if-else 判断 UART 句柄并调用对应全局实例
   */
  void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
  {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (huart == &huart1)
    {
      bsp_uart1.handle_idle_interrupt_internal(huart, Size, &xHigherPriorityTaskWoken);
    }
    else if (huart == &huart3)
    {
      bsp_uart3.handle_idle_interrupt_internal(huart, Size, &xHigherPriorityTaskWoken);
    }
    else if (huart == &huart4)
    {
      bsp_uart4.handle_idle_interrupt_internal(huart, Size, &xHigherPriorityTaskWoken);
    }
    else if (huart == &huart5)
    {
      bsp_uart5.handle_idle_interrupt_internal(huart, Size, &xHigherPriorityTaskWoken);
    }
    else if (huart == &huart7)
    {
      bsp_uart7.handle_idle_interrupt_internal(huart, Size, &xHigherPriorityTaskWoken);
    }
    else if (huart == &huart8)
    {
      bsp_uart8.handle_idle_interrupt_internal(huart, Size, &xHigherPriorityTaskWoken);
    }
    else if (huart == &huart9)
    {
      bsp_uart9.handle_idle_interrupt_internal(huart, Size, &xHigherPriorityTaskWoken);
    }
    else if (huart == &huart10)
    {
      bsp_uart10.handle_idle_interrupt_internal(huart, Size, &xHigherPriorityTaskWoken);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }

  /**
   * @brief UART TX Complete 回调函数
   * @note 发送完成时触发，用于继续发送剩余数据
   */
  void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
  {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // 注：UART5 无发送功能（未配 TX DMA），不会触发本回调，故无 huart5 分支
    if (huart == &huart1)
    {
      bsp_uart1.handle_tx_complete_from_isr(&xHigherPriorityTaskWoken);
    }
    else if (huart == &huart3)
    {
      bsp_uart3.handle_tx_complete_from_isr(&xHigherPriorityTaskWoken);
    }
    else if (huart == &huart4)
    {
      bsp_uart4.handle_tx_complete_from_isr(&xHigherPriorityTaskWoken);
    }
    else if (huart == &huart7)
    {
      bsp_uart7.handle_tx_complete_from_isr(&xHigherPriorityTaskWoken);
    }
    else if (huart == &huart8)
    {
      bsp_uart8.handle_tx_complete_from_isr(&xHigherPriorityTaskWoken);
    }
    else if (huart == &huart9)
    {
      bsp_uart9.handle_tx_complete_from_isr(&xHigherPriorityTaskWoken);
    }
    else if (huart == &huart10)
    {
      bsp_uart10.handle_tx_complete_from_isr(&xHigherPriorityTaskWoken);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

/**
 * @brief 以下是 BspUart<BUFFER_SIZE, MSG_SIZE>::BspUart 这个类的函数定义，看到这就可以不看了
 *
 * 串口驱动组件实现：（只使用了IDLE中断，其他未使用）
 * 设计要点：使用FreeRTOS的流缓冲区，实现单环形缓冲区的处理
 * 不用阻塞：DMA收发，规范中断回调函数（DMA都开普通模式 FIFO在只取最新模式会有问题）
 * 线程安全：在多任务环境下的互斥访问控制，防止多个任务同时操作串口导致的问题
 * 错误处理：如何处理DMA传输错误、缓冲区溢出、硬件故障等情况
 * 接口设计：对外接口的易用性和一致性
 * 该类封装了基于STM32 HAL库、FreeRTOS的串口驱动功能，
 * 只支持DMA传输、流缓冲区管理、互斥锁保护和错误处理等功能（错误处理那些回调有写，但是没放到回调函数中）
 *
 * @note 经过测试，无任何测试问题。
 * @note 单缓冲区在自己串口发送的时候串口助手显示有问题，但是逻辑是对的。内容可以正常的存入缓冲区然后等待一个一个的读取。
 * @note 双缓冲区逻辑无误，但是应用场景需要经过自己测试，理清他的逻辑，有点反直觉
 *
 * @param BUFFER_SIZE 存储的缓冲区大小（单双缓冲区）
 * @param MSG_SIZE 消息队列的大小（消息邮箱）
 *
 * @param cfg 串口配置（huart/接收模式/发送使能/实例ID，可匿名按序传入）
 */
template <size_t BUFFER_SIZE, size_t MSG_SIZE>
BspUart<BUFFER_SIZE, MSG_SIZE>::BspUart(const Config &cfg)

  : _huart(cfg.huart),
    _receive_mode(cfg.rx_mode),
    _transmit_enable(cfg.transmit_enable),
    _instance_id(cfg.instance_id)
{
  // 构造函数只做赋值；实例注册（运行时逻辑）推迟到 init()
}

template <size_t BUFFER_SIZE, size_t MSG_SIZE>
Status BspUart<BUFFER_SIZE, MSG_SIZE>::init()
{
  // 根据接收模式创建消息队列 - 只有LATEST_ONLY模式才创建
  if (_receive_mode == ReceiveMode::LATEST_ONLY)
  {
    snprintf(_msgq_name, sizeof(_msgq_name), "USART%d_MsgQ", _instance_id);

    // 对于LATEST_ONLY模式，消息队列长度为1，只保留最新数据
    _msg_queue_id = xQueueCreate(1, _msg_item_size);
    if (_msg_queue_id == nullptr)
    {
      cleanup_resources(); // 清理已创建的资源
      return Status::IO_ERROR;        // 消息队列创建失败
    }
  }
  else
  {
    _msg_queue_id = nullptr; // 非LATEST_ONLY模式不需要消息队列
  }

  // 初始化接收流缓冲区数组
  for (int i = 0; i < 2; i++)
  {
    _rx_stream_buffers[i] = nullptr;
  }

  // 根据接收模式创建相应的缓冲区
  switch (_receive_mode)
  {
    case ReceiveMode::SINGLE_BUFFER:
      _rx_stream_buffers[0] = xStreamBufferCreate(BUFFER_SIZE, 1);
      if (_rx_stream_buffers[0] == nullptr)
      {
        cleanup_resources(); // 清理已创建的资源
        return Status::IO_ERROR;        // 流缓冲区创建失败
      }
      break;
    case ReceiveMode::DOUBLE_BUFFER:
      // 创建两个流缓冲区用于双缓冲机制
      _rx_stream_buffers[0] = xStreamBufferCreate(BUFFER_SIZE, 1);
      if (_rx_stream_buffers[0] == nullptr)
      {
        cleanup_resources(); // 清理已创建的资源
        return Status::IO_ERROR;        // 流缓冲区创建失败
      }

      _rx_stream_buffers[1] = xStreamBufferCreate(BUFFER_SIZE, 1);
      if (_rx_stream_buffers[1] == nullptr)
      {
        cleanup_resources(); // 清理已创建的资源
        return Status::IO_ERROR;        // 流缓冲区创建失败
      }
      break;
    case ReceiveMode::LATEST_ONLY:
    default: // LATEST_ONLY
      // 不需要流缓冲区
      break;
  }

  if (_transmit_enable)
  {
    // 创建发送流缓冲区
    _tx_stream_buffer = xStreamBufferCreate(BUFFER_SIZE, 1);
    if (_tx_stream_buffer == nullptr)
    {
      cleanup_resources(); // 清理已创建的资源
      return Status::IO_ERROR;        // 发送流缓冲区创建失败
    }
  }
  else
  {
    _tx_stream_buffer = nullptr;
  }

  // 启用IDLE中断
  __HAL_UART_CLEAR_FLAG(_huart, UART_FLAG_IDLE);
  SET_BIT(_huart->Instance->CR1, USART_CR1_IDLEIE);

  // 启动接收
  start_reception();

  return Status::OK; // 初始化成功
}

// 析构函数实现
template <size_t BUFFER_SIZE, size_t MSG_SIZE>
BspUart<BUFFER_SIZE, MSG_SIZE>::~BspUart()
{
  stop_reception();

  cleanup_resources();
}

template <size_t BUFFER_SIZE, size_t MSG_SIZE>
void BspUart<BUFFER_SIZE, MSG_SIZE>::cleanup_resources()
{
  // 只有LATEST_ONLY模式才创建了消息队列，需要删除
  if (_msg_queue_id != nullptr)
  {
    vQueueDelete(_msg_queue_id);
    _msg_queue_id = nullptr;
  }

  // 统一释放接收流缓冲区
  for (int i = 0; i < 2; i++)
  {
    if (_rx_stream_buffers[i] != nullptr)
    {
      vStreamBufferDelete(_rx_stream_buffers[i]);
      _rx_stream_buffers[i] = nullptr;
    }
  }

  if (_tx_stream_buffer != nullptr)
  {
    vStreamBufferDelete(_tx_stream_buffer);
    _tx_stream_buffer = nullptr;
  }
}

// 发送数据实现
template <size_t BUFFER_SIZE, size_t MSG_SIZE>
Status BspUart<BUFFER_SIZE, MSG_SIZE>::send(const uint8_t *data, size_t size, size_t *written, uint32_t timeout)
{
  if (data == nullptr || size == 0)
  {
    return Status::BAD_ARG; // 参数非法
  }

  // 单次发送不能超过流缓冲区容量，否则 xStreamBufferSend 会永久阻塞（死锁）
  if (size > BUFFER_SIZE)
  {
    return Status::BAD_ARG; // 数据超长，调用方应分包发送
  }

  if (!_transmit_enable)
  {
    return Status::IO_ERROR; // 没使能发送
  }

  if (_tx_stream_buffer == nullptr)
  {
    return Status::IO_ERROR; // 发送缓冲区未初始化
  }

  // 将数据写入发送流缓冲区
  size_t bytes_written = xStreamBufferSend(_tx_stream_buffer, data, size, timeout);

  // 如果发送缓冲区中有数据，启动发送
  if (bytes_written > 0)
  {
    start_transmission();
  }

  if (written != nullptr)
  {
    *written = bytes_written;
  }

  return (bytes_written == size) ? Status::OK : Status::TIMEOUT;
}

// printf 格式化发送实现
template <size_t BUFFER_SIZE, size_t MSG_SIZE>
Status BspUart<BUFFER_SIZE, MSG_SIZE>::printf(const char *fmt, ...)
{
  if (fmt == nullptr)
  {
    return Status::BAD_ARG; // 参数非法
  }

  va_list args;
  va_start(args, fmt);
  int len = vsnprintf(_printf_buffer, sizeof(_printf_buffer), fmt, args);
  va_end(args);

  if (len <= 0)
  {
    return Status::BAD_ARG; // 格式化失败或空输出
  }

  // vsnprintf 返回的是期望写入的完整长度，实际写入可能被截断
  if (static_cast<size_t>(len) >= sizeof(_printf_buffer))
  {
    len = static_cast<int>(sizeof(_printf_buffer)) - 1;
  }

  return send(reinterpret_cast<const uint8_t *>(_printf_buffer), static_cast<size_t>(len));
}

// 接收数据实现
template <size_t BUFFER_SIZE, size_t MSG_SIZE>
Status BspUart<BUFFER_SIZE, MSG_SIZE>::receive(uint8_t *buffer, size_t size, size_t *received, uint32_t timeout)
{
  if (buffer == nullptr || size == 0)
  {
    return Status::BAD_ARG; // 参数非法
  }

  // 根据接收模式进行不同的处理
  switch (_receive_mode)
  {
    case ReceiveMode::LATEST_ONLY:
    {
      // 在LATEST_ONLY模式下，从消息队列获取最新数据

      // 获取最新消息
      BaseType_t status = xQueueReceive(_msg_queue_id, buffer, pdMS_TO_TICKS(timeout));

      if (status == pdTRUE)
      {
        size_t actual = (size < MSG_SIZE) ? size : MSG_SIZE; // 实际读取的字节数
        if (received != nullptr)
        {
          *received = actual;
        }
        return Status::OK;
      }
      return Status::TIMEOUT; // 没有数据或超时
    }

    case ReceiveMode::SINGLE_BUFFER:
    {
      // 单缓冲处理
      if (_rx_stream_buffers[0] == nullptr)
      {
        return Status::IO_ERROR; // 流缓冲区未创建
      }

      size_t bytes_read = xStreamBufferReceive(_rx_stream_buffers[0], buffer, size, pdMS_TO_TICKS(timeout));
      if (received != nullptr)
      {
        *received = bytes_read;
      }
      return (bytes_read > 0) ? Status::OK : Status::TIMEOUT;
    }

    case ReceiveMode::DOUBLE_BUFFER:
    {
      // 双缓冲处理
      StreamBufferHandle_t target_buffer = _current_buffer ? _rx_stream_buffers[1] : _rx_stream_buffers[0];
      if (target_buffer == nullptr)
      {
        return Status::IO_ERROR; // 流缓冲区未创建
      }

      size_t bytes_read = xStreamBufferReceive(target_buffer, buffer, size, pdMS_TO_TICKS(timeout));
      if (received != nullptr)
      {
        *received = bytes_read;
      }
      return (bytes_read > 0) ? Status::OK : Status::TIMEOUT;
    }

    default:
      return Status::BAD_ARG; // 未定义的接收模式
  }
}

// 获取发送缓冲区剩余空间实现
template <size_t BUFFER_SIZE, size_t MSG_SIZE>
size_t BspUart<BUFFER_SIZE, MSG_SIZE>::get_tx_free_space()
{
  if (_tx_stream_buffer != nullptr)
  {
    return xStreamBufferSpacesAvailable(_tx_stream_buffer);
  }
  return 0;
}

// 获取接收缓冲区可用数据量实现
template <size_t BUFFER_SIZE, size_t MSG_SIZE>
size_t BspUart<BUFFER_SIZE, MSG_SIZE>::get_rx_available_data()
{
  switch (_receive_mode)
  {
    case ReceiveMode::LATEST_ONLY:
    {
      // 对于LATEST_ONLY模式，检查消息队列是否有数据
      if (_msg_queue_id != nullptr)
      {
        uint32_t count = uxQueueMessagesWaiting(_msg_queue_id);
        return count * _msg_item_size;
      }
      return 0;
    }

    case ReceiveMode::SINGLE_BUFFER:
    {
      if (_rx_stream_buffers[0] != nullptr)
      {
        return xStreamBufferBytesAvailable(_rx_stream_buffers[0]);
      }
      return 0;
    }

    case ReceiveMode::DOUBLE_BUFFER:
    {
      size_t total_bytes = 0;
      for (int i = 0; i < 2; i++)
      {
        if (_rx_stream_buffers[i] != nullptr)
        {
          total_bytes += xStreamBufferBytesAvailable(_rx_stream_buffers[i]);
        }
      }
      return total_bytes;
    }

    default:
      return 0; // 未知接收模式
  }
}

// 开始接收数据实现
template <size_t BUFFER_SIZE, size_t MSG_SIZE>
void BspUart<BUFFER_SIZE, MSG_SIZE>::start_reception()
{
  // 启动多字节DMA接收
  HAL_UARTEx_ReceiveToIdle_DMA(_huart, _rx_dma_buffer, BUFFER_SIZE);
  _rx_active = true;
}

// 重启 DMA 接收（任务上下文）—— ISR 中不重启，推迟到任务中

template <size_t BUFFER_SIZE, size_t MSG_SIZE>
void BspUart<BUFFER_SIZE, MSG_SIZE>::restart_rx()
{
  if (_rx_active)
  {
    HAL_UARTEx_ReceiveToIdle_DMA(_huart, _rx_dma_buffer, BUFFER_SIZE);
  }
}

// 停止接收数据实现
template <size_t BUFFER_SIZE, size_t MSG_SIZE>
void BspUart<BUFFER_SIZE, MSG_SIZE>::stop_reception()
{
  _rx_active = false;
  HAL_UART_DMAStop(_huart);
}

// 开始传输数据实现（任务上下文）
template <size_t BUFFER_SIZE, size_t MSG_SIZE>
void BspUart<BUFFER_SIZE, MSG_SIZE>::start_transmission()
{
  if (_tx_stream_buffer == nullptr)
  {
    return; // 发送缓冲区未初始化
  }

  if (!is_transmitting())
  {
    // 从发送缓冲区获取数据准备发送（任务级API）
    size_t bytes_to_send = xStreamBufferReceive(_tx_stream_buffer, _tx_dma_buffer, BUFFER_SIZE, 0);
    if (bytes_to_send > 0)
    {
      HAL_UART_Transmit_DMA(_huart, _tx_dma_buffer, bytes_to_send);
    }
  }
}

// 开始传输数据实现（ISR上下文）
template <size_t BUFFER_SIZE, size_t MSG_SIZE>
void BspUart<BUFFER_SIZE, MSG_SIZE>::start_transmission_from_isr(BaseType_t *pxHigherPriorityTaskWoken)
{
  if (_tx_stream_buffer == nullptr)
  {
    return; // 发送缓冲区未初始化
  }

  if (!is_transmitting())
  {
    // 从发送缓冲区获取数据准备发送（ISR级API）
    size_t bytes_to_send = xStreamBufferReceiveFromISR(_tx_stream_buffer, _tx_dma_buffer, BUFFER_SIZE, pxHigherPriorityTaskWoken);
    if (bytes_to_send > 0)
    {
      HAL_UART_Transmit_DMA(_huart, _tx_dma_buffer, bytes_to_send);
    }
  }
}

// 检查是否正在传输实现
template <size_t BUFFER_SIZE, size_t MSG_SIZE>
bool BspUart<BUFFER_SIZE, MSG_SIZE>::is_transmitting()
{
  return (_huart->gState == HAL_UART_STATE_BUSY_TX);
}

// 发送完成处理函数 (由 TX Complete 中断调用)
template <size_t BUFFER_SIZE, size_t MSG_SIZE>
void BspUart<BUFFER_SIZE, MSG_SIZE>::handle_tx_complete_from_isr(BaseType_t *pxHigherPriorityTaskWoken)
{
  if (_tx_stream_buffer == nullptr || !_transmit_enable)
  {
    return;
  }

  // 检查发送缓冲区是否还有数据，如果有则继续发送
  if (xStreamBufferBytesAvailable(_tx_stream_buffer) > 0)
  {
    start_transmission_from_isr(pxHigherPriorityTaskWoken);
  }
}

// IDLE中断处理函数（ISR上下文）
template <size_t BUFFER_SIZE, size_t MSG_SIZE>
void BspUart<BUFFER_SIZE, MSG_SIZE>::handle_idle_interrupt_from_isr(uint32_t received_length, BaseType_t *pxHigherPriorityTaskWoken)
{
  _last_received_length = received_length;

  switch (_receive_mode)
  {
    case ReceiveMode::LATEST_ONLY:
    {
      // 处理整组数据，而非单个字节
      if (received_length >= _msg_item_size)
      {
        // 将整组数据的最后MSG_SIZE个字节作为最新数据
        uint8_t *latest_data_ptr = &_rx_dma_buffer[received_length - _msg_item_size];

        xQueueSendFromISR(_msg_queue_id, latest_data_ptr, pxHigherPriorityTaskWoken);
      }
      else if (received_length > 0)
      {
        // 如果接收的数据不足MSG_SIZE，将整个数据复制到一个临时缓冲区
        uint8_t temp_latest_data[MSG_SIZE] = {0}; // 初始化为0
        memcpy(temp_latest_data, _rx_dma_buffer, received_length);

        // TODO AI说这里有问题 这里清空队列 但是说ISR中不能用正常的清空 
        xQueueReset(_msg_queue_id);

        xQueueSendFromISR(_msg_queue_id, temp_latest_data, pxHigherPriorityTaskWoken);
      }

      // 重新启动DMA接收
      if (_rx_active)
      {
        HAL_UARTEx_ReceiveToIdle_DMA(_huart, _rx_dma_buffer, BUFFER_SIZE);
      }
    }
    break;

    case ReceiveMode::SINGLE_BUFFER:
    {
      // 对于其他模式，将整个数据包放入流缓冲区
      StreamBufferHandle_t target_buffer = nullptr;

      if (_receive_mode == ReceiveMode::SINGLE_BUFFER)
      {
        target_buffer = _rx_stream_buffers[0];
      }
      else // DOUBLE_BUFFER
      {
        // 修复：先获取当前使用的缓冲区，再切换
        target_buffer = _current_buffer ? _rx_stream_buffers[1] : _rx_stream_buffers[0];
      }

      // 1. 停止当前 DMA 传输，确保状态干净
      HAL_UART_DMAStop(_huart);

      // 2. 将数据发送到流缓冲区
      if (target_buffer != nullptr && received_length > 0)
      {
        xStreamBufferSendFromISR(target_buffer, _rx_dma_buffer, received_length, pxHigherPriorityTaskWoken);
      }

      // 3. 切换到下一个缓冲区（双缓冲模式）
      if (_receive_mode == ReceiveMode::DOUBLE_BUFFER)
      {
        _current_buffer = !_current_buffer;
      }

      // 4. 重新启动接收
      if (_rx_active)
      {
        HAL_UARTEx_ReceiveToIdle_DMA(_huart, _rx_dma_buffer, BUFFER_SIZE);
      }
    }
    break;
    default:
      return; // 未知情况
  }
}

// IDLE中断处理函数 (简化版)（ISR上下文）
template <size_t BUFFER_SIZE, size_t MSG_SIZE>
void BspUart<BUFFER_SIZE, MSG_SIZE>::handle_idle_interrupt_internal(UART_HandleTypeDef *huart, uint16_t size, BaseType_t *pxHigherPriorityTaskWoken)
{
  // 获取DMA剩余计数值，计算已接收的数据长度
  uint32_t dma_counter     = __HAL_DMA_GET_COUNTER(huart->hdmarx);
  uint32_t received_length = BUFFER_SIZE - dma_counter;

  // 优先使用 size 参数（HAL 提供的值更可靠）
  // 只有在 size 为 0 且 DMA 计数器不等于 BufferSize 时才认为是错误
  if (size > 0)
  {
    // 正常接收完成，处理数据
    handle_idle_interrupt_from_isr(size, pxHigherPriorityTaskWoken);
  }
  else if (received_length == 0 && dma_counter == BUFFER_SIZE)
  {
    // 没有收到任何数据，可能是虚假中断，忽略
  }
  else
  {
    // 其他情况视为错误
    dma_error_callback(huart);
  }
}

// DMA错误回调函数实现
template <size_t BUFFER_SIZE, size_t MSG_SIZE>
void BspUart<BUFFER_SIZE, MSG_SIZE>::dma_error_callback(UART_HandleTypeDef *huart)
{
  // 记录错误状态并尝试恢复
  handle_dma_error();
}

// 处理DMA错误实现
template <size_t BUFFER_SIZE, size_t MSG_SIZE>
void BspUart<BUFFER_SIZE, MSG_SIZE>::handle_dma_error()
{
  // 停止当前传输
  HAL_UART_DMAStop(_huart);

  // 尝试重启接收
  if (_rx_active)
  {
    start_reception();
  }
}
