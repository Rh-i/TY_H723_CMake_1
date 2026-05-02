#include "bsp_gpio_exti.hpp"
#include "gpio.h" // IWYU pragma: keep
#include "stm32h7xx_hal.h"

// 按键消抖延时时间（ms）
#define DEBOUNCE_DELAY_MS 5

int exti_count = 0;

extern "C" void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == GPIO_PIN_15)
  {
    uint32_t now = HAL_GetTick();

    static uint32_t last_trigger_time = 0;

    // 如果距离上次触发小于5ms，直接忽略
    if (now - last_trigger_time < DEBOUNCE_DELAY_MS)
    {
      return;
    }
    // 更新触发时间
    last_trigger_time = now;

    // 计数值++
    exti_count++;
    if (exti_count > 1000)
    {
      exti_count = 0;
    }
  }
}
