#ifndef __BSP_CFG_HPP__
#define __BSP_CFG_HPP__

#include "bsp_can.hpp"
#include "bsp_usart.hpp"


/* ==================== 全局 声明 ==================== */

///< CAN 
extern bsp_can bsp_can1;


///< USART
extern bsp_usart<128,8> bsp_usart1;


#endif // __BSP_CFG_HPP__