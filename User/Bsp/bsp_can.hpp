/**
 * @file bsp_can.hpp
 * @author Rh
 * @brief CAN2.0 标准帧 8长度 收发驱动
 * @version 0.4
 * @date 2026-05-02
 *
 * @todo 1. 滤波器未处理
 *
 * @copyright Copyright (c) 2026
 *
 * @details 使用示例：
 *
 * @note 先在bsp_cfg实例化并外部声明，再在freertos_init()中初始化它
 *
 *      bsp_can1(&hfdcan1,"CAN1");
 *      bsp_can1.init();
 *
 * @note 如何使用：
 *
 *      uint8_t data[8] = {0x01,0x02...}; // 定义数据内容
 *      bsp_can1.send(0x101,data);        // 存入缓冲区中 自动发送
 *
 *      CanRxMsg_t data1 = {0};           // 定义数据内容
 *      bsp_can1.receive(&data1);         // 从缓冲区取值（自动接收到缓冲区）
 *
 */

#ifndef __BSP_CAN_HPP__
#define __BSP_CAN_HPP__

#include "cmsis_os2.h"
#include "fdcan.h"    // IWYU pragma: keep
#include "FreeRTOS.h" // IWYU pragma: keep
#include "message_buffer.h"
#include "task.h"


/**
 * @brief CAN工作模式
 */
enum class CanMode
{
  NORMAL   = 0, ///< 正常模式
  SILENT   = 1, ///< 静默模式
  LOOPBACK = 2  ///< 环回模式
};


/**
 * @brief CAN接收消息结构体（固定8字节数据）
 */
typedef struct
{
  FDCAN_RxHeaderTypeDef header;
  uint8_t               data[8];
} CanRxMsg_t;


/**
 * @brief CAN发送消息结构体（固定8字节数据）
 */
typedef struct
{
  uint32_t std_id;
  uint8_t  data[8];
} CanTxMsg_t;


/**
 * @brief CAN驱动类
 *
 * @note CAN2.0标准帧，固定8字节数据
 * @note 收发使用FreeRTOS Message Buffer
 * @note 发送不等待硬件FIFO，直接放入缓冲区，由中断完成发送
 */
class BspCan
{
public:
  BspCan(FDCAN_HandleTypeDef *hfdcan, const char *name = "CAN", CanMode mode = CanMode::NORMAL);
  ~BspCan();

  bool init();
  bool send(uint32_t stdId, uint8_t *pData);
  bool receive(CanRxMsg_t *msg, uint32_t timeout = osWaitForever);
  void trigger_tx();                                               // 触发一次发送（任务上下文调用）
  void trigger_tx_from_isr(BaseType_t *pxHigherPriorityTaskWoken); // 触发一次发送（ISR上下文调用）
  void process_fifo0_isr();                                        // FIFO0 中断处理（供中断回调调用）

  MessageBufferHandle_t _rx_message_buffer;
  MessageBufferHandle_t _tx_message_buffer;

private:
  FDCAN_HandleTypeDef *_hfdcan;
  const char          *_name;
  CanMode              _work_mode;

  bool config_filter();
  bool start_hardware();
  bool start_reception();
};

#endif
