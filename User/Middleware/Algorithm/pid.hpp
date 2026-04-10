/**
 * @file pid.cpp
 * @author ChoseB (ChoseB@cumt.edu.cn)
 * @brief PID算法的封装
 * @version 0.1
 * @date 2026-03-07
 *
 * @copyright Copyright (c) 2026
 *
 * @brief 使用示例
 *
 * @note 实例化(以下初始化方式任选其一即可)
 *
 *      // option 1
 *      PID_t motor1PID(0.8,0,0);                                        // 只给kp,ki,kd
 *
 *      // option 2
 *      PID_t motor2PID(0.8,1,1,10000,0,3000,0,8000,50);                 // 给全参数
 *
 *      // option 3
 *      PID_t PID_temp(0.8,1,1,10000,0,3000,0,8000,50);
 *      PID_t motorsPID[4] = {PID_temp,PID_temp,PID_temp,PID_temp};      // 复制其他的pid对象
 *
 *      // option 4
 *      PID_t motor3PID(0.8,1,1,10000,0,3000,0,8000,50);
 *      motor3.PID.SwitchMode_DiffCalc(Diff_error);                      // optional, 有必要才禁用微分先行
 *
 * @note 使用(假设已有PID_t对象pid)
 *
 *      pid.FeedForward(FrictionFF());           // optional
 *      pid.FeedForward(FollowFF(dTarget));      // optional
 *      pid.Calc(tar,motor.angle,motor.speed);   // 最后一个参数是可去掉的
 */


#ifndef __PID_HPP__
#define __PID_HPP__

struct _PID_Param_t
{
  float kp; // 比例项系数
  float ki; // 积分项系数
  float kd; // 微分项系数
  _PID_Param_t();
  _PID_Param_t(float kp, float ki, float kd);
};

struct _PID_Limitation_t
{
  float Out_max;      // 总输出最大限制
  float P_max;        // 比例项最大限制，写0则关闭
  float I_max;        // 积分项最大限制，写0则采用总最大输出
  float D_max;        // 微分项最大限制，写0则关闭
  float F_max;        // 前馈项最大限制，写0则关闭
  float I_separation; // 积分分离阈值
  _PID_Limitation_t();
  _PID_Limitation_t(float Out_max, float P_max, float I_max, float D_max, float F_max, float I_separation);
};

struct _PID_Term_t
{
  float P_term; // 比例项计算值
  float I_term; // 积分项计算值
  float D_term; // 微分项计算值
  float F_term; // 前馈项计算值
  _PID_Term_t();
};

struct _PID_Input_t
{
  float target;   // 当前目标值
  float feedback; // 当前反馈值
  float error;    // 当前误差值，error = target - feedback

  float last_target; // 上一目标值
  float last_error;  // 上一误差值

  float delta_target; // 目标值微分，delta_target = target - last_target，也支持直接传入目标值微分
  float delta_error;  // 误差值微分，delta_target = error - last_error，也支持直接传入误差值微分
};

enum _PID_Diff_Calc_mode_t
{
  Diff_target      = 0x01, // 使用delta_target来计算微分项
  Diff_error       = 0x02, // 使用delta_target来计算微分项
  Disable_PID_Diff = 0x00, // 不计算微分项目
};

class PID_t
{
protected:
  _PID_Param_t      param; // 系数
  _PID_Limitation_t lim;   // 限幅
  _PID_Term_t       term;  // 各项计算值

  _PID_Diff_Calc_mode_t diffCalcMode; // 微分项计算模式

  virtual void Calc_Input(float target, float feedback); // 根据传入计算出其他输入项

public:
  _PID_Input_t input;  // 传入
  float        output; // 传出

  PID_t(_PID_Param_t param);
  PID_t(float kp, float ki, float kd);
  PID_t(_PID_Param_t param, _PID_Limitation_t limitation);
  PID_t(float kp, float ki, float kd, float Out_max, float P_max, float I_max, float D_max, float F_max, float I_separation);

  /**
   * @brief 切换微分项计算方式。通常在不适用于微分先行的动态系统中补充初始化
   *
   * @param mode 微分项计算方式。使用计算模式Diff_error来禁用微分先行
   */
  void SwitchMode_DiffCalc(_PID_Diff_Calc_mode_t mode);

  /**
   * @brief 将某一前馈函数预测的值引入计算
   *
   * @param feedforward 某一前馈函数返回的数值
   * @return float 前馈项总和
   */
  float FeedForward(float feedforward);

  /**
   * @brief pid计算函数
   *
   * @param target 目标值
   * @param feedback 反馈值
   * @return float 输出
   */
  float Calc(float target, float feedback);
  /**
   * @brief pid计算函数，目标值一阶导数自行导入
   *
   * @param target 目标值
   * @param feedback 反馈值
   * @param df_dt 目标值一阶导数
   * @note 示例:
   * 当动态系统被控量为位移x时，df_dt项可填入速度v，以提高微分项计算精度
   * @return float 输出
   */
  float Calc(float target, float feedback, float df_dt);

  /**
   * @brief 快速打印target和feedback到vofa [暂未实现]
   *
   */
  void Print();
};

#endif // __PID_HPP__