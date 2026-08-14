/**
 * @file device_cfg.hpp
 * @author Rh
 * @brief 设备层统一管理 —— extern 声明与 device_init()
 * @version 0.1
 * @date 2026-07-21
 *
 * @copyright Copyright (c) 2026
 *
 * @note 所有设备实例在此统一声明，在 device_cfg.cpp 中统一实例化。
 *       使用者只需 include 此头文件即可访问所有设备实例。
 *       当前为底层库形态：电机仅实例化、不初始化（不绑定到位信号量、不创建接收任务），
 *       业务接入时按需启用；LCD 由 menu_init() 正常初始化。
 */

#ifndef __DEVICE_CFG_HPP__
#define __DEVICE_CFG_HPP__

#include "device_emmv5.hpp"
#include "dm_imu.hpp"
#include "JC2804.hpp"
#include "Lcd.hpp"


/* ==================== 函数声明 ==================== */

void device_init();


/* ==================== 全局声明 ==================== */

///< EmmV5 步进闭环电机 — 每路独占一个 UART 绑定
///< 电机 ↔ UART ↔ 地址 唯一事实源（2.6，以 device_cfg.cpp 实例化为准）
///< ┌──────────┬───────┬────────┐
///< │ 电机      │ UART  │ 地址   │
///< ├──────────┼───────┼────────┤
///< │ y1       │ UART10│  1     │
///< │ y2       │ UART4 │  2     │
///< │ x        │ UART8 │  3     │
///< │ z        │ UART9 │  4     │
///< │ yaw      │ UART7 │  5     │
///< └──────────┴───────┴────────┘
extern DeviceEmmV5 emm_motor_x;
extern DeviceEmmV5 emm_motor_z;
extern DeviceEmmV5 emm_motor_yaw;
extern DeviceEmmV5 emm_motor_y1;
// extern DeviceEmmV5 emm_motor_y2; // TODO: CubeMX 添加 UART4 后启用


///< JC2804 云台电机（CAN1，ID = 0x600|设备号）
extern JC2804 motor_pitch;
extern JC2804 motor_yaw;

///< DM IMU（CAN1）
extern DmImu imu_bmi088;

///< LCD（SPI1 DMA）
extern Lcd lcd;


#endif // __DEVICE_CFG_HPP__
