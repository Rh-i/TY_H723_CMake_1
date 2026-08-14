#include "bsp_cfg.hpp"

/* ==================== 串口重定向 ==================== */

#define PRINT_UART huart1

/**
 * @brief ARM_GCC USART1 串口重定向、但阻塞 (printf)
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


/**
 * @brief bsp层整体的初始化
 * 
 */
void bsp_init()
{
  bsp_can1.init();
  bsp_can2.init();
  bsp_can3.init();
  bsp_usart1.init();
}


/* ==================== 全局实例化 ==================== */

///< CAN

/**
 * @brief 全局实例化
 * @param CAN句柄
 * @param 实例名称（用于资源命名）
 * @param CAN工作模式（默认正常模式）
 */
BspCan bsp_can1(&hfdcan1, "CAN1");
BspCan bsp_can2(&hfdcan2, "CAN2");
BspCan bsp_can3(&hfdcan3, "CAN3");


///< UART 但模板实例化需要在BspUsart中实现

/**
 * @brief 全局实例化
 * @param 第一个串口句柄
 * @param 第二个是串口接收模式
 * @param 第三个是是否启用发送逻辑
 * @note 这个 __attribute__((section(".dma_buffer"))) 是把他放到dtcm区域外，在.ld格式文件下实现的
 *
 */
__attribute__((section(".dma_buffer"))) BspUart<64, 8> bsp_usart1(&huart1, ReceiveMode::SINGLE_BUFFER, true, 1); // 添加实例ID为1


///< USB

/**
 * @brief USB BSP 全局单例引用定义。
 *
 */
BspUsb &bsp_usb = BspUsb::instance();