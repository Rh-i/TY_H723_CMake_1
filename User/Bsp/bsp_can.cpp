#include "bsp_can.hpp"
#include "cmsis_os2.h"
#include "string.h"
#include "task.h"
#include <stdio.h>


/* USER CODE BEGIN */

/* ==================== 声明CAN句柄 ==================== */
extern FDCAN_HandleTypeDef hfdcan1;


/* ==================== 全局实例化 ==================== */

/**
 * @brief 全局实例化
 * @param CAN句柄
 * @param 实例名称（用于资源命名）
 * @param CAN工作模式（默认正常模式）
 */
bsp_can bsp_can1(&hfdcan1, "CAN1");

/* USER CODE END */


/* ==================== 模板静态成员初始化 ==================== */
bsp_can *bsp_can::_instances[bsp_can::MAX_INSTANCES] = {nullptr};
size_t   bsp_can::_instance_count                    = 0;


/* ==================== 中断回调函数 ==================== */

extern "C"
{
  /**
   * @brief FDCAN接收FIFO0中断回调
   *
   * @note 从FIFO0读取数据并放入消息队列
   * @note 使用消息队列接收（中断回调+队列方式）
   */
  void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
  {
    // 通过CAN句柄查找对应的bsp_can实例
    bsp_can *instance = bsp_can::get_instance_by_handle(hfdcan);

    if (instance != nullptr)
    {
      // 找到对应实例，处理接收
      if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != 0)
      {
        can_rx_msg_t rxMsg;

        // 从FIFO0读取数据
        if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &rxMsg.header, rxMsg.data) == HAL_OK)
        {
          // 将数据放入消息队列（中断中timeout必须为0）
          if (instance->_rx_queue_handle != nullptr)
          {
            osMessageQueuePut(instance->_rx_queue_handle, &rxMsg, 0, 0);
          }
        }
      }
    }
    // 可在此扩展多CAN实例支持
  }

  /**
   * @brief FDCAN发送完成中断回调
   *
   * @note 暂不支持CANFD
   */
  void HAL_FDCAN_TxBufferCompleteCallback(FDCAN_HandleTypeDef *hfdcan, uint32_t BufferIndexes)
  {
    // 发送完成处理，可在此添加用户逻辑
    // 例如：更新发送状态、触发后续处理等
    (void)hfdcan;
    (void)BufferIndexes;
  }
}


/* ==================== 类函数实现 ==================== */

/**
 * @brief 构造函数实现
 *
 * @note 初始化成员变量，注册实例到静态表
 */
bsp_can::bsp_can(FDCAN_HandleTypeDef *hfdcan, const char *name, can_mode mode)

  : _hfdcan(hfdcan),
    _name(name),
    _rx_queue_handle(nullptr),
    _tx_mutex_handle(nullptr),
    _work_mode(mode),
    _is_initialized(false)
{
  // 注册当前实例到静态注册表中
  register_instance();
}


/**
 * @brief 析构函数实现
 *
 * @note 释放所有分配的资源
 */
bsp_can::~bsp_can()
{
  // 删除消息队列
  if (_rx_queue_handle != nullptr)
  {
    osMessageQueueDelete(_rx_queue_handle);
    _rx_queue_handle = nullptr;
  }

  // 删除互斥锁
  if (_tx_mutex_handle != nullptr)
  {
    osMutexDelete(_tx_mutex_handle);
    _tx_mutex_handle = nullptr;
  }

  // 停止FDCAN外设
  if (_hfdcan != nullptr)
  {
    HAL_FDCAN_Stop(_hfdcan);
  }

  _is_initialized = false;
}


/**
 * @brief 初始化函数实现
 *
 * @note 必须在FreeRTOS内核初始化完成后调用
 *       依次创建资源、配置硬件、启动接收
 */
bool bsp_can::init()
{
  // 1. 创建FreeRTOS资源
  if (!create_rtos_resources())
  {
    cleanup_resources();
    return false;
  }

  // 2. 配置CAN过滤器
  if (config_filter() != HAL_OK)
  {
    cleanup_resources();
    return false;
  }

  // 3. 启动CAN硬件
  if (start_hardware() != HAL_OK)
  {
    cleanup_resources();
    return false;
  }

  // 4. 启动接收（开启中断）
  if (start_reception() != HAL_OK)
  {
    cleanup_resources();
    return false;
  }

  _is_initialized = true;
  return true;
}


/**
 * @brief 创建FreeRTOS资源
 */
