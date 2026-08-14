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
  }

  /* 初始化BSP设备 */
  bsp_init();

  /* 初始化设备 */
  device_init();

  /* 初始化协议层 */
  protocol_init();

  /* 菜单任务（LCD 菜单 + 按键，事件转发给 menu 模块；优先级最高） */
  xTaskCreate(task_menu, "t_menu", 2048, NULL, tskIDLE_PRIORITY + 6, NULL);

  /* 菜单消息测试任务（3 个，各阻塞等待 menu_sem） */
  xTaskCreate(msg_task_task1, "m_tsk1", 256, NULL, tskIDLE_PRIORITY + 4, NULL);
  xTaskCreate(msg_task_task2, "m_tsk2", 256, NULL, tskIDLE_PRIORITY + 4, NULL);
  xTaskCreate(msg_task_task3, "m_tsk3", 256, NULL, tskIDLE_PRIORITY + 4, NULL);

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

  uint8_t data1[8] = {'1', '2', '3', '4', '5', '6', '7', '8'};

  vTaskDelay(pdMS_TO_TICKS(1000));
  printf("Default Task Started\n");
  vTaskDelay(pdMS_TO_TICKS(1000));

  for (;;)
  {
    bsp_can1.send(0x101, data1);
    bsp_can2.send(0x101, data1);
    bsp_can3.send(0x101, data1);
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
