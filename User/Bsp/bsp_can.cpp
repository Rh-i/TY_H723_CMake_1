#include "bsp_can.hpp"
#include "bsp_cfg.hpp"
#include "fdcan.h"
#include <string.h>


/* ==================== 中断回调函数 ==================== */

extern "C"
{
  /**
   * @brief FDCAN接收FIFO0中断回调
   */
  void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
  {
    if (RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE)
    {
      if (hfdcan == &hfdcan1)
      {
        bsp_can1.process_fifo0_isr();
      }
      else if (hfdcan == &hfdcan2)
      {
        bsp_can2.process_fifo0_isr();
      }
      else if (hfdcan == &hfdcan3)
      {
        bsp_can3.process_fifo0_isr();
      }
    }
  }

  /**
   * @brief FDCAN发送 FIFO 变空回调，用于继续排空软件发送缓冲区
   */
  void HAL_FDCAN_TxFifoEmptyCallback(FDCAN_HandleTypeDef *hfdcan)
  {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (hfdcan == &hfdcan1)
    {
      bsp_can1.trigger_tx_from_isr(&xHigherPriorityTaskWoken);
    }
    else if (hfdcan == &hfdcan2)
    {
      bsp_can2.trigger_tx_from_isr(&xHigherPriorityTaskWoken);
    }
    else if (hfdcan == &hfdcan3)
    {
      bsp_can3.trigger_tx_from_isr(&xHigherPriorityTaskWoken);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}


/* ==================== 类函数实现 ==================== */

BspCan::BspCan(const Config &cfg)

  : _rx_message_buffer(nullptr),
    _tx_message_buffer(nullptr),
    _hfdcan(cfg.hfdcan),
    _name(cfg.name),
    _work_mode(cfg.mode)
{
}

BspCan::~BspCan()
{
  if (_rx_message_buffer != nullptr)
  {
    vMessageBufferDelete(_rx_message_buffer);
    _rx_message_buffer = nullptr;
  }
  if (_tx_message_buffer != nullptr)
  {
    vMessageBufferDelete(_tx_message_buffer);
    _tx_message_buffer = nullptr;
  }
  if (_hfdcan != nullptr)
  {
    HAL_FDCAN_Stop(_hfdcan);
  }
}

Status BspCan::init()
{
  // 创建接收消息缓冲区
  _rx_message_buffer = xMessageBufferCreate((sizeof(CanRxMsg) + 4) * 8);
  if (_rx_message_buffer == nullptr)
    return Status::IO_ERROR;

  // 创建发送消息缓冲区
  _tx_message_buffer = xMessageBufferCreate((sizeof(CanTxMsg) + 4) * 8);
  if (_tx_message_buffer == nullptr)
  {
    vMessageBufferDelete(_rx_message_buffer);
    return Status::IO_ERROR;
  }

  // 配置过滤器
  if (config_filter() != Status::OK)
    return Status::IO_ERROR;

  // 启动硬件
  if (start_hardware() != Status::OK)
    return Status::IO_ERROR;

  // 开启接收和发送 FIFO 变空中断。TX_COMPLETE 需要指定 TXBTIE buffer
  // 位；FIFO 模式下传入 BufferIndexes=0 不会产生发送完成回调。
  if (start_reception() != Status::OK)
    return Status::IO_ERROR;

  return Status::OK;
}

Status BspCan::send(uint32_t std_id, uint8_t *data)
{
  if (data == nullptr)
    return Status::BAD_ARG;
  if (_tx_message_buffer == nullptr)
    return Status::NOT_INIT;

  CanTxMsg txMsg;
  txMsg.std_id = std_id;
  memcpy(txMsg.data, data, 8);

  // 放入发送缓冲区（不阻塞）
  size_t sent = xMessageBufferSend(_tx_message_buffer, &txMsg, sizeof(CanTxMsg), 0);

  // 如果发送FIFO有空闲，触发一次发送
  trigger_tx();

  return (sent == sizeof(CanTxMsg)) ? Status::OK : Status::FULL;
}

Status BspCan::receive(CanRxMsg *msg, uint32_t timeout)
{
  if (msg == nullptr)
    return Status::BAD_ARG;
  if (_rx_message_buffer == nullptr)
    return Status::NOT_INIT;

  size_t received = xMessageBufferReceive(_rx_message_buffer, msg, sizeof(CanRxMsg), timeout);
  return (received > 0) ? Status::OK : Status::TIMEOUT;
}

Status BspCan::config_filter()
{
  FDCAN_FilterTypeDef sFilterConfig;
  sFilterConfig.IdType       = FDCAN_STANDARD_ID;
  sFilterConfig.FilterIndex  = 0;
  sFilterConfig.FilterType   = FDCAN_FILTER_RANGE;
  sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  sFilterConfig.FilterID1    = 0x000;
  sFilterConfig.FilterID2    = 0x7FF;

  if (HAL_FDCAN_ConfigFilter(_hfdcan, &sFilterConfig) != HAL_OK)
    return Status::IO_ERROR;

  if (HAL_FDCAN_ConfigGlobalFilter(_hfdcan,
                                   FDCAN_ACCEPT_IN_RX_FIFO0,
                                   FDCAN_ACCEPT_IN_RX_FIFO0,
                                   FDCAN_FILTER_REMOTE,
                                   FDCAN_FILTER_REMOTE)
      != HAL_OK)
  {
    return Status::IO_ERROR;
  }
  return Status::OK;
}

Status BspCan::start_hardware()
{
  if (HAL_FDCAN_Start(_hfdcan) != HAL_OK)
    return Status::IO_ERROR;
  return Status::OK;
}

Status BspCan::start_reception()
{
  // 开启FIFO0新消息中断
  if (HAL_FDCAN_ActivateNotification(_hfdcan, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK)
    return Status::IO_ERROR;

  // 软件发送缓冲区积压时，在硬件 FIFO 发送完后继续排空。
  if (HAL_FDCAN_ActivateNotification(_hfdcan, FDCAN_IT_TX_FIFO_EMPTY, 0) != HAL_OK)
    return Status::IO_ERROR;

  return Status::OK;
}

/**
 * @brief 触发一次发送（任务上下文调用）
 */
void BspCan::trigger_tx()
{
  if (_tx_message_buffer == nullptr)
    return;

  if (HAL_FDCAN_GetTxFifoFreeLevel(_hfdcan) > 0)
  {
    CanTxMsg txMsg;
    size_t   len = xMessageBufferReceive(_tx_message_buffer, &txMsg, sizeof(CanTxMsg), 0);
    if (len > 0)
    {
      FDCAN_TxHeaderTypeDef txHeader;
      txHeader.Identifier          = txMsg.std_id;
      txHeader.IdType              = FDCAN_STANDARD_ID;
      txHeader.TxFrameType         = FDCAN_DATA_FRAME;
      txHeader.DataLength          = FDCAN_DLC_BYTES_8;
      txHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
      txHeader.BitRateSwitch       = FDCAN_BRS_OFF;
      txHeader.FDFormat            = FDCAN_CLASSIC_CAN;
      txHeader.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
      txHeader.MessageMarker       = 0;

      HAL_FDCAN_AddMessageToTxFifoQ(_hfdcan, &txHeader, txMsg.data);
    }
  }
}

/**
 * @brief 触发一次发送（ISR上下文调用）
 * @param pxHigherPriorityTaskWoken 需初始化为pdFALSE，若唤醒高优先级任务则置为pdTRUE
 */
void BspCan::trigger_tx_from_isr(BaseType_t *pxHigherPriorityTaskWoken)
{
  if (_tx_message_buffer == nullptr)
    return;

  if (HAL_FDCAN_GetTxFifoFreeLevel(_hfdcan) > 0)
  {
    CanTxMsg txMsg;
    size_t   len = xMessageBufferReceiveFromISR(_tx_message_buffer, &txMsg, sizeof(CanTxMsg), pxHigherPriorityTaskWoken);
    if (len > 0)
    {
      FDCAN_TxHeaderTypeDef txHeader;
      txHeader.Identifier          = txMsg.std_id;
      txHeader.IdType              = FDCAN_STANDARD_ID;
      txHeader.TxFrameType         = FDCAN_DATA_FRAME;
      txHeader.DataLength          = FDCAN_DLC_BYTES_8;
      txHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
      txHeader.BitRateSwitch       = FDCAN_BRS_OFF;
      txHeader.FDFormat            = FDCAN_CLASSIC_CAN;
      txHeader.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
      txHeader.MessageMarker       = 0;

      HAL_FDCAN_AddMessageToTxFifoQ(_hfdcan, &txHeader, txMsg.data);
    }
  }
}

/**
 * @brief FIFO0 中断处理（从硬件FIFO读取并放入Message Buffer）
 */
void BspCan::process_fifo0_isr()
{
  if (_hfdcan == nullptr || _rx_message_buffer == nullptr)
    return;

  CanRxMsg rxMsg;
  if (HAL_FDCAN_GetRxMessage(_hfdcan, FDCAN_RX_FIFO0, &rxMsg.header, rxMsg.data) == HAL_OK)
  {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xMessageBufferSendFromISR(_rx_message_buffer,
                              &rxMsg,
                              sizeof(CanRxMsg),
                              &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}
