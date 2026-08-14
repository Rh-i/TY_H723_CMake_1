#include "app_message.hpp"

#include "FreeRTOS.h" // IWYU pragma: keep
#include "task.h"


/**
 * @brief CAN 数据分发任务
 *
 * @note 接入业务后实现：轮询 bsp_can1.receive() 取帧，
 *       按帧 ID 分发到各设备回调（如 DM IMU 的 on_can_message()）。
 *
 * @param argument 任务参数（未使用，NULL）
 */
extern "C" void app_message_can_task(void *argument)
{
  (void)argument; // 未使用参数

  for (;;)
  {
    // TODO: 接入业务后实现 CAN 帧分发（一路 CAN 多个设备）
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}


/**
 * @brief UART 数据分发任务
 *
 * @note 接入业务后实现：轮询 bsp_uartX.receive() 取数据，
 *       按帧首字节地址分发到各设备接收处理（如多个 EmmV5 电机共用一个串口）。
 *
 * @param argument 任务参数（未使用，NULL）
 */
extern "C" void app_message_uart_task(void *argument)
{
  (void)argument; // 未使用参数

  for (;;)
  {
    // TODO: 接入业务后实现 UART 帧分发（一个串口多个设备）
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
