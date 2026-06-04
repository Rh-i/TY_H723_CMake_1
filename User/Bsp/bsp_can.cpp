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
      else if(hfdcan == &hfdcan3)
      {
        bsp_can3.process_fifo0_isr();
      }
    }
  }

  /**
   * @brief FDCAN发送完成中断回调
   */
  void HAL_FDCAN_TxBufferCompleteCallback(FDCAN_HandleTypeDef *hfdcan, uint32_t BufferIndexes)
  {
    if (hfdcan == &hfdcan1)
    {
      bsp_can1.trigger_tx();
    }
    else if (hfdcan == &hfdcan2)
    {
      bsp_can2.trigger_tx();
    }
    else if (hfdcan == &hfdcan3)
    {
      bsp_can3.trigger_tx();
    }
  }
}


/* ==================== 类函数实现 ==================== */

BspCan::BspCan(FDCAN_HandleTypeDef *hfdcan, const char *name, CanMode mode)

  : _rx_message_buffer(nullptr),
    _tx_message_buffer(nullptr),
    _hfdcan(hfdcan),
    _name(name),
    _work_mode(mode)
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

bool BspCan::init()
{
  // 创建接收消息缓冲区
  _rx_message_buffer = xMessageBufferCreate((sizeof(CanRxMsg_t) + 4) * 8);
  if (_rx_message_buffer == nullptr)
    return false;

  // 创建发送消息缓冲区
  _tx_message_buffer = xMessageBufferCreate((sizeof(CanTxMsg_t) + 4) * 8);
  if (_tx_message_buffer == nullptr)
  {
    vMessageBufferDelete(_rx_message_buffer);
    return false;
  }

  // 配置过滤器
  if (config_filter() != true)
    return false;

  // 启动硬件
  if (start_hardware() != true)
    return false;

  // 开启接收和发送完成中断
  if (start_reception() != true)
    return false;

  return true;
}

bool BspCan::send(uint32_t stdId, uint8_t *pData)
{
  if (pData == nullptr)
    return false;
  if (_tx_message_buffer == nullptr)
    return false;

  CanTxMsg_t txMsg;
  txMsg.std_id = stdId;
  memcpy(txMsg.data, pData, 8);

  // 放入发送缓冲区（不阻塞）
  size_t sent = xMessageBufferSend(_tx_message_buffer, &txMsg, sizeof(CanTxMsg_t), 0);
  if (sent != sizeof(CanTxMsg_t))
  {
    trigger_tx();
    sent = xMessageBufferSend(_tx_message_buffer, &txMsg, sizeof(CanTxMsg_t), 0);
    if (sent != sizeof(CanTxMsg_t))
    {
      return false;
    }
  }

  // 之前是这样写的，但是返回的sent有问题。但是如果我不检查sent，直接发，数据是没问题的
  // 如果发送FIFO有空闲，触发一次发送
  trigger_tx();

  return true;
}

bool BspCan::receive(CanRxMsg_t *msg, uint32_t timeout)
{
  if (msg == nullptr)
    return false;
  if (_rx_message_buffer == nullptr)
    return false;

  size_t received = xMessageBufferReceive(_rx_message_buffer, msg, sizeof(CanRxMsg_t), timeout);
  return (received > 0) ? true : false;
}

bool BspCan::config_filter()
{
  FDCAN_FilterTypeDef sFilterConfig;
  sFilterConfig.IdType       = FDCAN_STANDARD_ID;
  sFilterConfig.FilterIndex  = 0;
  sFilterConfig.FilterType   = FDCAN_FILTER_RANGE;
  sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  sFilterConfig.FilterID1    = 0x000;
  sFilterConfig.FilterID2    = 0x7FF;

  if (HAL_FDCAN_ConfigFilter(_hfdcan, &sFilterConfig) != HAL_OK)
    return false;

  if (HAL_FDCAN_ConfigGlobalFilter(_hfdcan,
                                   FDCAN_ACCEPT_IN_RX_FIFO0,
                                   FDCAN_ACCEPT_IN_RX_FIFO0,
                                   FDCAN_FILTER_REMOTE,
                                   FDCAN_FILTER_REMOTE)
      != HAL_OK)
  {
    return false;
  }
  return true;
}

bool BspCan::start_hardware()
{
  if (HAL_FDCAN_Start(_hfdcan) != HAL_OK)
    return false;
  return true;
}

bool BspCan::start_reception()
{
  // 开启FIFO0新消息中断
  if (HAL_FDCAN_ActivateNotification(_hfdcan, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK)
    return false;

  // 开启发送完成中断
  if (HAL_FDCAN_ActivateNotification(_hfdcan, FDCAN_IT_TX_COMPLETE, 0) != HAL_OK)
    return false;

  return true;
}

/**
 * @brief 触发一次发送（从发送缓冲区取数据发送）
 */
void BspCan::trigger_tx()
{
  if (_tx_message_buffer == nullptr)
    return;

  // 如果发送FIFO空闲
  if (HAL_FDCAN_GetTxFifoFreeLevel(_hfdcan) > 0)
  {
    CanTxMsg_t txMsg;
    // 非阻塞从发送缓冲区取数据
    size_t len = xMessageBufferReceive(_tx_message_buffer, &txMsg, sizeof(CanTxMsg_t), 0);
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

  CanRxMsg_t rxMsg;
  if (HAL_FDCAN_GetRxMessage(_hfdcan, FDCAN_RX_FIFO0, &rxMsg.header, rxMsg.data) == HAL_OK)
  {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xMessageBufferSendFromISR(_rx_message_buffer,
                              &rxMsg,
                              sizeof(CanRxMsg_t),
                              &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}
