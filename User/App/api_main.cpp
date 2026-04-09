#include "api_main.h"
#include "cmsis_os2.h"
#include "main.h" // IWYU pragma: keep
#include "stdio.h"
#include "bsp_usb.h"

/* BSP */
#include "bsp_usart.hpp"


/* DVC */
#include "jc2804.hpp"


/* SVC */
#include "protocol_usart.hpp"

#include "protocol_maixcam.hpp"


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

osThreadId_t         usb_rx_task_handle;
const osThreadAttr_t usb_rx_handler_task_arrtibutes = {
  .name       = "usb_rx_task",
  .stack_size = 512 * 4,
  .priority   = (osPriority_t)osPriorityNormal,
};

osSemaphoreId_t usb_init_semaphore_handle;
const osSemaphoreAttr_t usb_init_handler_arrtibutes = {
  .name = "usb_init_semaphore",
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
  bsp_usart6.init();
  bsp_usart9.init();
  bsp_can1.init();

  /* 初始化协议层（需要在maixcam之前初始化） */
  protocal_usart_9.init();

  /* 初始化MaixCam协议（内部会调用protocal_usart_9） */
  maixcam.init();

  /* 初始化设备 */

  /* 初始化信号量 */
  usb_init_semaphore_handle = osSemaphoreNew(1,0,&usb_init_handler_arrtibutes);

  /* 创建CAN接收后处理任务 */
  can_rx_task_handle = osThreadNew(_can_rx_handler_task, nullptr, &can_rx_handler_task_attributes);

  usb_rx_task_handle = osThreadNew(_usb_rx_handler_task, nullptr, &usb_rx_handler_task_arrtibutes);

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

uint8_t txbuffer[10] = {'H','e','l','l','o','\n','\0'};
/**
 * @brief 默认任务
 *
 * @param argument 任务参数
 */
extern "C" void _defaultTask(void *argument)
{
  (void)argument; // 未使用参数

  bsp_usb.init();
  osSemaphoreRelease(usb_init_semaphore_handle); // 初始化完成信号

  osDelay(1000);
  printf("Default Task Started\n");
  osDelay(1000);

  // motor_yaw.enter_closed_loop();
  // osDelay(10);
  // motor_yaw.set_control_mode(3);
  // osDelay(10);

  for (;;)
  {
    bsp_usb.cdc_write(txbuffer, sizeof(txbuffer));
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
extern "C" void _can_rx_handler_task(void *argument)
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
      if (device_id == (motor_yaw._device_id + 0x600))
      {
        motor_yaw.on_can_message(&rx_msg);
      }
      else if (device_id == (motor_pitch._device_id + 0x600))
      {
        motor_pitch.on_can_message(&rx_msg);
      }
    }
  }
}

extern "C" void _usb_rx_handler_task(void *argument)
{
  (void)argument;

  printf("USB RX Task Started\n");

  osSemaphoreAcquire(usb_init_semaphore_handle,osWaitForever);
  for (;;) 
  {
    bsp_usb.task();
    osDelay(1);
  }
}