bool bsp_can::create_rtos_resources()
{
  // 创建消息队列
  snprintf(_queue_name, sizeof(_queue_name), "%sRx_Queue", _name);

  const osMessageQueueAttr_t queue_attr = {
    .name      = _queue_name,
    .attr_bits = 0,
    .cb_mem    = nullptr,
    .cb_size   = 0,
    .mq_mem    = nullptr,
    .mq_size   = 0};

  _rx_queue_handle = osMessageQueueNew(16, sizeof(can_rx_msg_t), &queue_attr);
  if (_rx_queue_handle == nullptr)
  {
    return false;
  }

  // 创建互斥锁
  snprintf(_mutex_name, sizeof(_mutex_name), "%sTx_Mutex", _name);

  const osMutexAttr_t mutex_attr = {
    .name      = _mutex_name,
    .attr_bits = 0,
    .cb_mem    = nullptr,
    .cb_size   = 0};

  _tx_mutex_handle = osMutexNew(&mutex_attr);
  if (_tx_mutex_handle == nullptr)
  {
    return false;
  }

  return true;
}


/**
 * @brief 配置CAN过滤器
 *
 * @note 配置为接收所有标准ID
 *       过滤器索引0，接收所有标准帧到FIFO0
 */
HAL_StatusTypeDef bsp_can::config_filter()
{
  FDCAN_FilterTypeDef sFilterConfig;

  sFilterConfig.IdType       = FDCAN_STANDARD_ID;       // 标准ID
  sFilterConfig.FilterIndex  = 0;                       // 过滤器索引0
  sFilterConfig.FilterType   = FDCAN_FILTER_RANGE;      // 范围过滤器
  sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0; // 接收到的数据放入FIFO0
  sFilterConfig.FilterID1    = 0x000;                   // ID范围起始
  sFilterConfig.FilterID2    = 0x7FF;                   // ID范围结束（接收所有）

  if (HAL_FDCAN_ConfigFilter(_hfdcan, &sFilterConfig) != HAL_OK)
  {
    return HAL_ERROR;
  }

  // 配置全局过滤器：接收所有未匹配的报文到FIFO0
  if (HAL_FDCAN_ConfigGlobalFilter(_hfdcan,
                                   FDCAN_ACCEPT_IN_RX_FIFO0, // 接收非冗余帧到FIFO0
                                   FDCAN_ACCEPT_IN_RX_FIFO0,
                                   FDCAN_FILTER_REMOTE, // 远程帧处理
                                   FDCAN_FILTER_REMOTE)
      != HAL_OK)
  {
    return HAL_ERROR;
  }

  return HAL_OK;
}


/**
 * @brief 启动CAN硬件
 *
 * @note 根据工作模式配置FDCAN
 * @note 暂不支持CANFD：使用经典CAN模式（FDCAN_CLASSIC_CAN）
 */
HAL_StatusTypeDef bsp_can::start_hardware()
{
  // 配置工作模式
  // 注意：STM32H7的FDCAN支持多种模式配置
  switch (_work_mode)
  {
    case can_mode::NORMAL:
      // 正常模式：无需额外配置，使用默认设置
      break;

    case can_mode::SILENT:
      // 静默模式：通过HAL库配置
      // 暂未实现，可通过HAL_FDCAN_SetMode()
      break;

    case can_mode::LOOPBACK:
      // 环回模式：通过HAL库配置
      // 暂未实现，可通过HAL_FDCAN_SetMode()
      break;

    default:
      break;
  }

  // 启动FDCAN
  if (HAL_FDCAN_Start(_hfdcan) != HAL_OK)
  {
    return HAL_ERROR;
  }

  return HAL_OK;
}


/**
 * @brief 启动CAN接收
 *
 * @note 开启FIFO0新消息中断
 */
