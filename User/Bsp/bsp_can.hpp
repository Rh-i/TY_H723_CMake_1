/**
 * @file bsp_can.hpp
 * @author Rh
 * @brief 实现了一个简易的CAN2.0收发驱动（标准CAN，不支持CANFD）
 * @version 0.2
 * @date 2026-02-11
 *
 * @todo 1. 后续可扩展CAN2/CAN3支持；2. 优化过滤器配置 3. 目前使用查表法存入回调函数中，可能查询速度会慢，希望后人处理
 *
 * @copyright Copyright (c) 2026
 *
 * @details 使用示例：（必须要在freertos的任务中运行收发 中断中不行 中断不能阻塞）
 *
 * @note 使用示例
 *
 *   // 全局实例化类
 *   bsp_can bsp_can1(&hfdcan1);
 *
 *   // 初始化（在freertos内核开启之后调用）
 *   bsp_can1.init();
 *
 * @note 发送接收使用示例
 *
 *   // 发送数据（阻塞式）
 *   uint8_t tx_data[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
 *   bsp_can1.send(0x602, tx_data, 8);
 *
 *   // 接收数据（阻塞式）- 用户需要在自己的任务中调用
 *   can_rx_msg_t rx_msg;
 *   bsp_can1.receive(&rx_msg, osWaitForever);
 *
 * @note 重要说明：
 *   - 此类不创建内部接收任务，用户需要自行创建任务调用receive()处理数据
 *   - 每个CAN的接收处理逻辑完全由用户控制，可独立处理
 *
 * @note 三种工作模式说明：
 *   - 正常模式：正常收发CAN报文
 *   - 静默模式：只接收CAN报文，不发送，用于监听总线
 *   - 环回模式：自发自收，用于测试
 *
 * @note 暂不支持CANFD：所有CANFD相关代码/注释已标注
 */


#ifndef __BSP_CAN_HPP__
#define __BSP_CAN_HPP__

#include "cmsis_os2.h" 
#include "fdcan.h"     // IWYU pragma: keep
#include "FreeRTOS.h"  // IWYU pragma: keep
#include "task.h"      


/* USER CODE BEGIN */

/* ==================== 外部声明 ==================== */

// 前向声明
class bsp_can;

/**
 * @brief 全局实例化
 * @param CAN句柄
 * @param 实例名称（用于资源命名）
 * @param CAN工作模式（默认正常模式）
 */
extern bsp_can bsp_can1;

/* USER CODE END */


/**
 * @brief CAN工作模式枚举
 *
 * @note 暂不支持CANFD模式
 */
enum class can_mode
{
  NORMAL   = 0, ///< 正常模式：正常收发CAN报文
  SILENT   = 1, ///< 静默模式：只接收CAN报文，不发送，用于监听总线
  LOOPBACK = 2  ///< 环回模式：自发自收，用于测试
};


/**
 * @brief CAN接收消息结构体（标准CAN）
 *
 * @note 暂不支持CANFD：使用FDCAN_RxHeaderTypeDef，不支持扩展帧和CANFD
 */
typedef struct
{
  FDCAN_RxHeaderTypeDef header;  ///< CAN帧头信息
  uint8_t               data[8]; ///< 接收数据（标准CAN最大8字节）
} can_rx_msg_t;


/**
 * @brief CAN发送消息结构体（标准CAN）
 *
 * @note 暂不支持CANFD：仅支持标准CAN帧
 */
typedef struct
{
  uint32_t std_id;  ///< 标准CAN ID
  uint8_t  data[8]; ///< 发送数据
  uint8_t  len;     ///< 数据长度（0-8字节）
} can_tx_msg_t;


/**
 * @brief CAN驱动类
 *
 * @note 仅支持标准CAN（CAN2.0），不支持CANFD
 * @note 设计要点：
 *       - 使用FreeRTOS的互斥锁保护发送寄存器
 *       - 使用消息队列缓存接收数据
 *       - 不创建内部接收任务，由用户自行创建任务调用receive()处理
 *       - 接收处理逻辑完全由用户控制，每个CAN可独立处理
 */
class bsp_can
{
private:
  /* ==================== 硬件相关 ==================== */
  FDCAN_HandleTypeDef *_hfdcan; ///< CAN/FDCAN句柄指针
  const char          *_name;   ///< 实例名称（用于资源命名）

public:
  osMessageQueueId_t      _rx_queue_handle = nullptr; ///< CMSIS_OS2消息队列（接收）
  osMutexId_t             _tx_mutex_handle = nullptr; ///< CMSIS_OS2互斥锁（发送）
  can_mode                _work_mode;                 ///< CAN工作模式
  bool                    _is_initialized = false;    ///< 初始化标志
  char                    _queue_name[32];            ///< 消息队列名字
  char                    _mutex_name[32];            ///< 互斥锁名字
  static constexpr size_t MAX_INSTANCES = 3;          ///< 最大实例数量
  static bsp_can         *_instances[MAX_INSTANCES];  ///< 实例指针数组
  static size_t           _instance_count;            ///< 已注册实例数量

