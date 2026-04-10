#include "pid.hpp"
#include <math.h>

// @Choose-B 这俩是hyw，最好别用cpp的库，嵌入式遭不住
// #include <functional>
// #include <numeric>

void PID_t::Calc_Input(float target, float feedback)
{
  input.last_target  = input.target;
  input.last_error   = input.error;
  input.error        = input.target - input.feedback;
  input.delta_target = input.target - input.last_target;
  input.delta_error  = input.error - input.last_error;
}

_PID_Param_t::_PID_Param_t() : kp(0), ki(0), kd(0)
{
}
_PID_Param_t::_PID_Param_t(float kp, float ki, float kd) : kp(kp), ki(ki), kd(kd)
{
}
_PID_Limitation_t::_PID_Limitation_t() : Out_max(0), P_max(0), I_max(0), D_max(0), F_max(0), I_separation(0)
{
}
_PID_Limitation_t::_PID_Limitation_t(float Out_max, float P_max, float I_max, float D_max, float F_max, float I_separation) : Out_max(Out_max), P_max(P_max), I_max(I_max), D_max(D_max), F_max(F_max), I_separation(I_separation)
{
  this->I_max = I_max == 0 ? Out_max : I_max;
}
_PID_Term_t::_PID_Term_t() : P_term(0), I_term(0), D_term(0), F_term(0)
{
}

PID_t::PID_t(_PID_Param_t param, _PID_Limitation_t limitation) : param(param), lim(limitation), term()
{
  diffCalcMode = param.kd == 0 ? Diff_target : Disable_PID_Diff;
}
PID_t::PID_t(_PID_Param_t param) : param(param), lim(), term()
{
  diffCalcMode = param.kd == 0 ? Diff_target : Disable_PID_Diff;
}
PID_t::PID_t(float kp, float ki, float kd) : param(kp, ki, kd), lim(), term()
{
  diffCalcMode = param.kd == 0 ? Diff_target : Disable_PID_Diff;
}
PID_t::PID_t(float kp, float ki, float kd, float Out_max, float P_max, float I_max, float D_max, float F_max, float I_separation) : param(kp, ki, kd), lim(Out_max, P_max, I_max, D_max, F_max, I_separation), term()
{
  diffCalcMode = param.kd == 0 ? Diff_target : Disable_PID_Diff;
}

void PID_t::SwitchMode_DiffCalc(_PID_Diff_Calc_mode_t mode)
{
  diffCalcMode = mode;
}

float PID_t::FeedForward(float feedforward)
{
  term.F_term += feedforward;
  return term.F_term;
}

float PID_t::Calc(float target, float feedback)
{
  Calc_Input(target, feedback);

  term.P_term = param.kp * input.error;
  // 积分分离阈值为0时禁用积分分离
  term.I_term = fabsf(input.error) < lim.I_separation || lim.I_separation == 0 ? term.I_term + param.ki * input.error : 0;
  switch (diffCalcMode)
  {
    case Diff_target:
    {
      term.D_term = param.kd * input.delta_target; // 微分先行
      break;
    }
    case Diff_error:
    {
      term.D_term = param.kd * input.delta_error; // 常规微分
      break;
    }
    case Disable_PID_Diff:
    {
      term.D_term = 0;
      break;
    }
  }

  // 各项限幅
  term.P_term = term.P_term > lim.P_max ? lim.P_max : term.P_term;
  term.P_term = term.P_term < -lim.P_max ? -lim.P_max : term.P_term;
  term.I_term = term.I_term > lim.I_max ? lim.I_max : term.I_term;
  term.I_term = term.I_term < -lim.I_max ? -lim.I_max : term.I_term;
  term.D_term = term.D_term > lim.D_max ? lim.D_max : term.D_term;
  term.D_term = term.D_term < -lim.D_max ? -lim.D_max : term.D_term;
  term.F_term = term.F_term > lim.F_max ? lim.F_max : term.F_term;
  term.F_term = term.F_term < -lim.F_max ? -lim.F_max : term.F_term;

  // 计算输出
  output = term.P_term + term.I_term + term.D_term + term.F_term;
  output = output > lim.Out_max ? lim.Out_max : output;
  output = output < -lim.Out_max ? -lim.Out_max : output;

  // 清零前馈项，准备下一次累加
  term.F_term = 0;
  return output;
}

float PID_t::Calc(float target, float feedback, float df_dt)
{
  Calc_Input(target, feedback);

  term.P_term = param.kp * input.error;
  // 微分分离阈值为0时禁用微分分离
  term.I_term = fabsf(input.error) < lim.I_separation || lim.I_separation == 0 ? term.I_term + param.ki * input.error : 0;
  switch (diffCalcMode)
  {
    case Diff_target:
    {
      input.delta_target = df_dt;
      term.D_term        = param.kd * input.delta_target; // 微分先行
      break;
    }
    case Diff_error:
    {
      input.delta_error = df_dt;
      term.D_term       = param.kd * input.delta_error; // 常规微分
      break;
    }
    case Disable_PID_Diff:
    {
      term.D_term = 0;
      break;
    }
  }

  // 各项限幅
  term.P_term = term.P_term > lim.P_max ? lim.P_max : term.P_term;
  term.P_term = term.P_term < -lim.P_max ? -lim.P_max : term.P_term;
  term.I_term = term.I_term > lim.I_max ? lim.I_max : term.I_term;
  term.I_term = term.I_term < -lim.I_max ? -lim.I_max : term.I_term;
  term.D_term = term.D_term > lim.D_max ? lim.D_max : term.D_term;
  term.D_term = term.D_term < -lim.D_max ? -lim.D_max : term.D_term;
  term.F_term = term.F_term > lim.F_max ? lim.F_max : term.F_term;
  term.F_term = term.F_term < -lim.F_max ? -lim.F_max : term.F_term;

  // 计算输出
  output = term.P_term + term.I_term + term.D_term + term.F_term;
  output = output > lim.Out_max ? lim.Out_max : output;
  output = output < -lim.Out_max ? -lim.Out_max : output;

  // 清零前馈项，准备下一次累加
  term.F_term = 0;
  return output;
}

void PID_t::Print()
{
  // 暂未实现
  return;
}
