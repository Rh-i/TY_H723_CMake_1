#include "bsp_gpio_exti.hpp"
#include "gpio.h" // IWYU pragma: keep

int exti_count = 0;

extern "C" void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  // TODO 需要加消抖，目前是概率一次按钮会触发两次计数值++，给一个10ms以内的震荡次数都算一次
  if(GPIO_Pin == GPIO_PIN_15)
  {
    exti_count++;
		if(exti_count > 1000)
		{
			exti_count = 0;
		}
  }
}
