#include "JC2804.hpp"


/* USER CODE BEGIN */

/* ==================== 全局类成员实例化 ==================== */

JC2804 motor_yaw(bsp_can1, 2);
JC2804 motor_pitch(bsp_can1, 1);

/* USER CODE END */


/* ==================== 静态成员赋值 ==================== */

const float JC2804::VOLTAGE_SCALE     = 0.1f;
const float JC2804::CURRENT_SCALE     = 0.01f;
const float JC2804::SPEED_SCALE       = 0.01f;
const float JC2804::POSITION_SCALE    = 0.01f;
const float JC2804::TEMPERATURE_SCALE = 0.1f;


/* ==================== 构造函数与析构函数 ==================== */

/**
 * @brief 构造函数
 * @param can_interface CAN接口指针
 * @param device_id 设备ID
 */
JC2804::JC2804(BspCan& can_interface, uint8_t device_id)

  : _device_id(device_id),
    _last_request_type(NONE_REQUEST),
    _can(can_interface)
{
}


/**
 * @brief 析构函数
 */
JC2804::~JC2804()
{
}


/* ==================== 底层发送与请求跟踪 ==================== */

/**
 * @brief 异步发送命令
 * @param cmd 命令字节
 * @param data 数据指针
 * @param len 数据长度
 */
void JC2804::send_async_command(uint8_t cmd, const uint8_t* data, uint8_t len)
{
  if (len > 8)
  {
    return;
  }

  uint32_t tx_id = 0x600 | _device_id; // 标准帧ID格式：0x600 + DeviceID
  _can.send(tx_id, const_cast<uint8_t*>(data), len);
}


/**
 * @brief 发送读取请求并记录请求类型
 * @param cmd 命令
 * @param reg_addr 寄存器地址
 * @param req_type 请求类型
 */
void JC2804::send_read_request(uint8_t cmd, uint16_t reg_addr, RequestType req_type)
{
  uint8_t data[8] = {0};
  data[0]         = cmd;                    // 命令字
  data[1]         = (reg_addr >> 8) & 0xFF; // 寄存器地址高字节
  data[2]         = reg_addr & 0xFF;        // 寄存器地址低字节

  /* 记录本次请求类型，以便响应时能正确解析 */
  _last_request_type = req_type;

  send_async_command(cmd, data, 8);
}


/* ==================== 控制指令实现 ==================== */

/**
 * @brief 设置扭矩（电流）
 * @param torque 扭矩值
 */
void JC2804::set_torque(float torque)
{
  /* 文档5.8: 命令字0x2B, 寄存器0x0020, 2字节有符号电流值（单位：A×100） */
  int16_t current_raw = static_cast<int16_t>(torque / CURRENT_SCALE);
  uint8_t data[8]     = {
    0x2B, // Command: Write Register
    0x00,
    0x20, // Register Address: 0x0020 (Current/Torque)
    0x00, // Reserved
    static_cast<uint8_t>(current_raw >> 8),
    static_cast<uint8_t>(current_raw & 0xFF),
    0x00,
    0x00 // Padding
  };
  send_async_command(0x2B, data, 8);
}


/**
 * @brief 设置速度
 * @param speed 速度值（rpm）
 */
void JC2804::set_speed(float speed)
{
  /* 文档5.9: 0x23 + reg 0x21, 4字节有符号速度（rpm×100） */
  int32_t spd_raw = static_cast<int32_t>(speed / SPEED_SCALE);
  uint8_t data[8] = {
    0x23, // Command: Write Register
    0x00,
    0x21, // Register Address: 0x0021 (Speed)
    0x00, // Reserved
    static_cast<uint8_t>(spd_raw >> 24),
    static_cast<uint8_t>(spd_raw >> 16),
    static_cast<uint8_t>(spd_raw >> 8),
    static_cast<uint8_t>(spd_raw & 0xFF)};
  send_async_command(0x23, data, 8);
}


/**
 * @brief 设置绝对位置
 * @param position 位置值（度）
 */