  /* ==================== 公共接口 ==================== */

  /**
   * @brief 构造函数
   *
   * @note 初始化CAN驱动对象，配置FreeRTOS资源
   *
   * @param hfdcan CAN/FDCAN句柄指针
   * @param name 实例名称（用于生成资源名称），默认为"CAN"
   * @param mode CAN工作模式，默认为正常模式
   */
  bsp_can(FDCAN_HandleTypeDef *hfdcan, const char *name = "CAN", can_mode mode = can_mode::NORMAL);

  /**
   * @brief 析构函数
   *
   * @note 释放所有分配的资源
   */
  ~bsp_can();

  /**
   * @brief 初始化函数
   *
   * @note 必须在FreeRTOS内核初始化完成后调用
   *       创建消息队列、互斥锁，并启动CAN硬件
   *       不创建接收任务，由用户自行创建任务处理接收数据
   *
   * @return true 初始化成功
   * @return false 初始化失败
   */
  bool init();

  /**
   * @brief 发送标准CAN数据帧（阻塞式）
   *
   * @note 内部使用互斥锁保护，支持超时
   * @note 暂不支持CANFD：仅支持标准CAN帧
   *
   * @param stdId 标准CAN ID（11位）
   * @param pData 待发送数据指针
   * @param len 数据长度（0-8字节）
   * @param timeout 超时时间（tick），默认永久等待
   * @return HAL_StatusTypeDef HAL_OK/HAL_ERROR/HAL_TIMEOUT
   */
  HAL_StatusTypeDef send(uint32_t stdId, uint8_t *pData, uint8_t len, uint32_t timeout = osWaitForever);

  /**
   * @brief 接收CAN数据帧（阻塞式）
   *
   * @note 从消息队列中获取数据，支持超时等待
   *       用户需要在自己的任务中循环调用此函数处理接收数据
   *
   * @param msg 接收消息结构体指针
   * @param timeout 超时时间（tick），默认永久等待
   * @return osStatus_t osOK/osErrorTimeout/osErrorResource
   */
  osStatus_t receive(can_rx_msg_t *msg, uint32_t timeout = osWaitForever);

  /**
   * @brief 获取接收队列中的消息数量
   *
   * @return uint32_t 队列中的消息数量
   */
  uint32_t get_queue_count();

  /**
   * @brief 获取CAN硬件句柄指针
   *
   * @return FDCAN_HandleTypeDef* CAN句柄指针
   */
  FDCAN_HandleTypeDef *get_handle()
  {
    return _hfdcan;
  }

  /**
   * @brief 通过CAN句柄查找对应的bsp_can实例
   *
   * @note 静态成员函数，用于在中断回调中通过CAN句柄找到对应的类实例
   *
   * @param hfdcan CAN句柄指针
   * @return bsp_can* 找到的实例指针，未找到返回nullptr
   */
  static bsp_can *get_instance_by_handle(FDCAN_HandleTypeDef *hfdcan);

  /**
   * @brief 注册实例到静态注册表中
   *
   * @note 在构造函数中自动调用
   *
   * @return true 注册成功
   * @return false 注册失败（已满）
   */
  bool register_instance();

private:
  /**
   * @brief 配置CAN过滤器
   *
   * @note 配置为接收所有标准ID
   *
   * @return HAL_StatusTypeDef HAL_OK/HAL_ERROR
   */
  HAL_StatusTypeDef config_filter();

  /**
   * @brief 启动CAN硬件
   *
   * @note 根据工作模式配置FDCAN
   * @note 暂不支持CANFD：使用经典CAN模式
   *
   * @return HAL_StatusTypeDef HAL_OK/HAL_ERROR
   */
  HAL_StatusTypeDef start_hardware();

  /**
   * @brief 清理资源
   *
   * @note 在析构函数和初始化失败时调用
   */
  void cleanup_resources();

  /**
   * @brief 创建FreeRTOS资源
   *
   * @note 创建消息队列、互斥锁
   *
   * @return true 创建成功
   * @return false 创建失败
   */
  bool create_rtos_resources();

  /**
   * @brief 启动CAN接收
   *
   * @note 开启接收中断
   *
   * @return HAL_StatusTypeDef HAL_OK/HAL_ERROR
   */
  HAL_StatusTypeDef start_reception();
};


#endif // __BSP_CAN_HPP__
