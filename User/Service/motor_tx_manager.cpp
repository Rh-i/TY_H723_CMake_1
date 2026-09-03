#include "motor_tx_manager.hpp"

#include "FreeRTOS.h" // IWYU pragma: keep
#include "task.h"


namespace
{
class ScopedTaskCritical
{
public:
  ScopedTaskCritical()
    : _active(xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
  {
    if (_active)
    {
      taskENTER_CRITICAL();
    }
  }

  ~ScopedTaskCritical()
  {
    if (_active)
    {
      taskEXIT_CRITICAL();
    }
  }

private:
  bool _active;
};
} // namespace


MotorTxManager::Endpoint *MotorTxManager::_head = nullptr;
MotorTxManager::Endpoint *MotorTxManager::_tail = nullptr;
uint32_t MotorTxManager::_tick = 0U;


Status MotorTxManager::register_endpoint(Endpoint &endpoint)
{
  if ((endpoint._send_callback == nullptr) ||
      (endpoint._schedule.period_ticks == 0U) ||
      (endpoint._schedule.phase_ticks >= endpoint._schedule.period_ticks))
  {
    return Status::BAD_ARG;
  }

  const ScopedTaskCritical lock;
  if (endpoint._registered)
  {
    return Status::BUSY;
  }

  Endpoint *previous = nullptr;
  Endpoint *current  = _head;
  while ((current != nullptr) &&
         (current->_schedule.order <= endpoint._schedule.order))
  {
    previous = current;
    current  = current->_next;
  }

  endpoint._next = current;
  if (previous == nullptr)
  {
    _head = &endpoint;
  }
  else
  {
    previous->_next = &endpoint;
  }

  if (current == nullptr)
  {
    _tail = &endpoint;
  }

  endpoint._registered = true;
  endpoint._last_status = Status::OK;
  return Status::OK;
}


Status MotorTxManager::unregister_endpoint(Endpoint &endpoint)
{
  const ScopedTaskCritical lock;
  if (!endpoint._registered)
  {
    return Status::BAD_ARG;
  }

  Endpoint *previous = nullptr;
  Endpoint *current  = _head;
  while ((current != nullptr) && (current != &endpoint))
  {
    previous = current;
    current  = current->_next;
  }

  if (current == nullptr)
  {
    return Status::BAD_ARG;
  }

  if (previous == nullptr)
  {
    _head = current->_next;
  }
  else
  {
    previous->_next = current->_next;
  }

  if (_tail == current)
  {
    _tail = previous;
  }

  current->_next        = nullptr;
  current->_registered  = false;
  current->_last_status = Status::NOT_INIT;
  return Status::OK;
}


Status MotorTxManager::update(void)
{
  Status result = Status::OK;
  const ScopedTaskCritical lock;

  for (Endpoint *endpoint = _head; endpoint != nullptr; endpoint = endpoint->_next)
  {
    const Schedule &schedule = endpoint->_schedule;
    if ((_tick % schedule.period_ticks) != schedule.phase_ticks)
    {
      continue;
    }

    if ((endpoint->_ready_callback != nullptr) &&
        !endpoint->_ready_callback(endpoint->_context))
    {
      continue;
    }

    const Status send_status = endpoint->_send_callback(endpoint->_context);
    endpoint->_last_status   = send_status;
    if ((result == Status::OK) && (send_status != Status::OK))
    {
      result = send_status;
    }
  }

  ++_tick;
  return result;
}


uint32_t MotorTxManager::tick(void)
{
  const ScopedTaskCritical lock;
  return _tick;
}
