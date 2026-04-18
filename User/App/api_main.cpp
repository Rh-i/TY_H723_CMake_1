#include "api_main.h"
#include "cmsis_os2.h"
#include "main.h" // IWYU pragma: keep
#include "stdio.h"

/* BSP */
#include "bsp_cfg.hpp"

/* SVC */
#include "data_pack.hpp"


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

osThreadId_t         usb_tx_task_handle;
const osThreadAttr_t usb_tx_handler_task_arrtibutes = {
  .name       = "usb_tx_task",
  .stack_size = 512 * 4,
  .priority   = (osPriority_t)osPriorityNormal,
};

osThreadId_t         usb_rx_task_handle;
const osThreadAttr_t usb_rx_handler_task_arrtibutes = {
  .name       = "usb_rx_task",
  .stack_size = 512 * 4,
  .priority   = (osPriority_t)osPriorityNormal,
};

osSemaphoreId_t         usb_init_semaphore_handle;
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

  bsp_can1.init();

  /* 初始化协议层 */

  /* 初始化设备 */

  /* 初始化信号量 */

  // @Choose-B 记得修改这些任务和信号量的定义。这些内容其实bsp_usb.init()，这个里面就可以实现，具体的参考我的写法。这样太脏了耦合度比较高，变量啥的都封装进去。然后尽力把tinyusb变成一个对象库

  usb_init_semaphore_handle = osSemaphoreNew(1, 0, &usb_init_handler_arrtibutes);

  usb_tx_task_handle = osThreadNew(_usb_tx_handler_task, nullptr, &usb_tx_handler_task_arrtibutes);

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

uint8_t   a1;
float     f1;
data_pack test_pack;
/**
 * @brief 默认任务，这个原本命名为_start_default_task。但是每次开FreeRTOS这个里面，默认是这个名字
 *
 * @note 保留这个名字，但是其他任务要类似：_start_default_task
 * @param argument 任务参数
 */
extern "C" void StartDefaultTask(void *argument)
{
  (void)argument; // 未使用参数

  bsp_usb.init();
  osSemaphoreRelease(usb_init_semaphore_handle); // 初始化完成信号

  osDelay(1000);
  printf("Default Task Started\n");
  osDelay(1000);

  test_pack.link_data(&a1);
  test_pack.link_data(&f1);

  a1 = 1;
  f1 = 1.5f;

  for (;;)
  {
    test_pack.get_data();
    a1++;
    f1 += 0.5f;
    test_pack.distribute_data();
    a1++;
    f1 += 0.5f;
    osDelay(1000);
  }
}


uint8_t   test_data;
data_pack tx_data_pack(0xAA);

extern "C" void _usb_tx_handler_task(void *argument)
{
  (void)argument;

  osStatus_t tx_statu;

  /* 链接各模块变量 */
  tx_data_pack.link_data(&test_data);

  printf("USB TX Task Started\n");

  for (;;)
  {
    test_data = HAL_GetTick() % 200;
    tx_statu  = tx_data_pack.send_data();
    if (tx_statu != osOK)
    {
      printf("USB TX ERROR!\n");
    }
    osDelay(1);
  }
}

uint8_t         rx_test_data = 0;
data_pack       rx_data_pack(0xAA);
extern "C" void _usb_rx_handler_task(void *argument)
{
  (void)argument;

  osStatus_t rx_statu;

  /* 链接各模块变量 */
  rx_data_pack.link_data(&rx_test_data);

  printf("USB RX Task Started\n");

  osSemaphoreAcquire(usb_init_semaphore_handle, osWaitForever);
  for (;;)
  {
    bsp_usb.task();
    rx_statu = rx_data_pack.receive_data();
    if (rx_statu != osOK)
    {
      printf("USB RX ERROR!");
    }
    osDelay(1);
  }
}