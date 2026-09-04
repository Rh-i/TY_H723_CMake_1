#ifndef LOADER_BOARD_H
#define LOADER_BOARD_H

/**
 * @brief 在没有应用启动代码的情况下初始化 Loader 运行环境。
 *
 * 启用 FPU，安装 RAM 向量表，初始化 HAL/SysTick、480 MHz 系统时钟、
 * QSPI 引脚和外设。External Loader 的 Init() 会首先调用本函数。
 *
 * @return 0 成功，-1 表示时钟或 QSPI 初始化失败。
 */
int loader_board_init(void);

/**
 * @brief 板级初始化阶段诊断值：1=开始，2=HAL 完成，3=时钟完成，4=完成；
 *        -2=时钟失败，-3=QSPI 失败。
 */
extern volatile int loader_board_stage;

#endif