void JC2804::set_absolute_position(float position)
{
  /* 文档5.10: 0x23 + reg 0x23, 4字节有符号位置（度×100） */
  int32_t pos_raw = static_cast<int32_t>(position / POSITION_SCALE);
  uint8_t data[8] = {
    0x23, // Command: Write Register
    0x00,
    0x23, // Register Address: 0x0023 (Absolute Position)
    0x00, // Reserved
    static_cast<uint8_t>(pos_raw >> 24),
    static_cast<uint8_t>(pos_raw >> 16),
    static_cast<uint8_t>(pos_raw >> 8),
    static_cast<uint8_t>(pos_raw & 0xFF)};
  send_async_command(0x23, data, 8);
}


/**
 * @brief 设置相对位置
 * @param position 位置值（度）
 */
void JC2804::set_relative_position(float position)
{
  /* 文档5.11: 0x23 + reg 0x25, 4字节有符号相对位置（度×100） */
  int32_t pos_raw = static_cast<int32_t>(position / POSITION_SCALE);
  uint8_t data[8] = {
    0x23, // Command: Write Register
    0x00,
    0x25, // Register Address: 0x0025 (Relative Position)
    0x00, // Reserved
    static_cast<uint8_t>(pos_raw >> 24),
    static_cast<uint8_t>(pos_raw >> 16),
    static_cast<uint8_t>(pos_raw >> 8),
    static_cast<uint8_t>(pos_raw & 0xFF)};
  send_async_command(0x23, data, 8);
}


/**
 * @brief 低速模式下设置低速
 * @param speed 速度值（rpm）
 */
void JC2804::set_low_speed(float speed)
{
  /* 文档5.12: 0x2B + reg 0x27, 2字节无符号低速（rpm） */
  uint16_t spd_raw = static_cast<uint16_t>(speed);
  uint8_t  data[8] = {
    0x2B, // Command: Write Register
    0x00,
    0x27, // Register Address: 0x0027 (Low Speed)
    0x00, // Reserved
    static_cast<uint8_t>(spd_raw >> 8),
    static_cast<uint8_t>(spd_raw & 0xFF),
    0x00,
    0x00 // Padding
  };
  send_async_command(0x2B, data, 8);
}


/**
 * @brief 速度位置控制
 * @param position 位置值
 * @param speed 速度值
 */
void JC2804::pv_command(int32_t position, float speed)
{
  /* 文档5.13: 命令字0x24 */
  int32_t  pos_raw = static_cast<int32_t>(position * 100); // 放大100倍
  uint16_t spd_raw = static_cast<uint16_t>(speed);         // rpm整数
  uint8_t  data[8] = {
    0x24, // Command: PV Command
    static_cast<uint8_t>(pos_raw >> 24),
    static_cast<uint8_t>(pos_raw >> 16),
    static_cast<uint8_t>(pos_raw >> 8),
    static_cast<uint8_t>(pos_raw & 0xFF),
    static_cast<uint8_t>(spd_raw >> 8),
    static_cast<uint8_t>(spd_raw & 0xFF),
    0x00 // Padding
  };
  send_async_command(0x24, data, 8);
}


/**
 * @brief 速度位置力矩模式
 * @param position 位置值
 * @param speed 速度值
 * @param torque_percent 力矩百分比
 */
void JC2804::pvt_command(int32_t position, float speed, float torque_percent)
{
  /* 文档5.14: 命令字0x25 */
  int32_t  pos_raw    = static_cast<int32_t>(position * 100);
  uint16_t spd_raw    = static_cast<uint16_t>(speed);
  uint8_t  torque_raw = static_cast<uint8_t>(torque_percent);
  uint8_t  data[8]    = {
    0x25, // Command: PVT Command
    static_cast<uint8_t>(pos_raw >> 24),
    static_cast<uint8_t>(pos_raw >> 16),
    static_cast<uint8_t>(pos_raw >> 8),
    static_cast<uint8_t>(pos_raw & 0xFF),
    static_cast<uint8_t>(spd_raw >> 8),
    static_cast<uint8_t>(spd_raw & 0xFF),
    torque_raw // Torque Percentage
  };
  send_async_command(0x25, data, 8);
}


/**
 * @brief 设置控制模式
 * @param mode 模式值（0-5）
 */
