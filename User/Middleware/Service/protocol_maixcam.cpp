#include "protocol_maixcam.hpp"
#include "protocol_usart.hpp"
#include <stdio.h>
#include <string.h>

/* USER CODE BEGIN */

/* ==================== 全局实例化 ==================== */

/**
 * @brief 全局MaixCam协议实例
 */
protocol_maixcam maixcam(&protocal_usart_1);

/* USER CODE END */


/* ==================== 构造函数与析构函数 ==================== */

/**
 * @brief 构造函数
 * @param protocol_uart 串口协议实例指针
 */
protocol_maixcam::protocol_maixcam(protocol_usart* protocol_uart) : _protocol_uart(protocol_uart)
{
  memset(&_data, 0, sizeof(_data));
}


/**
 * @brief 析构函数
 */
protocol_maixcam::~protocol_maixcam()
{
}


/* ==================== 公共接口实现 ==================== */

/**
 * @brief 初始化MaixCam协议
 */
void protocol_maixcam::init()
{
  /* 初始化数据存储区 */
  memset(&_data, 0, sizeof(_data));
}


/**
 * @brief 解析接收到的数据帧
 * @param cmd 命令码
 * @param data 数据指针
 * @param len 数据长度
 */
void protocol_maixcam::parse(uint8_t cmd, uint8_t* data, uint8_t len)
{
  switch (cmd)
  {
    case MAIXCAM_CMD_HEARTBEAT: /* 0x01 */
      parse_heartbeat(data, len);
      break;

    case MAIXCAM_CMD_OFFSET_DATA: /* 0x10 */
      parse_offset_data(data, len);
      break;

    case MAIXCAM_CMD_IMU_DATA: /* 0x11 */
      parse_imu_data(data, len);
      break;

    case MAIXCAM_CMD_STATUS_REPORT: /* 0x12 */
      parse_status_report(data, len);
      break;

    default:
      printf("[MaixCam] Unknown CMD: 0x%02X\n", cmd);
      break;
  }
}


/**
 * @brief 发送开始自瞄命令给视觉模块
 * @note CMD = 0x20, 帧格式: AA 55 20 00 SUM 0C
 */
void protocol_maixcam::send_start_aim()
{
  uint8_t dummy = 0;
  _protocol_uart->send(MAIXCAM_CMD_START_AIM, &dummy, 0);
}


/**
 * @brief 发送停止自瞄命令给视觉模块
 * @note CMD = 0x21, 帧格式: AA 55 21 00 SUM 0C
 */
void protocol_maixcam::send_stop_aim()
{
  uint8_t dummy = 0;
  _protocol_uart->send(MAIXCAM_CMD_STOP_AIM, &dummy, 0);
}


/**
 * @brief 发送校准IMU命令给视觉模块
 * @note CMD = 0x22, 帧格式: AA 55 22 00 SUM 0C
 */
void protocol_maixcam::send_calibrate_imu()
{
  uint8_t dummy = 0;
  _protocol_uart->send(MAIXCAM_CMD_CALIBRATE_IMU, &dummy, 0);
}


/* ==================== 私有解析函数实现 ==================== */

/**
 * @brief 解析偏移量数据 (CMD = 0x10)
 *
 * @note 数据格式：小端序int16_t，放大100倍
 *       frame[0:1] = offset_x (int16)
 *       frame[2:3] = offset_y (int16)
 */
void protocol_maixcam::parse_offset_data(uint8_t* data, uint8_t len)
{
  if (len < 4)
  {
    return;
  }

  /* 解析int16_t小端序数据 */
  int16_t offset_x_raw = (int16_t)(data[0] | (data[1] << 8));
  int16_t offset_y_raw = (int16_t)(data[2] | (data[3] << 8));

  /* 转换为实际角度（数据放大100倍） */
  _data.offset_x = offset_x_raw / 100.0f;
  _data.offset_y = offset_y_raw / 100.0f;
}


/**
 * @brief 解析IMU数据 (CMD = 0x11)
 *
 * @note 数据格式：小端序int16_t，放大100倍
 *       frame[0:1] = pitch (int16)
 *       frame[2:3] = roll (int16)
 *       frame[4:5] = yaw (int16)
 */
void protocol_maixcam::parse_imu_data(uint8_t* data, uint8_t len)
{
  if (len < 6 || data == nullptr)
  {
    return;
  }

  /* 解析int16_t小端序数据 */
  int16_t pitch_raw = (int16_t)(data[0] | (data[1] << 8));
  int16_t roll_raw  = (int16_t)(data[2] | (data[3] << 8));
  int16_t yaw_raw   = (int16_t)(data[4] | (data[5] << 8));

  /* 转换为实际角度（数据放大100倍） */
  _data.imu_pitch = pitch_raw / 100.0f;
  _data.imu_roll  = roll_raw / 100.0f;
  _data.imu_yaw   = yaw_raw / 100.0f;

  /* 移除printf以提高处理速度 - 如需调试可重新启用 */
  /* printf("[MaixCam] IMU: Pitch=%.2f deg, Roll=%.2f deg, Yaw=%.2f deg\n", */
  /*        _data.imu_pitch, */
  /*        _data.imu_roll, */
  /*        _data.imu_yaw); */
}


/**
 * @brief 解析状态报告 (CMD = 0x12)
 *
 * @note 数据格式：
 *       frame[0] = status
 *       frame[1] = flags
 *         bit0: calibrated
 *         bit1: aiming
 */
void protocol_maixcam::parse_status_report(uint8_t* data, uint8_t len)
{
  if (len < 2 || data == nullptr)
  {
    return;
  }

  /* 解析状态 */
  _data.status = (maixcam_status_e)data[0];

  /* 解析标志位 */
  _data.flags.calibrated = (data[1] >> 0) & 0x01;
  _data.flags.aiming     = (data[1] >> 1) & 0x01;

  /* 移除printf以提高处理速度 - 如需调试可重新启用 */
  /* printf("[MaixCam] Status: %s, Calibrated=%d, Aiming=%d\n", */
  /*        status_str, */
  /*        _data.flags.calibrated, */
  /*        _data.flags.aiming); */
}


/**
 * @brief 解析心跳包 (CMD = 0x01)
 *
 * @note 数据格式：
 *       frame[0] = 固定0x01
 */
void protocol_maixcam::parse_heartbeat(uint8_t* data, uint8_t len)
{
  (void)data;
  if (len < 1)
  {
    return;
  }

  /* 更新心跳状态 */
  _data.heartbeat_received  = true;
  _data.last_heartbeat_time = osKernelGetTickCount();

  /* 移除printf以提高处理速度 - 如需调试可重新启用 */
  /* printf("[MaixCam] Heartbeat received\n"); */
}
