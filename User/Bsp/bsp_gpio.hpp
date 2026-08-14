/**
 * @file bsp_gpio.hpp
 * @author Rh
 * @brief GPIO 统一封装 — 输出引脚控制
 * @version 0.3
 * @date 2026-07-25
 *
 * @copyright Copyright (c) 2026
 *
 * @details BspGpio 类：GPIO 输出引脚的 set/reset/toggle/read 封装。
 *
 * @note 使用示例：
 * @code
 *   BspGpio led;
 *   led.init({GPIOA, GPIO_PIN_5});
 *   led.set();
 *   led.toggle();
 * @endcode
 */

#ifndef __BSP_GPIO_HPP__
#define __BSP_GPIO_HPP__

#include "main.h" // IWYU pragma: keep
#include <stdint.h>

#include "status.hpp" // 统一状态码


/* ==================== GPIO 输出引脚封装类 ==================== */

/**
 * @brief GPIO 输出引脚封装类
 *
 * @note 两种初始化方式:
 *       1. 构造时绑定: BspGpio power({GPIOA, GPIO_PIN_5});
 *       2. 默认构造 + init(): BspGpio led; led.init({GPIOA, GPIO_PIN_5});
 */
class BspGpio
{
public:
  /** GPIO 引脚配置 */
  struct Config
  {
    GPIO_TypeDef *port; ///< GPIO 端口 (GPIOA / GPIOB / ...)
    uint16_t      pin;  ///< 引脚掩码 (GPIO_PIN_x)
  };

  BspGpio() = default;

  BspGpio(GPIO_TypeDef *port, uint16_t pin) : _port(port), _pin(pin)
  {
  }

  /**
   * @brief 绑定引脚（硬件已由 CubeMX 初始化）
   * @param cfg 端口 + 引脚
   * @return Status OK=绑定成功，BAD_ARG=端口/引脚非法
   */
  Status init(const Config &cfg)
  {
    if (cfg.port == nullptr || cfg.pin == 0U)
    {
      return Status::BAD_ARG; // 端口/引脚非法
    }
    _port = cfg.port;
    _pin  = cfg.pin;
    return Status::OK;
  }

  void set() const { HAL_GPIO_WritePin(_port, _pin, GPIO_PIN_SET); }
  void reset() const { HAL_GPIO_WritePin(_port, _pin, GPIO_PIN_RESET); }
  void toggle() const { HAL_GPIO_TogglePin(_port, _pin); }

  void write(bool state) const
  {
    HAL_GPIO_WritePin(_port, _pin, state ? GPIO_PIN_SET : GPIO_PIN_RESET);
  }

  bool read() const { return (HAL_GPIO_ReadPin(_port, _pin) != GPIO_PIN_RESET); }

  GPIO_TypeDef *get_port() const { return _port; }
  uint16_t      get_pin() const { return _pin; }

private:
  GPIO_TypeDef *_port = nullptr;
  uint16_t      _pin  = 0U;
};

#endif // __BSP_GPIO_HPP__
