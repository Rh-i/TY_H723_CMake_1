/**
 * @file gimbal_jc.cpp
 * @author Rh
 * @brief 云台控制模块实现
 * @version 0.1
 * @date 2026-02-13
 *
 * @copyright Copyright (c) 2026
 *
 * @details 使用JC2804电机实现云台控制
 *
 * @note 坐标系统说明：
 *       电机转法逆时针
 *       (pitch电机正转为向上)
 *       ^z
 *       |   / y(front)(相机朝向)
 *       |  /
 *       | /
 *       . ————————> x(right) (yaw电机反转为向右)
 *
 *       云台控制系统中:
 *       - Yaw轴: 水平旋转轴，对应x轴方向的旋转
 *       - Pitch轴: 垂直俯仰轴，对应y轴方向的上下移动
 */

#ifndef __GIMBAL_JC_HPP__
#define __GIMBAL_JC_HPP__


#include "JC2804.hpp"


/* USER CODE BEGIN*/

/* ==================== 外部声明 ==================== */
class gimbal_jc;


/* USER CODE END */


/**
 * @brief 云台控制模式枚举
 */
enum class gimbal_position_mode_e
{
  GIMBAL_POSITION_MODE_TRAPEZOID = 2, ///< 位置梯形模式
  GIMBAL_POSITION_MODE_FILTER    = 3, ///< 位置滤波模式
  GIMBAL_POSITION_MODE_PASS      = 4  ///< 位置直通模式
};


/**
 * @brief 云台状态枚举
 */
enum class gimbal_state_e
{
  GIMBAL_MOTIONLESS = 0, ///< 云台静止
  GIMBAL_YAWING,         ///< Yaw轴转动
  GIMBAL_PITCHING,       ///< Pitch轴转动
  GIMBAL_BOTH_MOVING     ///< 两轴同时转动
};


/**
 * @brief 云台控制类
 *
 * @note 使用JC2804电机实现云台控制
 */
class gimbal_jc
{
public:
  /* ==================== 构造与析构 ==================== */

  /**
   * @brief 构造函数
   * @param yaw_motor Yaw轴电机引用
   * @param pitch_motor Pitch轴电机引用
   */
  gimbal_jc(jc2804& yaw_motor, jc2804& pitch_motor);

  /**
   * @brief 析构函数
   */
  ~gimbal_jc();


  /* ==================== 公共接口 ==================== */

  /**
   * @brief 云台初始化
   * @note 需要在FreeRTOS内核启动后调用
   */
  void init();

  /**
   * @brief 设置云台位置模式
   * @param mode 位置模式
   */
  void set_position_mode(gimbal_position_mode_e mode);

  /**
   * @brief 控制云台转动到指定角度
   * @param yaw_angle Yaw轴目标角度（度）
   * @param pitch_angle Pitch轴目标角度（度）
   */
  void set_angle(float yaw_angle, float pitch_angle);

  /**
   * @brief 控制云台Yaw轴单独转动
   * @param yaw_angle Yaw轴目标角度（度）
   */
  void set_yaw_angle(float yaw_angle);

  /**
   * @brief 控制云台Pitch轴单独转动
   * @param pitch_angle Pitch轴目标角度（度）
   */
  void set_pitch_angle(float pitch_angle);

  /**
   * @brief 增量控制云台角度
   * @param yaw_offset Yaw轴增量角度（度）
   * @param pitch_offset Pitch轴增量角度（度）
   */
  void increment_angle(float yaw_offset, float pitch_offset);

  /**
   * @brief 获取当前云台状态
   * @return 云台当前状态
   */
  gimbal_state_e get_state();

  /**
   * @brief 进入闭环模式
   */
  void enter_closed_loop();

  /**
   * @brief 设置零点
   */
  void set_zero_point();

  /**
   * @brief 获取电机数据
   * @param yaw_data Yaw电机数据引用
   * @param pitch_data Pitch电机数据引用
   */
  void get_motor_data(MotorData& yaw_data, MotorData& pitch_data);

  /**
   * @brief 停止云台运动
   */
  void stop();


private:
  /* ==================== 私有成员变量 ==================== */

  jc2804&                _yaw_motor;                                                                 ///< Yaw轴电机引用
  jc2804&                _pitch_motor;                                                               ///< Pitch轴电机引用
  gimbal_position_mode_e _control_mode        = gimbal_position_mode_e::GIMBAL_POSITION_MODE_FILTER; ///< 当前控制模式
  gimbal_state_e         _state               = gimbal_state_e::GIMBAL_MOTIONLESS;                   ///< 当前云台状态
  float                  _current_yaw_angle   = 0.0f;                                                ///< 当前Yaw角度
  float                  _current_pitch_angle = 0.0f;                                                ///< 当前Pitch角度
  float                  _target_yaw_angle    = 0.0f;                                                ///< 目标Yaw角度
  float                  _target_pitch_angle  = 0.0f;                                                ///< 目标Pitch角度


  /* ==================== 私有成员函数 ==================== */

  /**
   * @brief 更新云台状态
   */
  void update_state();
};


#endif // __GIMBAL_JC_HPP__
