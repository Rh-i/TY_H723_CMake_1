/**
 * @file pid_speed.hpp
 * @brief 速度环PID — 内嵌 Pid 实例，自包含
 */

#ifndef __PID_SPEED_HPP__
#define __PID_SPEED_HPP__

#include "pid.hpp"
#include <stdint.h>

class PidSpeed
{
public:
  PidSpeed();

  /**
   * @brief 速度环PID全参数初始化
   *
   * @param kp         比例系数
   * @param ki         积分系数
   * @param kd         微分系数
   * @param out_max    输出限幅（PWM 占空比最大值）
   * @param i_max      积分限幅（绝对值），≤ out_max
   * @param dt         采样周期 (s)，如 0.02=20ms；≤0 时回退 dt=1.0
   * @param input_min  速度输入下限（min ≥ max 时关闭校验）
   * @param input_max  速度输入上限
   */
  void  init(float kp, float ki, float kd,
             float out_max, float i_max,
             float dt,
             float input_min, float input_max);

  float calculate_speed(float target_speed, float current_speed_cm_s);
  void  reset();

  float get_output()    const { return _pid.get_output(); }
  float get_speed_cm_s() const { return _speed_cm_s; }

private:
  Pid     _pid;           /* 内嵌的位置式PID */
  float   _speed_cm_s;    /* 当前速度 cm/s */
  float   _target_speed;  /* 目标速度 cm/s */
};

#endif // __PID_SPEED_HPP__
