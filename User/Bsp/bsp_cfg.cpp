#include "bsp_cfg.hpp"


/* ==================== 声明的句柄 ==================== */

extern FDCAN_HandleTypeDef hfdcan1;
extern UART_HandleTypeDef  huart1;


/* ==================== 串口重定向 ==================== */

#define PRINT_UART huart1

/**
 * @brief ARM_GCC UART6 串口重定向、但阻塞 (printf)、使用了cubemx自带的设置，为重定向自动加锁
 * @note extern "C" 的原因是，这些函数是覆盖在原来weak弱定义上的，不能被cpp进行名称修饰
 */
extern "C" int __io_putchar(int ch)
{
  HAL_UART_Transmit(&PRINT_UART, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
  return ch;
}
extern "C" int _write(int fd, char *ptr, int len)
{
  HAL_UART_Transmit(&PRINT_UART, (uint8_t *)ptr, len, HAL_MAX_DELAY);
  return len;
}


/* ==================== 全局实例化 ==================== */

///< CAN

/**
 * @brief 全局实例化
 * @param CAN句柄
 * @param 实例名称（用于资源命名）
 * @param CAN工作模式（默认正常模式）
 */
bsp_can bsp_can1(&hfdcan1, "CAN1");


///< UART

/**
 * @brief 全局实例化，模板实例化需要在BspUsart中实现
 * @param 第一个串口句柄
 * @param 第二个是串口接收模式
 * @param 第三个是是否启用发送逻辑
 * @note 这个 __attribute__((section(".dma_buffer"))) 是把他放到dtcm区域外，在.ld格式文件下实现的
 *
 */
__attribute__((section(".dma_buffer"))) bsp_usart<128, 8> bsp_usart1(&huart1, receive_mode::SINGLE_BUFFER, true, 1); // 添加实例ID为6


///< USB

/**
 * @brief USB BSP 全局单例引用定义。
 *
 */
BspUsb& bsp_usb = BspUsb::instance();