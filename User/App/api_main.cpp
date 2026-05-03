#include "api_main.h"
#include "cmsis_os2.h"
#include "main.h" // IWYU pragma: keep
#include "stdio.h"

/* BSP */
#include "bsp_cfg.hpp"
#include <stdint.h>


/**
 * @brief FreeRTOS后的相关初始化
 *
 * @note 在main.c的MX_FREERTOS_INIT函数中调用
 *       用于创建FreeRTOS任务和初始化外设驱动
 *
 */
void all_init()
{
  /* 初始化BSP设备 */
  bsp_init();

  /* 初始化协议层 */

  /* 初始化设备 */

  /* 初始化信号量 */

  printf("freertos_init_ok\n");
}


/**
 * @brief FreeRTOS任务说明
 *
 * @note 以下均为FreeRTOS的内容定义，使用C调用C++，需要extern "C"声明，让RTOS接管
 *       CubeMX提供了FreeRTOS配置，故而使用它的CMSIS_OS2
 *       CMSIS_OS2封装了一层，导致很多东西和原生的FreeRTOS不一样
 *       CMSIS_OS2的初始句柄不对外声明，如果想用只能单独extern出来用
 *       CMSIS_OS2做了层封装，方便使用。但是原生的FreeRTOS在以后要用的时候，还是要花时间适应
 *       CMSIS_OS2的默认任务只能weak声明，其他的可以使用外部声明
 *       printf要加\n
 *
 * @note 在C++中使用FreeRTOS的Task函数时，
 *       需要将任务函数声明为extern "C"格式，
 *       同时函数参数必须是void *pvParameters。
 */


uint64_t defaultCount = 0;
uint8_t data1[8]{'1','2','3','4','5','6','7','8'};

/**
 * @brief 默认任务，这个原本命名为_start_default_task。但是每次开FreeRTOS这个里面，默认是这个名字
 *
 * @note 保留这个名字，但是其他任务要类似：_start_default_task
 * @param argument 任务参数
 */
extern "C" void StartDefaultTask(void *argument)
{
  (void)argument; // 未使用参数

  osDelay(1000);
  printf("Default Task Started\n");
  osDelay(1000);

  for (;;)
  {
    // defaultCount++;
    bsp_usart1.send(data1, 8,0);
    osDelay(100);
  }
}
