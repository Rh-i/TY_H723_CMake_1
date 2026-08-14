#include "FreeRTOS.h" // IWYU pragma: keep
#include "semphr.h"
#include "task.h"

#include "api_main.h"  // menu_sem 声明
#include "bsp_cfg.hpp" // bsp_uart1

extern "C" void msg_task_task1(void *arg)
{
  uint8_t buf[3] = {0x01, 0x01, '\n'};
  (void)arg;
  for (;;)
  {
    xSemaphoreTake(menu_sem[0], portMAX_DELAY);
    bsp_uart1.send(buf, sizeof(buf), nullptr, 100);
  }
}

extern "C" void msg_task_task2(void *arg)
{
  (void)arg;
  uint8_t buf[3] = {0x02, 0x02, '\n'};
  for (;;)
  {
    xSemaphoreTake(menu_sem[1], portMAX_DELAY);
    bsp_uart1.send(buf, sizeof(buf), nullptr, 100);
  }
}

extern "C" void msg_task_task3(void *arg)
{
  (void)arg;
  uint8_t buf[3] = {0x03, 0x03, '\n'};
  for (;;)
  {
    xSemaphoreTake(menu_sem[2], portMAX_DELAY);
    bsp_uart1.send(buf, sizeof(buf), nullptr, 100);
  }
}