void JC2804::set_control_mode(uint8_t mode)
{
  /* 文档5.15: 0x2B + reg 0x60, 写入模式值（0~5） */
  uint8_t data[8] = {
    0x2B, // Command: Write Register
    0x00,
    0x60, // Register Address: 0x0060 (Control Mode)
    0x00, // Reserved
    0x00, // Padding
    mode, // Mode Value (0-5)
    0x00,
    0x00};
  send_async_command(0x2B, data, 8);
}


/* ==================== 系统控制指令实现 ==================== */

/**
 * @brief 系统空闲
 */
void JC2804::idle()
{
  uint8_t data[8] = {0x2B, 0x00, 0xA0, 0x00, 0x00, 0x01, 0x00, 0x00};
  send_async_command(0x2B, data, 8);
}


/**
 * @brief 进入闭环模式
 */
void JC2804::enter_closed_loop()
{
  uint8_t data[8] = {0x2B, 0x00, 0xA2, 0x00, 0x00, 0x01, 0x00, 0x00};
  send_async_command(0x2B, data, 8);
}


/**
 * @brief 擦除数据
 */
void JC2804::erase()
{
  uint8_t data[8] = {0x2B, 0x00, 0xA3, 0x00, 0x00, 0x01, 0x00, 0x00};
  send_async_command(0x2B, data, 8);
}


/**
 * @brief 保存数据
 */
void JC2804::save()
{
  uint8_t data[8] = {0x2B, 0x00, 0xA4, 0x00, 0x00, 0x01, 0x00, 0x00};
  send_async_command(0x2B, data, 8);
}


/**
 * @brief 重启
 */
void JC2804::restart()
{
  uint8_t data[8] = {0x2B, 0x00, 0xA5, 0x00, 0x00, 0x01, 0x00, 0x00};
  send_async_command(0x2B, data, 8);
}


/**
 * @brief 设置零点
 */
void JC2804::set_origin()
{
  uint8_t data[8] = {0x2B, 0x00, 0xA6, 0x00, 0x00, 0x01, 0x00, 0x00};
  send_async_command(0x2B, data, 8);
}


/**
 * @brief 设置临时零点
 */
void JC2804::set_temporary_origin()
{
  uint8_t data[8] = {0x2B, 0x00, 0xA7, 0x00, 0x00, 0x01, 0x00, 0x00};
  send_async_command(0x2B, data, 8);
}


/* ==================== 读取请求实现 ==================== */

/**
 * @brief 读取电压
 */
void JC2804::request_power_voltage()
{
  send_read_request(0x4B, 0x0004, VOLTAGE_REQUEST);
}


/**
 * @brief 读取总线电流
 */
void JC2804::request_bus_current()
{
  send_read_request(0x4B, 0x0005, CURRENT_REQUEST);
}


/**
 * @brief 读取实时速度
 */
void JC2804::request_real_time_speed()
{
  send_read_request(0x43, 0x0006, SPEED_REQUEST);
}


/**
 * @brief 读取实时位置
 */
void JC2804::request_real_time_position()
{
  send_read_request(0x43, 0x0008, POSITION_REQUEST);
}


/**
 * @brief 读取驱动器温度
 */
void JC2804::request_driver_temperature()
{
  send_read_request(0x4B, 0x000A, DRIVER_TEMP_REQUEST);
}


/**
 * @brief 读取电机温度
 */
void JC2804::request_motor_temperature()
{
  send_read_request(0x4B, 0x000B, MOTOR_TEMP_REQUEST);
}


/**
 * @brief 读取错误信息
 */
void JC2804::request_error_info()
{
  send_read_request(0x43, 0x000C, ERROR_INFO_REQUEST);
}


/* ==================== 数据解析实现 ==================== */

/**
 * @brief 解析接收数据
 * @param data 数据指针
 */
