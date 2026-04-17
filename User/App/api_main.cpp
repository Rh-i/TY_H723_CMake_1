#include "api_main.h"
#include "cmsis_os2.h"
#include "main.h" // IWYU pragma: keep
#include "stdio.h"


/* BSP */
#include "bsp_cfg.hpp"


/* DVC */
#include "JC2804.hpp"


/* SVC */
#include "protocol_usart.hpp"

#include "protocol_maixcam.hpp"


#ifdef __cplusplus
extern "C"
{
#endif


  /**
   * @brief 主应用程序初始化（非FreeRTOS）
   *
   * @note 在main.c的main函数中调用，用于非FreeRTOS环境下的初始化
   *
   */
  void app_init()
  {
    printf("\napp_init_ok\n");
  }


  /* ==================== 任务句柄定义 ==================== */

  osThreadId_t         can_rx_task_handle; ///< CAN接收后处理任务句柄
  const osThreadAttr_t can_rx_handler_task_attributes = {
    .name       = "can_rx_task",
    .stack_size = 128 * 4,
    .priority   = (osPriority_t)osPriorityNormal,
  };


  /**
   * @brief FreeRTOS相关初始化
   *
   * @note 在main.c的MX_FREERTOS_INIT函数中调用
   *       用于创建FreeRTOS任务和初始化外设驱动
   *
   */
  void freertos_init()
  {
    /* 初始化BSP设备 */
    bsp_usart1.init();
    bsp_can1.init();

    /* 初始化协议层（需要在maixcam之前初始化） */
    protocal_usart_1.init();

    /* 初始化MaixCam协议（内部会调用protocal_usart_9） */
    maixcam.init();

    /* 初始化设备 */

    /* 创建CAN接收后处理任务 */
    can_rx_task_handle = osThreadNew(_can_rx_handler_task, nullptr, &can_rx_handler_task_attributes);

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


  /**
   * @brief 默认任务
   *
   * @param argument 任务参数
   */
  void StartDefaultTask(void *argument)
  {
    (void)argument; // 未使用参数

    osDelay(1000);
    printf("Default Task Started\n");
    osDelay(1000);

    uint8_t data[4] = {'1', '2', '3', '\n'};

    for (;;)
    {
      bsp_usart1.send(data, 4);
      osDelay(1000);
    }
  }


  /**
   * @brief CAN接收后处理任务
   *
   * @note 处理从CAN总线接收到的数据，根据设备ID分发到对应的电机处理
   *
   * @param argument 任务参数
   */
  void _can_rx_handler_task(void *argument)
  {
    (void)argument; // 未使用参数

    printf("CAN RX Task Started\n");
    can_rx_msg_t rx_msg;

    for (;;)
    {
      osStatus_t status = bsp_can1.receive(&rx_msg, osWaitForever);

      if (status == osOK)
      {
        /* 根据ID判断是哪个设备 */
        uint32_t device_id = rx_msg.header.Identifier;

        /* 查找对应的jc2804实例 */
        if (device_id == (motor_yaw._device_id + 0x580))
        {
          motor_yaw.on_can_message(rx_msg);
        }
        else if (device_id == (motor_pitch._device_id + 0x580))
        {
          motor_pitch.on_can_message(rx_msg);
        }
      }
    }
  }


#ifdef __cplusplus
}
#endif
