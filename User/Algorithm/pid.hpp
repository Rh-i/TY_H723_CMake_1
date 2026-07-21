/**
 * @file pid.hpp
 * @brief 位置式PID控制器 — 类声明
 *
 * @note 位置式 PID 算法（带 dt 时间缩放）:
 *   e(k)     = target - feedback
 *   p_out    = Kp × e(k)
 *   integral += Ki × e(k) × dt       （带抗饱和 + 限幅）
 *   d_out    = Kd × (e(k)-e(k-1)) / dt
 *   output   = p_out + integral + d_out  （带限幅）
 *
 * @note 抗饱和策略：输出饱和时，若误差方向与饱和方向一致，停止积分累加。
 *       仅当误差能将输出拉回线性区时才继续积分（条件积分法）。
 */

#ifndef __PID_HPP__
#define __PID_HPP__

#include <stdint.h>

class Pid
{
public:
  Pid();

  /**
   * @brief PID 全参数初始化（推荐）
   *
   * @param kp         比例系数
   * @param ki         积分系数
   * @param kd         微分系数
   * @param out_max    输出限幅（绝对值）
   * @param i_max      积分限幅（绝对值）
   * @param dt         采样周期 (s)，如 0.02=20ms；≤0 时回退 dt=1.0
   * @param input_min  输入下限（min ≥ max 时关闭校验）
   * @param input_max  输入上限
   */
  void  init(float kp, float ki, float kd,
             float out_max, float i_max,
             float dt,
             float input_min, float input_max);

  float calculate(float target, float feedback);
  void  reset();

  /* ---- 访问器 ---- */
  float get_output() const { return _output; }
  float get_error()  const { return _error; }
  float get_dt()     const { return _dt; }

private:
  /**
   * @brief 输入合法性校验 + 钳位
   * @param value 原始输入值
   * @return 校验后的安全值
   */
  float clamp_input(float value) const;

  /* ---- PID 系数 ---- */
  float _kp; /* 比例系数 */
  float _ki; /* 积分系数 */
  float _kd; /* 微分系数 */

  /* ---- 运行状态 ---- */
  float _target;    /* 目标值 */
  float _feedback;  /* 反馈值 */
  float _error;     /* 当前误差 */

  /* ---- 积分 ---- */
  float _integral;      /* 积分累计 */
  float _integral_max;  /* 积分限幅 */

  /* ---- 输出 ---- */
  float _output;      /* PID 输出 */
  float _output_max;  /* 输出限幅 */

  /* ---- 微分 ---- */
  float _last_error;  /* 上次误差 */

  /* ---- 时间缩放 ---- */
  float _dt;          /* 采样周期 (s)，0=未设置(等效1.0) */

  /* ---- 输入范围 ---- */
  float _input_min;   /* 输入下限 */
  float _input_max;   /* 输入上限 */
  bool  _input_limited; /* 是否启用输入限幅 */
};

#endif // __PID_HPP__
