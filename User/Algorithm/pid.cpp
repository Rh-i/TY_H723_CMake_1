/**
 * @file pid.cpp
 * @brief 位置式PID控制器 — 实现
 *
 * @note 位置式 PID 公式（带 dt 时间缩放）:
 *   e(k)     = target - feedback
 *   p_out    = Kp × e(k)
 *   d_out    = Kd × (e(k) - e(k-1)) / dt    （dt 缩放）
 *   I 条件累加: 输出未饱和 或 误差在退饱和方向 时才累加
 *   integral += Ki × e(k) × dt              （dt 缩放 + 抗饱和）
 *   output   = p_out + integral + d_out     （带限幅）
 */

#include "pid.hpp"
#include <math.h>

/* ==================== 构造 / 析构 ==================== */

/**
 * @brief 默认构造函数 — 初始化为零参数
 */
Pid::Pid()
{
  init(0, 0, 0, 0, 0, 0, 0, 0);
}

/* ==================== 初始化 ==================== */

/**
 * @brief PID 全参数初始化（8 参数，最终实现）
 */
void Pid::init(float kp, float ki, float kd, float out_max, float i_max, float dt, float input_min, float input_max)
{
  /* 系数 */
  _kp = kp;
  _ki = ki;
  _kd = kd;

  /* 限幅 */
  _output_max   = out_max;
  _integral_max = i_max;

  /* dt */
  _dt = (dt > 1e-9f) ? dt : 0;

  /* 输入范围 */
  if (input_min < input_max)
  {
    _input_min     = input_min;
    _input_max     = input_max;
    _input_limited = true;
  }
  else
  {
    _input_min     = 0;
    _input_max     = 0;
    _input_limited = false;
  }

  /* 状态清零 */
  _target     = 0;
  _feedback   = 0;
  _error      = 0;
  _integral   = 0;
  _output     = 0;
  _last_error = 0;
}


/* ==================== 输入校验 ==================== */

/**
 * @brief 输入值合法性校验 + 钳位
 *
 * 处理以下异常:
 * - NaN / Inf → 返回 0（安全默认值）
 * - 超出 [_input_min, _input_max] → 钳位到边界
 * - 未启用输入限幅 → 原值返回
 *
 * @param value 原始输入值
 * @return 校验后的安全值
 */
float Pid::clamp_input(float value) const
{
  /* 非法浮点值（NaN, Inf）→ 安全回退 */
  if (!isfinite(value))
  {
    return 0;
  }

  /* 输入范围钳位 */
  if (_input_limited)
  {
    if (value > _input_max)
      return _input_max;
    if (value < _input_min)
      return _input_min;
  }

  return value;
}

/* ==================== 计算 ==================== */

/**
 * @brief PID 计算（位置式，带抗饱和 + dt 缩放 + 输入校验）
 *
 * @param target   目标值
 * @param feedback 反馈值
 * @return PID 输出值（已限幅到 [-output_max, +output_max]）
 *
 * @note 抗饱和策略（条件积分法）:
 *       - 先计算 P + D + 旧积分，得试探输出
 *       - 若试探输出超出限幅上限 且 误差 > 0（正误差会把输出推得更高）→ 不累加积分
 *       - 若试探输出超出限幅下限 且 误差 < 0（负误差会把输出推得更低）→ 不累加积分
 *       - 其他情况（输出在线性区，或误差在退饱和方向）→ 正常累加积分
 *
 * @note dt 缩放:
 *       - I 项: Ki × error × dt  （未设 dt 时 dt=1.0）
 *       - D 项: Kd × (error - last_error) / dt
 */
float Pid::calculate(float target, float feedback)
{
  /* ---- 输入校验 ---- */
  _target   = clamp_input(target);
  _feedback = clamp_input(feedback);

  /* ---- 误差 ---- */
  _error = _target - _feedback;

  /* ---- dt 确定 ---- */
  float dt = (_dt > 1e-9f) ? _dt : 1.0f;

  /* ---- 比例项 ---- */
  float p_out = _kp * _error;

  /* ---- 微分项（dt 缩放）---- */
  float d_out = _kd * (_error - _last_error) / dt;
  _last_error = _error;

  /* ================================================================
   * 抗饱和：条件积分法（积分遇限停止累加）
   *
   * 先以当前积分值合成试探输出，判断是否已饱和。
   * 仅在下列情况累加积分:
   *   1. 试探输出未超出限幅（线性区）
   *   2. 误差方向与饱和方向相反（误差在把输出拉回线性区）
   *
   * 若输出已达上限且误差仍为正（推高输出），停止累加。
   * 若输出已达下限且误差仍为负（压低输出），停止累加。
   * ================================================================ */
  float pre_output = p_out + _integral + d_out;

  bool sat_high = (pre_output > _output_max);  /* 试探输出触及上限 */
  bool sat_low  = (pre_output < -_output_max); /* 试探输出触及下限 */

  /* 仅当不在"同向饱和"状态时才累加积分 */
  bool windup_high = (sat_high && _error > 0); /* 饱和上限 + 正误差 → 停积 */
  bool windup_low  = (sat_low && _error < 0);  /* 饱和下限 + 负误差 → 停积 */

  if (!windup_high && !windup_low)
  {
    _integral += _ki * _error * dt; /* dt 缩放积分 */
  }

  /* ---- 积分限幅（在条件累加之后做安全钳位）---- */
  if (_integral > _integral_max)
    _integral = _integral_max;
  if (_integral < -_integral_max)
    _integral = -_integral_max;

  /* ---- 输出合成 ---- */
  _output = p_out + _integral + d_out;

  /* ---- 输出限幅 ---- */
  if (_output > _output_max)
    _output = _output_max;
  if (_output < -_output_max)
    _output = -_output_max;

  return _output;
}

/* ==================== 重置 ==================== */

/**
 * @brief PID 重置
 *
 * @note 清零积分累计和微分历史，保留系数、限幅、dt 和输入范围不变
 */
void Pid::reset()
{
  _error      = 0;
  _integral   = 0;
  _output     = 0;
  _last_error = 0;
}
