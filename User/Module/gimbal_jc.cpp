#include "cmsis_os2.h"
#include "gimbal_jc.hpp"


/* USER CODE BEGIN */

// 全局实例化云台类

/* USER CODE END */


/* ==================== 构造函数与析构函数 ==================== */

/**
 * @brief 构造函数
 * @param yaw_motor Yaw轴电机指针
 * @param pitch_motor Pitch轴电机指针
 */
gimbal_jc::gimbal_jc(jc2804& yaw_motor, jc2804& pitch_motor)

  : _yaw_motor(yaw_motor),
    _pitch_motor(pitch_motor)
{
}


/**
 * @brief 析构函数
 */
gimbal_jc::~gimbal_jc()
{
}


/* ==================== 公共接口实现 ==================== */

/**
 * @brief 云台初始化
 */
void gimbal_jc::init()
{
  /* 进入闭环模式 */
  enter_closed_loop();

  /* 设置初始控制模式为位置平滑模式 */
  set_position_mode(gimbal_position_mode_e::GIMBAL_POSITION_MODE_FILTER);

  /* 初始时设置速度为0，防止电机失控 */
  _yaw_motor.set_speed(0.0f);
  _pitch_motor.set_speed(0.0f);

  /* 更新状态 */
  _state = gimbal_state_e::GIMBAL_MOTIONLESS;
}


/**
 * @brief 设置云台位置模式
 * @param mode 位置模式
 */
void gimbal_jc::set_position_mode(gimbal_position_mode_e mode)
{
  _control_mode = mode;
  osDelay(10);
  _yaw_motor.set_control_mode(static_cast<uint8_t>(mode));
  osDelay(10);
  _pitch_motor.set_control_mode(static_cast<uint8_t>(mode));
}


/**
 * @brief 控制云台转动到指定角度
 * @param yaw_angle Yaw轴目标角度（度）
 * @param pitch_angle Pitch轴目标角度（度）
 */
void gimbal_jc::set_angle(float yaw_angle, float pitch_angle)
{
  _target_yaw_angle   = yaw_angle;
  _target_pitch_angle = pitch_angle;

  /* 根据当前控制模式发送相应的位置指令 */
  switch (_control_mode)
  {
    case gimbal_position_mode_e::GIMBAL_POSITION_MODE_TRAPEZOID:
    case gimbal_position_mode_e::GIMBAL_POSITION_MODE_FILTER:
    case gimbal_position_mode_e::GIMBAL_POSITION_MODE_PASS:
    default:
      _yaw_motor.set_absolute_position(yaw_angle);
      _pitch_motor.set_absolute_position(pitch_angle);
      break;
  }

  update_state();
}


/**
 * @brief 控制云台Yaw轴单独转动
 * @param yaw_angle Yaw轴目标角度（度）
 */
void gimbal_jc::set_yaw_angle(float yaw_angle)
{
  _target_yaw_angle = yaw_angle;

  switch (_control_mode)
  {
    case gimbal_position_mode_e::GIMBAL_POSITION_MODE_TRAPEZOID:
    case gimbal_position_mode_e::GIMBAL_POSITION_MODE_FILTER:
    case gimbal_position_mode_e::GIMBAL_POSITION_MODE_PASS:
    default:
      _yaw_motor.set_absolute_position(yaw_angle);
      break;
  }

  update_state();
}


/**
 * @brief 控制云台Pitch轴单独转动
 * @param pitch_angle Pitch轴目标角度（度）
 */
void gimbal_jc::set_pitch_angle(float pitch_angle)
{
  _target_pitch_angle = pitch_angle;

  switch (_control_mode)
  {
    case gimbal_position_mode_e::GIMBAL_POSITION_MODE_TRAPEZOID:
    case gimbal_position_mode_e::GIMBAL_POSITION_MODE_FILTER:
    case gimbal_position_mode_e::GIMBAL_POSITION_MODE_PASS:
    default:
      _pitch_motor.set_absolute_position(pitch_angle);
      break;
  }

  update_state();
}


/**
 * @brief 增量控制云台角度
 * @param yaw_offset Yaw轴增量角度（度）
 * @param pitch_offset Pitch轴增量角度（度）
 */
void gimbal_jc::increment_angle(float yaw_offset, float pitch_offset)
{
  float new_yaw_angle   = _current_yaw_angle + yaw_offset;
  float new_pitch_angle = _current_pitch_angle + pitch_offset;

  set_angle(new_yaw_angle, new_pitch_angle);
}


/**
 * @brief 进入闭环模式
 */
void gimbal_jc::enter_closed_loop()
{
  _yaw_motor.enter_closed_loop();
  osDelay(10); // 等待电机响应
  _pitch_motor.enter_closed_loop();
  osDelay(10); // 等待电机响应
}


/**
 * @brief 设置零点
 */
void gimbal_jc::set_zero_point()
{
  _yaw_motor.set_origin();
  _pitch_motor.set_origin();
  osDelay(100); // 等待电机响应
}


/**
 * @brief 获取当前云台状态
 * @return 云台当前状态
 */
gimbal_state_e gimbal_jc::get_state()
{
  return _state;
}


/**
 * @brief 获取电机数据
 * @param yaw_data Yaw电机数据引用
 * @param pitch_data Pitch电机数据引用
 */
void gimbal_jc::get_motor_data(MotorData& yaw_data, MotorData& pitch_data)
{
  yaw_data   = _yaw_motor.get_latest_data_struct();
  pitch_data = _pitch_motor.get_latest_data_struct();
}


/**
 * @brief 停止云台运动
 */
void gimbal_jc::stop()
{
  _yaw_motor.set_speed(0.0f);
  _pitch_motor.set_speed(0.0f);
  _state = gimbal_state_e::GIMBAL_MOTIONLESS;
}


/**
 * @brief 更新云台状态
 */
void gimbal_jc::update_state()
{
  bool yaw_moving   = (_target_yaw_angle != _current_yaw_angle);
  bool pitch_moving = (_target_pitch_angle != _current_pitch_angle);

  if (yaw_moving && pitch_moving)
  {
    _state = gimbal_state_e::GIMBAL_BOTH_MOVING;
  }
  else if (yaw_moving)
  {
    _state = gimbal_state_e::GIMBAL_YAWING;
  }
  else if (pitch_moving)
  {
    _state = gimbal_state_e::GIMBAL_PITCHING;
  }
  else
  {
    _state = gimbal_state_e::GIMBAL_MOTIONLESS;
  }

  /* 更新当前位置 */
  _current_yaw_angle   = _target_yaw_angle;
  _current_pitch_angle = _target_pitch_angle;
}
