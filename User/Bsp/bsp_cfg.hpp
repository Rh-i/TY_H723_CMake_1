/**
 * @file bsp_cfg.hpp
 * @author ALL
 * @brief 作为bsp层的总初始化和extern部分，更方便管理和移植
 * @version 0.1
 * @date 2026-04-18
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef __BSP_CFG_HPP__
#define __BSP_CFG_HPP__

#include "bsp_can.hpp"
#include "bsp_uart.hpp"
#include "bsp_usb.hpp"

/* ==================== 全局 声明 ==================== */

///< CAN
extern BspCan bsp_can1;


///< USART

extern BspUart<128, 8> bsp_usart1;


///< USB
extern BspUsb& bsp_usb;


#endif // __BSP_CFG_HPP__