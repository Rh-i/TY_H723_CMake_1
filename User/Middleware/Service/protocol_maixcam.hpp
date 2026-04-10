/**
 * @file protocol_maixcam.hpp
 * @brief MaixCam视觉模块串口通信协议头文件
 * @version 0.1
 * @date 2026-03-23
 *
 * @copyright Copyright (c) 2026
 *
 * @details 基于MaixCam2硬件的云台瞄准系统串口协议
 *
 * @note 帧格式：AA 55 CMD LEN DATA... SUM 0C
 *
 * @note 接收命令（上位机 -> 板端）：
 *       - CMD 0x20: 开始自瞄
 *       - CMD 0x21: 停止自瞄
 *       - CMD 0x22: 触发校准
 *
 * @note 发送命令（板端 -> 上位机）：
 *       - CMD 0x01: 心跳包
 *       - CMD 0x10: 偏移量数据
 *       - CMD 0x11: 陀螺仪数据
 *       - CMD 0x12: 设备状态报告
 *
 * @note 使用示例：
 *       - 在外部声明：protocol_maixcam maixcam(&protocal_usart_9);
 *       - 初始化：maixcam.init();
 *       - 发送命令：maixcam.send_start_aim();
 *       - 获取数据：maixcam_data_t& data = maixcam.get_data();
 */

#ifndef __PROTOCOL_MAIXCAM_HPP__
#define __PROTOCOL_MAIXCAM_HPP__


#include "stdbool.h"
#include "stddef.h"
#include "stdint.h"


/* ==================== 前向声明 ==================== */

class protocol_usart;


/* ==================== MaixCam协议命令码定义 ==================== */

/**
 * @brief 发送命令（板端 -> 上位机/视觉模块）
 */
#define MAIXCAM_CMD_HEARTBEAT 0x01     ///< 心跳包
#define MAIXCAM_CMD_OFFSET_DATA 0x10   ///< 偏移量数据（角度值）
#define MAIXCAM_CMD_IMU_DATA 0x11      ///< 陀螺仪数据
#define MAIXCAM_CMD_STATUS_REPORT 0x12 ///< 设备状态报告

/**
 * @brief 接收命令（上位机/视觉模块 -> 板端）
 */
#define MAIXCAM_CMD_START_AIM 0x20     ///< 开始自瞄
#define MAIXCAM_CMD_STOP_AIM 0x21      ///< 停止自瞄
#define MAIXCAM_CMD_CALIBRATE_IMU 0x22 ///< 触发校准


/* ==================== 状态定义 ==================== */

/**
 * @brief 视觉模块状态枚举
 */
typedef enum maixcam_status_e
{
  MAIXCAM_STATUS_INIT        = 0x00, ///< 初始化状态
  MAIXCAM_STATUS_CALIBRATING = 0x01, ///< 校准中
  MAIXCAM_STATUS_READY       = 0x02, ///< 就绪（校准完成）
  MAIXCAM_STATUS_AIMING      = 0x03, ///< 自瞄中
  MAIXCAM_STATUS_ERROR       = 0x04  ///< 错误状态
} maixcam_status_e;


/**
 * @brief 视觉模块标志位结构体
 */
typedef struct maixcam_flags_t
{
  uint8_t calibrated : 1; ///< 陀螺仪校准状态（1=已校准）
  uint8_t aiming : 1;     ///< 自瞄状态（1=开启）
  uint8_t reserved : 6;   ///< 保留位
} maixcam_flags_t;


/**
 * @brief 视觉模块数据结构体（存储接收到的数据）
 */
typedef struct maixcam_data_t
{
  /* 心跳包 */
  bool     heartbeat_received;  ///< 是否收到心跳
  uint32_t last_heartbeat_time; ///< 上次心跳时间（Tick）

  /* 偏移量数据 */
  float offset_x; ///< X轴偏移角度（度）
  float offset_y; ///< Y轴偏移角度（度）

  /* IMU数据 */
  float imu_pitch; ///< 俯仰角（度）
  float imu_roll;  ///< 横滚角（度）
  float imu_yaw;   ///< 偏航角（度）

  /* 状态报告 */
  maixcam_status_e status; ///< 设备状态
  maixcam_flags_t  flags;  ///< 标志位
} maixcam_data_t;


/* ==================== MaixCam协议类 ==================== */

/**
 * @brief MaixCam视觉模块协议类
 *
 * @note 封装串口协议处理，所有逻辑在类内部实现
 */
class protocol_maixcam
{
private:
  /* ==================== 私有成员变量 ==================== */

  protocol_usart* _protocol_uart; ///< 串口协议实例指针
  maixcam_data_t  _data;          ///< 视觉模块数据存储

  /* ==================== 私有成员函数 ==================== */

  /**
   * @brief 解析偏移量数据 (CMD = 0x10)
   * @param data 数据指针
   * @param len 数据长度
   */
  void parse_offset_data(uint8_t* data, uint8_t len);

  /**
   * @brief 解析IMU数据 (CMD = 0x11)
   * @param data 数据指针
   * @param len 数据长度
   */
  void parse_imu_data(uint8_t* data, uint8_t len);

  /**
   * @brief 解析状态报告 (CMD = 0x12)
   * @param data 数据指针
   * @param len 数据长度
   */
  void parse_status_report(uint8_t* data, uint8_t len);

  /**
   * @brief 解析心跳包 (CMD = 0x01)
   * @param data 数据指针
   * @param len 数据长度
   */
  void parse_heartbeat(uint8_t* data, uint8_t len);


public:
  /* ==================== 构造函数与析构函数 ==================== */

  /**
   * @brief 构造函数
   * @param protocol_uart 串口协议实例指针
   */
  explicit protocol_maixcam(protocol_usart* protocol_uart);

  /**
   * @brief 析构函数
   */
  ~protocol_maixcam();


  /* ==================== 公共接口 ==================== */

  /**
   * @brief 初始化MaixCam协议
   */
  void init();

  /**
   * @brief 解析接收到的数据帧
   * @note 由外部协议层调用
   *
   * @param cmd 命令码
   * @param data 数据指针
   * @param len 数据长度
   */
  void parse(uint8_t cmd, uint8_t* data, uint8_t len);

  /**
   * @brief 发送开始自瞄命令给视觉模块
   * @note CMD = 0x20, 帧格式: AA 55 20 00 SUM 0C
   */
  void send_start_aim();

  /**
   * @brief 发送停止自瞄命令给视觉模块
   * @note CMD = 0x21, 帧格式: AA 55 21 00 SUM 0C
   */
  void send_stop_aim();

  /**
   * @brief 发送校准IMU命令给视觉模块
   * @note CMD = 0x22, 帧格式: AA 55 22 00 SUM 0C
   */
  void send_calibrate_imu();

  /**
   * @brief 获取视觉模块数据引用
   * @return maixcam_data_t& 数据引用
   */
  maixcam_data_t& get_data()
  {
    return _data;
  }
};


/* ==================== 外部声明 ==================== */

/**
 * @brief 全局MaixCam协议实例
 */
extern protocol_maixcam maixcam;


#endif // __PROTOCOL_MAIXCAM_HPP__
