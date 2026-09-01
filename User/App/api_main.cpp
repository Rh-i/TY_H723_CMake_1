#include "api_main.h"
#include "FreeRTOS.h" // IWYU pragma: keep
#include "main.h"     // IWYU pragma: keep
#include "task.h"

#include <stdint.h>
#include <stdio.h>

/* BSP */
#include "bsp_cfg.hpp"

/* Device */
#include "device_cfg.hpp"

/* Protocol */
#include "protocol_cfg.hpp"

/* 任务声明 */
#include "app_task.hpp"


/* ==================== 全局变量定义 ==================== */

///< 菜单信号量数组（3 个，menu 模块长按触发，由 api_main 统一创建）
SemaphoreHandle_t menu_sem[3];

/* C620/M3508 CAN1 实机测试诊断量（可在调试器 Live Watch 中查看） */
volatile Status   can1_statu                = Status::NOT_INIT;
volatile uint32_t can1_tx_ok_count          = 0;
volatile uint32_t can1_tx_full_count        = 0;
volatile uint32_t can1_rx_count             = 0;
volatile uint32_t can1_feedback_202_count   = 0;
volatile uint32_t can1_last_rx_id           = 0;
volatile int16_t  c620_speed_rpm            = 0;
volatile uint16_t c620_peak_abs_speed_rpm   = 0;
volatile int16_t  c620_given_current        = 0;
volatile uint8_t  c620_temperature          = 0;
volatile uint32_t can1_last_error_code      = 0;
volatile uint32_t can1_tx_error_count       = 0;
volatile uint32_t can1_rx_error_count       = 0;
volatile uint32_t can1_bus_off              = 0;


/**
 * @brief FreeRTOS后的相关初始化
 *
 * @note 在main.c的MX_FREERTOS_INIT函数中调用
 *       用于创建FreeRTOS任务和初始化外设驱动
 *
 *       初始化顺序：
 *         1. 统一创建信号量（必须在创建任何任务之前！）
 *         2. bsp_init()     —— 外设 BSP 初始化
 *         3. device_init()  —— 设备层初始化
 *         4. protocol_init()—— 协议层初始化
 *         5. ...
 */
void all_init()
{
  // 创建信号量
  for (int i = 0; i < 3; i++)
  {
    // 菜单信号量
    menu_sem[i] = xSemaphoreCreateBinary();
    configASSERT(menu_sem[i] != nullptr);
  }

  /* 初始化BSP设备 */
  bsp_init();

  /* 初始化设备 */
  device_init();

  /* 初始化协议层 */
  protocol_init();

  /* 菜单任务（LCD 菜单 + 按键，事件转发给 menu 模块；优先级最高） */
  configASSERT(xTaskCreate(task_menu, "t_menu", 2048, NULL, tskIDLE_PRIORITY + 6, NULL) == pdPASS);

  /* 菜单消息测试任务（3 个，各阻塞等待 menu_sem） */
  configASSERT(xTaskCreate(msg_task_task1, "m_tsk1", 256, NULL, tskIDLE_PRIORITY + 4, NULL) == pdPASS);
  configASSERT(xTaskCreate(msg_task_task2, "m_tsk2", 256, NULL, tskIDLE_PRIORITY + 4, NULL) == pdPASS);
  configASSERT(xTaskCreate(msg_task_task3, "m_tsk3", 256, NULL, tskIDLE_PRIORITY + 4, NULL) == pdPASS);

  printf("freertos_init_ok\n");
}


/**
 * @brief ST的默认任务，弱定义，在这里定义，方便寻找
 *
 * @note 任务名保留 CubeMX 生成的 StartDefaultTask（便于 CubeMX 重新生成时同名兼容）；
 *       自定义任务函数命名遵循小写蛇形规范（如 task_menu）。
 *
 * @note freertos调用cpp的函数，需要extern "C"修饰，避免C++的名称修饰导致找不到函数
 *
 * @param argument 任务参数
 */
extern "C" void StartDefaultTask(void *argument)
{
  (void)argument; // 未使用参数

  constexpr int16_t kTestCurrent = 512; // 512 / 16384 * 20 A = 0.625 A
  uint8_t           tx_data[8]   = {0}; // 0x200: ID2 位于 DATA[2:3]
  CanRxMsg          rx_msg       = {};
  TickType_t        wake_time    = xTaskGetTickCount();
  const TickType_t  test_start   = wake_time;
  uint32_t          diagnostic_divider = 0;

  printf("Default Task Started\n");

  for (;;)
  {
    const TickType_t elapsed = xTaskGetTickCount() - test_start;

    // 上电 2 秒后仅驱动 ID2 一秒，随后持续发送零电流。
    const int16_t current = ((elapsed >= pdMS_TO_TICKS(2000)) &&
                             (elapsed < pdMS_TO_TICKS(3000)))
                              ? kTestCurrent
                              : 0;
    tx_data[2] = static_cast<uint8_t>(static_cast<uint16_t>(current) >> 8);
    tx_data[3] = static_cast<uint8_t>(current);

    can1_statu = bsp_can1.send(0x200, tx_data);
    if (can1_statu == Status::OK)
    {
      ++can1_tx_ok_count;
    }
    else if (can1_statu == Status::FULL)
    {
      ++can1_tx_full_count;
    }

    // C620 ID2 的反馈标识符为 0x202，默认反馈频率为 1 kHz。
    while (bsp_can1.receive(&rx_msg, 0) == Status::OK)
    {
      ++can1_rx_count;
      can1_last_rx_id = rx_msg.header.Identifier;
      if (rx_msg.header.Identifier == 0x202U)
      {
        ++can1_feedback_202_count;
        c620_speed_rpm     = static_cast<int16_t>((static_cast<uint16_t>(rx_msg.data[2]) << 8) | rx_msg.data[3]);
        c620_given_current = static_cast<int16_t>((static_cast<uint16_t>(rx_msg.data[4]) << 8) | rx_msg.data[5]);
        c620_temperature   = rx_msg.data[6];
        const int32_t speed = c620_speed_rpm;
        const uint16_t abs_speed = static_cast<uint16_t>(speed < 0 ? -speed : speed);
        if (abs_speed > c620_peak_abs_speed_rpm)
        {
          c620_peak_abs_speed_rpm = abs_speed;
        }
      }
    }

    // 每 100 ms 保存一次协议状态。LastErrorCode=3 通常表示 ACK error。
    if (++diagnostic_divider >= 100U)
    {
      diagnostic_divider = 0;
      FDCAN_ProtocolStatusTypeDef protocol_status = {};
      FDCAN_ErrorCountersTypeDef error_counters = {};
      if (HAL_FDCAN_GetProtocolStatus(&hfdcan1, &protocol_status) == HAL_OK)
      {
        can1_last_error_code = protocol_status.LastErrorCode;
        can1_bus_off         = protocol_status.BusOff;
      }
      if (HAL_FDCAN_GetErrorCounters(&hfdcan1, &error_counters) == HAL_OK)
      {
        can1_tx_error_count = error_counters.TxErrorCnt;
        can1_rx_error_count = error_counters.RxErrorCnt;
      }
    }

    vTaskDelayUntil(&wake_time, pdMS_TO_TICKS(1));
  }
}
