/**
 * @file uart_type_list.hpp
 * @author Rh
 * @brief 串口编译期多态工具箱 —— C++11 可变参数类型列表
 * @version 0.1
 * @date 2026-08-13
 *
 * @copyright Copyright (c) 2026
 *
 * @details 编译期“对 UART 类型列表中每个类型执行一次 F”，用于串口场景的纯静态分发：
 *          零虚表、零函数指针、零运行时循环。
 *
 * @code
 *   using Uarts = UartTypeList<Uart1, Uart4, Uart7>;
 *   struct UartDispatcher { template <typename T> void apply(); };
 *   UartDispatcher d;
 *   ForEachUartType<Uarts, UartDispatcher>::run(d);
 * @endcode
 */

#ifndef __SERVICE_UART_TYPE_LIST_HPP__
#define __SERVICE_UART_TYPE_LIST_HPP__

/**
 * @brief UART 类型列表
 */
template <typename... Ts>
struct UartTypeList
{
};

/**
 * @brief 对 UART 类型列表中每个类型执行一次 F（递归展开）
 */
template <typename List, typename F>
struct ForEachUartType; // 声明

template <typename Head, typename... Tail, typename F>
struct ForEachUartType<UartTypeList<Head, Tail...>, F>
{
  static void run(F &f)
  {
    f.template apply<Head>();
    ForEachUartType<UartTypeList<Tail...>, F>::run(f);
  }
};

template <typename F>
struct ForEachUartType<UartTypeList<>, F>
{
  static void run(F &)
  {
  }
};

#endif // __SERVICE_UART_TYPE_LIST_HPP__