void JC2804::store_received_data(const uint8_t (&data)[8])
{
  /* 根据_last_request_type解析数据 */
  switch (_last_request_type)
  {
    case VOLTAGE_REQUEST:
      if (data[0] == 0x4B)
      {
        uint16_t raw        = (static_cast<uint16_t>(data[4]) << 8) | data[5];
        latest_data.voltage = raw * VOLTAGE_SCALE;
        _last_request_type  = NONE_REQUEST; // 清除请求类型
      }
      break;

    case CURRENT_REQUEST:
      if (data[0] == 0x4B)
      {
        uint16_t raw        = (static_cast<uint16_t>(data[4]) << 8) | data[5];
        latest_data.current = raw * CURRENT_SCALE;
        _last_request_type  = NONE_REQUEST;
      }
      break;

    case SPEED_REQUEST:
      if (data[0] == 0x43)
      {
        int32_t raw        = (static_cast<int32_t>(data[4]) << 24) | (static_cast<int32_t>(data[5]) << 16) | (static_cast<int32_t>(data[6]) << 8) | (static_cast<int32_t>(data[7]));
        latest_data.speed  = static_cast<float>(raw) * SPEED_SCALE;
        _last_request_type = NONE_REQUEST;
      }
      break;

    case POSITION_REQUEST:
      if (data[0] == 0x43)
      {
        int32_t raw          = (static_cast<int32_t>(data[4]) << 24) | (static_cast<int32_t>(data[5]) << 16) | (static_cast<int32_t>(data[6]) << 8) | (static_cast<int32_t>(data[7]));
        latest_data.position = static_cast<float>(raw) * POSITION_SCALE;
        _last_request_type   = NONE_REQUEST;
      }
      break;

    case DRIVER_TEMP_REQUEST:
      if (data[0] == 0x4B)
      {
        uint16_t raw            = (static_cast<uint16_t>(data[4]) << 8) | data[5];
        latest_data.driver_temp = raw * TEMPERATURE_SCALE;
        _last_request_type      = NONE_REQUEST;
      }
      break;

    case MOTOR_TEMP_REQUEST:
      if (data[0] == 0x4B)
      {
        uint16_t raw           = (static_cast<uint16_t>(data[4]) << 8) | data[5];
        latest_data.motor_temp = raw * TEMPERATURE_SCALE;
        _last_request_type     = NONE_REQUEST;
      }
      break;

    case ERROR_INFO_REQUEST:
      if (data[0] == 0x43)
      {
        uint32_t raw           = (static_cast<uint32_t>(data[4]) << 24) | (static_cast<uint32_t>(data[5]) << 16) | (static_cast<uint32_t>(data[6]) << 8) | (static_cast<uint32_t>(data[7]));
        latest_data.error_info = raw;
        _last_request_type     = NONE_REQUEST;
      }
      break;

    default:
      /* 如果没有匹配的请求类型，则清零 */
      _last_request_type = NONE_REQUEST;
      break;
  }
}


/**
 * @brief 响应验证
 * @param expected_cmd 期望命令
 * @param rx_msg 接收消息指针
 * @return 验证结果
 */
bool JC2804::validate_response(uint8_t expected_cmd, const CanRxMsg_t& rx_msg)
{
  (void)expected_cmd; // 未使用参数

  uint32_t expected_id = 0x580 | _device_id; // 响应ID格式：0x580 + DeviceID

  if (rx_msg.header.Identifier != expected_id)
  {
    return false;
  }

  /* 简化验证逻辑，只要ID对得上，就认为是本设备的响应 */
  return true;
}


/**
 * @brief CAN消息回调处理
 * @param rx_msg CAN接收消息
 */
void JC2804::on_can_message(const CanRxMsg_t& rx_msg)
{
  /* 验证消息ID是否属于本设备 */
  if (!validate_response(0, rx_msg))
  {
    return;
  }

  uint8_t received_cmd = rx_msg.data[0];

  /* 检查是否是读取响应，并且我们之前确实发送了读取请求 */
  if (((received_cmd == 0x4B || received_cmd == 0x43) && _last_request_type != NONE_REQUEST))
  {
    /* 尝试解析响应数据 */
    store_received_data(rx_msg.data);
  }
}


/**
 * @brief 获取最新数据
 * @return MotorData 电机数据
 */
MotorData JC2804::get_latest_data_struct()
{
  MotorData data_copy = latest_data;
  return data_copy;
}
