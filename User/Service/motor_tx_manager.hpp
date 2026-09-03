/**
 * @file motor_tx_manager.hpp
 * @author ChoseB
 * @brief 不同电机协议控制帧的统一周期发送管理声明
 * @version 0.1
 * @date 2026-09-02
 *
 * @details 发送管理器以 CAN 控制帧端点为调度单位，而不是以逻辑电机为单位。
 *          一个普通 DM 电机或一个 DJI 聚合控制 Bus 均可分别注册为一个 Endpoint。
 *          update() 由单一固定频率任务调用，再根据各端点的 period_ticks、
 *          phase_ticks 和 order 决定发送时机及顺序。
 *
 * @note 注册、注销、排序、并发保护和发送逻辑实现在 motor_tx_manager.cpp。
 */
#ifndef __MOTOR_TX_MANAGER_HPP__
#define __MOTOR_TX_MANAGER_HPP__

#include "status.hpp"

#include <stdint.h>

class MotorTxManager
{
public:
  typedef Status (*SendCallback)(void *context);
  typedef bool (*ReadyCallback)(const void *context);

  /**
   * @brief 单个控制帧端点的周期调度参数
   */
  struct Schedule
  {
    Schedule(uint16_t period_ticks = 1U, uint16_t phase_ticks = 0U, uint16_t order = 0U)
      : period_ticks(period_ticks),
        phase_ticks(phase_ticks),
        order(order)
    {
    }

    uint16_t period_ticks; ///< 相邻两次发送之间的 update() 次数，必须大于 0
    uint16_t phase_ticks;  ///< 周期内发送相位，必须小于 period_ticks
    uint16_t order;        ///< 同一 update() 内的发送顺序，数值较小者优先
  };

  /**
   * @brief 可注册到统一发送管理器的非拥有型控制帧端点
   *
   * @note context、回调和 Endpoint 对象必须在注册期间始终有效。Endpoint 不拥有 context。
   */
  class Endpoint
  {
  public:
    Endpoint(void *context = nullptr,
             SendCallback send_callback = nullptr,
             ReadyCallback ready_callback = nullptr,
             const Schedule &schedule = Schedule())
      : _context(context),
        _send_callback(send_callback),
        _ready_callback(ready_callback),
        _schedule(schedule),
        _last_status(Status::NOT_INIT),
        _next(nullptr),
        _registered(false)
    {
    }

    Status last_status(void) const
    {
      return _last_status;
    }

    bool registered(void) const
    {
      return _registered;
    }

    Endpoint(const Endpoint &) = delete;
    Endpoint &operator=(const Endpoint &) = delete;
    Endpoint(Endpoint &&) = delete;
    Endpoint &operator=(Endpoint &&) = delete;

  private:
    void         *_context;        ///< 回调上下文，由具体电机或聚合 Bus 持有
    SendCallback  _send_callback;  ///< 发送一帧缓存控制数据
    ReadyCallback _ready_callback; ///< true 时本周期允许发送；nullptr 表示始终允许
    Schedule      _schedule;       ///< 周期、相位和顺序
    Status        _last_status;    ///< 最近一次回调执行结果
    Endpoint     *_next;           ///< 内部有序单向链表的后继节点
    bool          _registered;     ///< 是否已加入统一管理器

    friend class MotorTxManager;
  };

  /**
   * @brief 注册一个控制帧端点
   * @param endpoint 待注册端点
   * @return Status::OK 注册成功；Status::BAD_ARG 配置或回调非法；
   *         Status::BUSY 端点已经注册。
   * @note 端点按 order 排序；order 相同时保持注册先后顺序。
   */
  static Status register_endpoint(Endpoint &endpoint);

  /**
   * @brief 注销一个控制帧端点
   * @param endpoint 待注销端点
   * @return Status::OK 注销成功；Status::BAD_ARG 端点未注册或链表异常。
   */
  static Status unregister_endpoint(Endpoint &endpoint);

  /**
   * @brief 推进一次统一电机发送调度
   * @return Status::OK 本周期所有应发送端点均成功；其他状态表示至少一帧发送失败。
   * @note 应由单一高优先级任务以固定基准频率调用。
   */
  static Status update(void);

  /**
   * @brief 查询管理器已经推进的基准周期数
   */
  static uint32_t tick(void);

private:
  static Endpoint *_head;
  static Endpoint *_tail;
  static uint32_t  _tick;
};

#endif // __MOTOR_TX_MANAGER_HPP__