HAL_StatusTypeDef bsp_can::start_reception()
{
  // 开启FIFO0新消息中断
  if (HAL_FDCAN_ActivateNotification(_hfdcan, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK)
  {
    return HAL_ERROR;
  }

  // 开启发送完成中断（可选）
  // if (HAL_FDCAN_ActivateNotification(_hfdcan, FDCAN_IT_TX_COMPLETE, 0) != HAL_OK)
  // {
  //   return HAL_ERROR;
  // }

  return HAL_OK;
}


/**
 * @brief 清理资源
 */
void bsp_can::cleanup_resources()
{
  // 删除消息队列
  if (_rx_queue_handle != nullptr)
  {
    osMessageQueueDelete(_rx_queue_handle);
    _rx_queue_handle = nullptr;
  }

  // 删除互斥锁
  if (_tx_mutex_handle != nullptr)
  {
    osMutexDelete(_tx_mutex_handle);
    _tx_mutex_handle = nullptr;
  }

  // 停止CAN硬件
  if (_hfdcan != nullptr)
  {
    HAL_FDCAN_Stop(_hfdcan);
  }

  _is_initialized = false;
}


/**
 * @brief 发送标准CAN数据帧（阻塞式）
 *
 * @note 内部使用互斥锁保护发送寄存器
 * @note 暂不支持CANFD：使用经典CAN帧格式
 */
HAL_StatusTypeDef bsp_can::send(uint32_t stdId, uint8_t *pData, uint8_t len, uint32_t timeout)
{
  // 参数检查
  if (pData == nullptr || len > 8)
  {
    return HAL_ERROR;
  }

  // 构建发送帧头
  FDCAN_TxHeaderTypeDef txHeader;

  txHeader.Identifier  = stdId;             // 标准CAN ID
  txHeader.IdType      = FDCAN_STANDARD_ID; // 标准ID类型
  txHeader.TxFrameType = FDCAN_DATA_FRAME;  // 数据帧

  // 设置数据长度
  switch (len)
  {
    case 0:
      txHeader.DataLength = FDCAN_DLC_BYTES_0;
      break;
    case 1:
      txHeader.DataLength = FDCAN_DLC_BYTES_1;
      break;
    case 2:
      txHeader.DataLength = FDCAN_DLC_BYTES_2;
      break;
    case 3:
      txHeader.DataLength = FDCAN_DLC_BYTES_3;
      break;
    case 4:
      txHeader.DataLength = FDCAN_DLC_BYTES_4;
      break;
    case 5:
      txHeader.DataLength = FDCAN_DLC_BYTES_5;
      break;
    case 6:
      txHeader.DataLength = FDCAN_DLC_BYTES_6;
      break;
    case 7:
      txHeader.DataLength = FDCAN_DLC_BYTES_7;
      break;
    case 8:
    default:
      txHeader.DataLength = FDCAN_DLC_BYTES_8;
      break;
  }

  // 配置发送参数
  // 暂不支持CANFD：使用经典CAN模式
  txHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;   // 错误状态指示
  txHeader.BitRateSwitch       = FDCAN_BRS_OFF;      // 波特率切换关闭
  txHeader.FDFormat            = FDCAN_CLASSIC_CAN;  // 经典CAN格式
  txHeader.TxEventFifoControl  = FDCAN_NO_TX_EVENTS; // 不生成发送事件
  txHeader.MessageMarker       = stdId;              // 消息标记

  // 使用互斥锁保护发送寄存器
  if (_tx_mutex_handle != nullptr)
  {
    osStatus_t mutexStatus = osMutexAcquire(_tx_mutex_handle, timeout);
    if (mutexStatus != osOK)
    {
      return HAL_TIMEOUT; // 获取互斥锁超时
    }
  }

  // 等待发送FIFO有空间
  uint32_t       timeoutCount = 0;
  const uint32_t maxTimeout   = 5; // 最大等待次数

  while (HAL_FDCAN_GetTxFifoFreeLevel(_hfdcan) == 0)
  {
    if (++timeoutCount > maxTimeout)
    {
      if (_tx_mutex_handle != nullptr)
      {
        osMutexRelease(_tx_mutex_handle);
      }
      return HAL_TIMEOUT;
    }
    osDelay(1); // 简单退避
  }

  // 发送数据
  HAL_StatusTypeDef status = HAL_FDCAN_AddMessageToTxFifoQ(_hfdcan, &txHeader, pData);

  // 释放互斥锁
  if (_tx_mutex_handle != nullptr)
  {
    osMutexRelease(_tx_mutex_handle);
  }

  return status;
}


/**
 * @brief 接收CAN数据帧（阻塞式）
 *
 * @note 从消息队列中获取数据
 */
osStatus_t bsp_can::receive(can_rx_msg_t *msg, uint32_t timeout)
{
  if (msg == nullptr)
  {
    return osErrorParameter;
  }

  if (_rx_queue_handle == nullptr)
  {
    return osErrorResource;
  }

  // 从消息队列获取数据
  return osMessageQueueGet(_rx_queue_handle, msg, nullptr, timeout);
}


/**
 * @brief 获取接收队列中的消息数量
 */
uint32_t bsp_can::get_queue_count()
{
  if (_rx_queue_handle != nullptr)
  {
    return osMessageQueueGetCount(_rx_queue_handle);
  }
  return 0;
}


/**
 * @brief 注册实例到静态注册表
 */
bool bsp_can::register_instance()
{
  // 检查是否已满
  if (_instance_count >= MAX_INSTANCES)
  {
    return false;
  }

  // 添加到注册表
  _instances[_instance_count] = this;
  _instance_count++;

  return true;
}


/**
 * @brief 通过CAN句柄查找实例
 */
bsp_can *bsp_can::get_instance_by_handle(FDCAN_HandleTypeDef *hfdcan)
{
  // 遍历注册表，查找匹配的CAN句柄
  for (size_t i = 0; i < _instance_count; i++)
  {
    if (_instances[i] != nullptr && _instances[i]->_hfdcan == hfdcan)
    {
      return _instances[i];
    }
  }

  return nullptr;
}
