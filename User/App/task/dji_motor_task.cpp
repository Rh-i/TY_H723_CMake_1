#include "app_task.hpp"

#include "FreeRTOS.h" // IWYU pragma: keep
#include "bsp_cfg.hpp"
#include "dji_motor.hpp"
#include "dm_motor.hpp"
#include "motor_tx_manager.hpp"
#include "task.h"

#include <stdint.h>


volatile uint32_t dji_motor_task_loop_count = 0U;
volatile uint32_t dji_motor_rx_frame_count = 0U;
volatile uint32_t dji_motor_rx_dispatch_count = 0U;
volatile uint32_t dji_motor_rx_ignored_count = 0U;
volatile Status dji_motor_task_send_status = Status::NOT_INIT;


namespace
{
void process_registered_can(BspCan &can_item)
{
  if (!dji_motor_uses_can(can_item) && !dm_motor_uses_can(can_item))
  {
    return;
  }

  CanRxMsg rx = {};
  while (can_item.receive(&rx, 0U) == Status::OK)
  {
    ++dji_motor_rx_frame_count;
    Status dispatch_status = dji_motor_dispatch_rx(can_item, rx);
    if (dispatch_status != Status::OK)
    {
      dispatch_status = dm_motor_dispatch_rx(can_item, rx);
    }

    if (dispatch_status == Status::OK)
    {
      ++dji_motor_rx_dispatch_count;
    }
    else
    {
      ++dji_motor_rx_ignored_count;
    }
  }
}
} // namespace


extern "C" void dji_motor_task(void *argument)
{
  (void)argument; // 任务不需要外部参数。
  static_assert(configTICK_RATE_HZ == 1000U,
                "dji_motor_task requires a 1 kHz FreeRTOS tick");

  TickType_t wake_time = xTaskGetTickCount();
  for (;;)
  {
    process_registered_can(bsp_can1);
    process_registered_can(bsp_can2);
    process_registered_can(bsp_can3);

    dji_motor_task_send_status = MotorTxManager::update();
    ++dji_motor_task_loop_count;
    vTaskDelayUntil(&wake_time, pdMS_TO_TICKS(1U));
  }
}
