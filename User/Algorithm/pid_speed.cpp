/**
 * @file pid_speed.cpp
 * @brief 速度环PID — 实现
 *
 * @note 速度计算公式:
 *   speed_cm_s = delta_count × WHEEL_PERIMETER_CM / ENCODER_RESOLUTION / (CONTROL_PERIOD_MS/1000)
 *              = delta_count × 19.48 / 2000 / 0.02
 *              = delta_count × 0.487 cm/s
 *
 * @note 输出为 PWM 占空比 (0 ~ out_max)
 * @note 积分限幅、采样周期、输入范围均在 init() 中一次性配置，无需额外调用。
 */

#include "pid_speed.hpp"

/* ==================== 构造 / 析构 ==================== */

PidSpeed::PidSpeed()
{
  init(0, 0, 0, 0, 0, 0, 0, 0);
}

/* ==================== 初始化 ==================== */

/**
 * @brief 8 参数全配置（最终实现）
 */
void PidSpeed::init(float kp, float ki, float kd, float out_max, float i_max, float dt, float input_min, float input_max)
{
  _pid.init(kp, ki, kd, out_max, i_max, dt, input_min, input_max);

  _speed_cm_s   = 0;
  _target_speed = 0;
}

/* ==================== 计算 ==================== */

/**
 * @brief 速度环PID计算（直接传入速度值，不做编码器换算）
 *
 * @param target_speed      目标速度 cm/s
 * @param current_speed_cm_s 当前速度 cm/s（由外部 Motor 换算）
 * @return PWM 输出值
 */
float PidSpeed::calculate_speed(float target_speed, float current_speed_cm_s)
{
  _speed_cm_s   = current_speed_cm_s;
  _target_speed = target_speed;

  return _pid.calculate(target_speed, current_speed_cm_s);
}

/* ==================== 重置 ==================== */

/**
 * @brief 速度环PID重置
 *
 * @note 清零积分累计和速度状态，保留 PID 系数不变
 */
void PidSpeed::reset()
{
  _pid.reset();

  _speed_cm_s   = 0;
  _target_speed = 0;
}
