/**
 * @file JC2804.hpp
 * @author Rh
 * @brief 俱瓷科技JC2804电机驱动
 * @version 0.1
 * @date 2026-02-13
 *
 * @todo 这个库写的非常简陋，很多功能没测
 *       可扩展更多电机型号支持
 *
 * @copyright Copyright (c) 2026
 *
 * @details 使用bsp_can和FreeRTOS封装，适用于电赛云台
 *
 * @note 初始化示例
 *       jc2804 motor_yaw(bsp_can1, 2);    // 创建类对象
 *       motor_yaw.init();                 // 初始化电机（在内核初始化之后使用）
 *       motor_yaw.on_can_message(rx_msg); // 在CAN接收回调处理任务中调用
 *
 * @note 控制示例
 *       motor_yaw.enter_closed_loop();    // 进入闭环模式
 *       osDelay(100);
 *       motor_yaw.set_control_mode(1);    // 进入速度模式
 *       osDelay(100);
 *       motor_yaw.set_speed(0);           // 设置速度0（防止失控）
 *       osDelay(100);
 *       motor_yaw.set_speed(100);         // 设置速度100（正常操控）
 */

#ifndef __JC2804_HPP__
#define __JC2804_HPP__


#include "bsp_cfg.hpp"


/* USER CODE BEGIN */

/* ==================== 外部声明 ==================== */

// 前向声明
class jc2804;

extern jc2804 motor_pitch; ///< Pitch轴电机实例
extern jc2804 motor_yaw;   ///< Yaw轴电机实例

/* USER CODE END */


/**
 * @brief 电机数据结构体
 */
struct MotorData
{
  float    voltage     = 0.0f; ///< 电压（V）
  float    current     = 0.0f; ///< 电流（A）
  float    speed       = 0.0f; ///< 速度（rpm）
  float    position    = 0.0f; ///< 位置（度）
  float    driver_temp = 0.0f; ///< 驱动器温度（摄氏度）
  float    motor_temp  = 0.0f; ///< 电机温度（摄氏度）
  uint32_t error_info  = 0;    ///< 错误信息
};


/**
 * @brief JC2804电机驱动类
 *
 * @note 使用bsp_can接口封装，支持多种控制模式
 */
class jc2804
{
public:
  /* ==================== 构造与析构 ==================== */

  /**
   * @brief 构造函数
   * @param can_interface CAN接口引用
   * @param device_id 设备ID
   */
  jc2804(bsp_can& can_interface, uint8_t device_id);

  /**
   * @brief 析构函数
   */
  ~jc2804();


  /* ==================== 公有成员变量 ==================== */

  uint32_t _device_id; ///< 设备ID（外部可直接访问）


  /* ==================== 控制指令 ==================== */

  /**
   * @brief 设置扭矩（电流）
   * @param torque 扭矩值
   */
  void set_torque(float torque);

  /**
   * @brief 设置速度
   * @param speed 速度值（rpm）
   */
  void set_speed(float speed);

  /**
   * @brief 设置绝对位置
   * @param position 位置值（度）
   */
  void set_absolute_position(float position);

  /**
   * @brief 设置相对位置
   * @param position 位置值（度）
   */
  void set_relative_position(float position);

  /**
   * @brief 低速模式下设置低速
   * @param speed 速度值（rpm）
   */
  void set_low_speed(float speed);

  /**
   * @brief 速度位置控制
   * @param position 位置值
   * @param speed 速度值
   */
  void pv_command(int32_t position, float speed);

  /**
   * @brief 速度位置力矩模式
   * @param position 位置值
   * @param speed 速度值
   * @param torque_percent 力矩百分比
   */
  void pvt_command(int32_t position, float speed, float torque_percent);

  /**
   * @brief 设置控制模式
   * @param mode 模式值：
   *        0-力矩模式
   *        1-速度模式
   *        2-位置梯形模式
   *        3-位置滤波模式
   *        4-位置直通模式
   *        5-低速模式
   */
  void set_control_mode(uint8_t mode);


  /* ==================== 系统控制指令 ==================== */

  /**
   * @brief 系统空闲
   */
  void idle();

  /**
   * @brief 进入闭环模式
   */
  void enter_closed_loop();

  /**
   * @brief 擦除数据
   */
  void erase();

  /**
   * @brief 保存数据
   */
  void save();

  /**
   * @brief 重启
   */
  void restart();

  /**
   * @brief 设置零点
   */
  void set_origin();

  /**
   * @brief 设置临时零点
   */
  void set_temporary_origin();


  /* ==================== 读取请求 ==================== */

  /**
   * @brief 读取电压
   */
  void request_power_voltage();

  /**
   * @brief 读取总线电流
   */
  void request_bus_current();

  /**
   * @brief 读取实时速度
   */
  void request_real_time_speed();

  /**
   * @brief 读取实时位置
   */
  void request_real_time_position();

  /**
   * @brief 读取驱动器温度
   */
  void request_driver_temperature();

  /**
   * @brief 读取电机温度
   */
  void request_motor_temperature();

  /**
   * @brief 读取错误信息
   */
  void request_error_info();


  /* ==================== 数据获取 ==================== */

  /**
   * @brief 获取最新数据
   * @return MotorData 电机数据
   */
  MotorData get_latest_data_struct();


  /* ==================== CAN回调接口 ==================== */

  /**
   * @brief CAN消息回调处理
   * @param rx_msg CAN接收消息
   */
  void on_can_message(const can_rx_msg_t& rx_msg);


private:
  /* ==================== 静态常量 ==================== */

  static const float VOLTAGE_SCALE;     ///< 电压缩放因子
  static const float CURRENT_SCALE;     ///< 电流缩放因子
  static const float SPEED_SCALE;       ///< 速度缩放因子
  static const float POSITION_SCALE;    ///< 位置缩放因子
  static const float TEMPERATURE_SCALE; ///< 温度缩放因子


  /* ==================== 接收类型枚举 ==================== */

  enum RequestType
  {
    NONE_REQUEST = 0,    ///< 无请求
    VOLTAGE_REQUEST,     ///< 电压请求
    CURRENT_REQUEST,     ///< 电流请求
    SPEED_REQUEST,       ///< 速度请求
    POSITION_REQUEST,    ///< 位置请求
    DRIVER_TEMP_REQUEST, ///< 驱动器温度请求
    MOTOR_TEMP_REQUEST,  ///< 电机温度请求
    ERROR_INFO_REQUEST   ///< 错误信息请求
  };


  /* ==================== 私有成员变量 ==================== */

  RequestType _last_request_type; ///< 上一次请求类型
  bsp_can&    _can;               ///< CAN接口引用
  MotorData   latest_data;        ///< 最新数据存储


  /* ==================== 私有成员函数 ==================== */

  /**
   * @brief 异步发送命令
   * @param cmd 命令字节
   * @param data 数据指针
   * @param len 数据长度
   */
  void send_async_command(uint8_t cmd, const uint8_t* data, uint8_t len);

  /**
   * @brief 发送读取请求并记录请求类型
   * @param cmd 命令
   * @param reg_addr 寄存器地址
   * @param req_type 请求类型
   */
  void send_read_request(uint8_t cmd, uint16_t reg_addr, RequestType req_type);

  /**
   * @brief 响应验证
   * @param expected_cmd 期望命令
   * @param rx_msg 接收消息引用
   * @return 验证结果
   */
  bool validate_response(uint8_t expected_cmd, const can_rx_msg_t& rx_msg);

  /**
   * @brief 解析接收数据
   * @param data 数据数组引用
   */
  void store_received_data(const uint8_t (&data)[8]);
};


#endif // __JC2804_HPP__
