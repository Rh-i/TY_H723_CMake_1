#include "dji_motor.hpp"

#include "FreeRTOS.h" // IWYU pragma: keep
#include "task.h"

#include <math.h>
#include <string.h>


namespace
{
constexpr uint16_t ENCODER_COUNTS     = 8192U;
constexpr int16_t  WRAP_THRESHOLD     = 4096;
constexpr float    TWO_PI             = 6.28318530717958647692f;
constexpr float    RPM_TO_RAD_PER_SEC = TWO_PI / 60.0f;

/** @brief 在调度器运行后用任务临界区保护共享的注册表与输出数据。 */
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

int16_t read_i16_be(const uint8_t *data)
{
  const uint16_t raw = (static_cast<uint16_t>(data[0]) << 8U) | data[1];
  return static_cast<int16_t>(raw);
}
} // namespace


/* ==================== DjiMotorBus ==================== */

DjiMotorBus::DjiMotorBus()
  : _tx_msg{},
    _can_item(nullptr),
    _slots{},
    _attached_count(0U),
    _used(false),
    _statu(Status::NOT_INIT)
{
}


DjiMotorBus::~DjiMotorBus()
{
  _can_item       = nullptr;
  _attached_count = 0U;
  _used           = false;
  _statu          = Status::NOT_INIT;
}


DjiMotorBus *DjiMotorBus::pool(void)
{
  static DjiMotorBus pool[MAX_BUS_COUNT];
  return pool;
}


Status DjiMotorBus::acquire(BspCan &can_item,
                            uint32_t std_id,
                            uint8_t slot,
                            DjiMotorBus **bus)
{
  if (bus == nullptr)
  {
    return Status::BAD_ARG;
  }
  *bus = nullptr;

  if ((std_id > 0x7FFU) || (slot >= 4U))
  {
    return Status::BAD_ARG;
  }

  const ScopedTaskCritical lock;

  DjiMotorBus *bus_pool = pool();
  DjiMotorBus *free_bus = nullptr;
  for (uint8_t index = 0U; index < MAX_BUS_COUNT; ++index)
  {
    DjiMotorBus &item = bus_pool[index];
    if (item._used)
    {
      if ((item._can_item == &can_item) && (item._tx_msg.std_id == std_id))
      {
        if (item._slots[slot].occupied)
        {
          item._statu = Status::BUSY;
          return Status::BUSY;
        }

        item._slots[slot].occupied = true;
        item._slots[slot].output   = 0;
        ++item._attached_count;
        item._statu = Status::OK;
        *bus        = &item;
        return Status::OK;
      }
    }
    else if (free_bus == nullptr)
    {
      free_bus = &item;
    }
  }

  if (free_bus == nullptr)
  {
    return Status::FULL;
  }

  memset(&free_bus->_tx_msg, 0, sizeof(free_bus->_tx_msg));
  memset(free_bus->_slots, 0, sizeof(free_bus->_slots));
  free_bus->_tx_msg.std_id         = std_id;
  free_bus->_can_item              = &can_item;
  free_bus->_slots[slot].output    = 0;
  free_bus->_slots[slot].occupied = true;
  free_bus->_attached_count       = 1U;
  free_bus->_used                 = true;
  free_bus->_statu                = Status::OK;
  *bus                            = free_bus;

  return Status::OK;
}


Status DjiMotorBus::release(DjiMotorBus *bus, uint8_t slot)
{
  if ((bus == nullptr) || (slot >= 4U))
  {
    return Status::BAD_ARG;
  }

  const ScopedTaskCritical lock;

  bool belongs_to_pool = false;
  DjiMotorBus *bus_pool = pool();
  for (uint8_t index = 0U; index < MAX_BUS_COUNT; ++index)
  {
    if (&bus_pool[index] == bus)
    {
      belongs_to_pool = true;
      break;
    }
  }

  if (!belongs_to_pool || !bus->_used || !bus->_slots[slot].occupied)
  {
    return Status::BAD_ARG;
  }

  bus->_slots[slot].output   = 0;
  bus->_slots[slot].occupied = false;
  if (bus->_attached_count > 0U)
  {
    --bus->_attached_count;
  }

  if (bus->_attached_count == 0U)
  {
    memset(&bus->_tx_msg, 0, sizeof(bus->_tx_msg));
    memset(bus->_slots, 0, sizeof(bus->_slots));
    bus->_can_item = nullptr;
    bus->_used     = false;
    bus->_statu    = Status::NOT_INIT;
  }
  else
  {
    bus->_statu = Status::OK;
  }

  return Status::OK;
}


Status DjiMotorBus::setOutput(uint8_t slot, int16_t output)
{
  if (slot >= 4U)
  {
    return Status::BAD_ARG;
  }

  const ScopedTaskCritical lock;
  if (!_used || (_can_item == nullptr) || !_slots[slot].occupied)
  {
    return Status::NOT_INIT;
  }

  _slots[slot].output = output;
  _statu              = Status::OK;
  return Status::OK;
}


Status DjiMotorBus::send(void)
{
  uint8_t data[8] = {0};
  BspCan *can_item = nullptr;
  uint32_t std_id  = 0U;

  {
    const ScopedTaskCritical lock;
    if (!_used || (_can_item == nullptr))
    {
      return Status::NOT_INIT;
    }

    can_item = _can_item;
    std_id   = _tx_msg.std_id;
    for (uint8_t slot = 0U; slot < 4U; ++slot)
    {
      const uint16_t raw = _slots[slot].occupied
                             ? static_cast<uint16_t>(_slots[slot].output)
                             : 0U;
      data[slot * 2U]      = static_cast<uint8_t>(raw >> 8U);
      data[slot * 2U + 1U] = static_cast<uint8_t>(raw);
    }
    memcpy(_tx_msg.data, data, sizeof(data));
  }

  const Status result = can_item->send(std_id, data);
  {
    const ScopedTaskCritical lock;
    _statu = result;
  }
  return result;
}


Status DjiMotorBus::sendAll(void)
{
  Status result = Status::OK;
  DjiMotorBus *bus_pool = pool();

  for (uint8_t index = 0U; index < MAX_BUS_COUNT; ++index)
  {
    bool used = false;
    {
      const ScopedTaskCritical lock;
      used = bus_pool[index]._used;
    }

    if (used)
    {
      const Status send_status = bus_pool[index].send();
      if ((result == Status::OK) && (send_status != Status::OK))
      {
        result = send_status;
      }
    }
  }

  return result;
}


Status DjiMotorSendAll(void)
{
  return DjiMotorBus::sendAll();
}


/* ==================== DjiMotor ==================== */

template <MotorType type>
DjiMotor<type> *DjiMotor<type>::_head = nullptr;

template <MotorType type>
DjiMotor<type> *DjiMotor<type>::_tail = nullptr;


template <MotorType type>
DjiMotor<type>::DjiMotor(BspCan &can_item,
                         uint8_t motor_id,
                         float ratio,
                         uint16_t ecd_offset,
                         DjiMotorControlMode control_mode)
  : _data{},
    _online(30U),
    _lvbo_data{},
    _bus(nullptr),
    _can_item(&can_item),
    _motor_id(motor_id),
    _bus_slot(0U),
    _control_mode(control_mode),
    _statu(Status::NOT_INIT),
    _last_ecd(0),
    _total_ecd(0),
    _last_velocity(0.0f),
    _feedback_ready(false),
    _next(nullptr)
{
  if (!MotorIDValid(motor_id) || (ecd_offset >= ENCODER_COUNTS) || (ratio < 0.0f))
  {
    _statu = Status::BAD_ARG;
    return;
  }

  if (ratio == 0.0f)
  {
    if (type == Motor3508)
    {
      ratio = 3591.0f / 187.0f;
    }
    else if (type == Motor2006)
    {
      ratio = 36.0f;
    }
    else
    {
      ratio = 1.0f;
    }
  }

  if (type != Motor6020)
  {
    _control_mode = DjiMotorControlMode::CURRENT;
  }

  _data.param.ecdOffset    = ecd_offset;
  _data.param.ecdFullRange = ENCODER_COUNTS;
  _data.param.currentLimit = static_cast<uint16_t>(output_limit(_control_mode));
  _data.param.ratio        = ratio;
  _bus_slot                = ControlSlot(motor_id);

  if (dji_motor_feedback_in_use(can_item, feedback_std_id(motor_id)))
  {
    _statu = Status::BUSY;
    return;
  }

  _statu = DjiMotorBus::acquire(can_item,
                                ControlStdID(motor_id, _control_mode),
                                _bus_slot,
                                &_bus);
  if (_statu == Status::OK)
  {
    register_motor();
  }
}


template <MotorType type>
DjiMotor<type>::~DjiMotor()
{
  if (_bus != nullptr)
  {
    unregister_motor();
    DjiMotorBus::release(_bus, _bus_slot);
    _bus = nullptr;
  }
}


template <MotorType type>
const MotorData &DjiMotor<type>::data(void) const
{
  return _data;
}


template <MotorType type>
const Online &DjiMotor<type>::online(void) const
{
  return _online;
}


template <MotorType type>
const LuenbergerMotorData &DjiMotor<type>::lvboData(void) const
{
  return _lvbo_data;
}


template <MotorType type>
Status DjiMotor<type>::statu(void) const
{
  return _statu;
}


template <MotorType type>
Status DjiMotor<type>::DataUnpack(CanRxMsg rx)
{
  if ((_bus == nullptr) || (_can_item == nullptr))
  {
    _statu = Status::NOT_INIT;
    return _statu;
  }

  if ((rx.header.Identifier != feedback_std_id(_motor_id)) ||
      (rx.header.IdType != FDCAN_STANDARD_ID) ||
      (rx.header.RxFrameType != FDCAN_DATA_FRAME) ||
      (rx.header.DataLength != FDCAN_DLC_BYTES_8))
  {
    _statu = Status::BAD_ARG;
    return _statu;
  }

  const int16_t ecd     = static_cast<int16_t>((static_cast<uint16_t>(rx.data[0]) << 8U) | rx.data[1]);
  const int16_t rpm     = read_i16_be(&rx.data[2]);
  const int16_t current = read_i16_be(&rx.data[4]);
  const bool had_previous_feedback = _feedback_ready;

  if (!_feedback_ready)
  {
    int32_t initial_delta = static_cast<int32_t>(ecd) - _data.param.ecdOffset;
    if (initial_delta > WRAP_THRESHOLD)
    {
      initial_delta -= ENCODER_COUNTS;
    }
    else if (initial_delta < -WRAP_THRESHOLD)
    {
      initial_delta += ENCODER_COUNTS;
    }
    _total_ecd      = initial_delta;
    _feedback_ready = true;
  }
  else
  {
    int32_t delta = static_cast<int32_t>(ecd) - _last_ecd;
    if (delta > WRAP_THRESHOLD)
    {
      delta -= ENCODER_COUNTS;
    }
    else if (delta < -WRAP_THRESHOLD)
    {
      delta += ENCODER_COUNTS;
    }
    _total_ecd += delta;
  }
  _last_ecd = ecd;

  _data.rawData.ecd           = ecd;
  _data.rawData.rpm           = rpm;
  _data.rawData.TorqueCurrent = current;
  _data.rawData.Temperature   = (type == Motor2006) ? 0U : rx.data[6];

  const float velocity = static_cast<float>(rpm) * RPM_TO_RAD_PER_SEC / _data.param.ratio;
  const float angle    = static_cast<float>(_total_ecd) * TWO_PI /
                      (static_cast<float>(ENCODER_COUNTS) * _data.param.ratio);
  float single_angle = fmodf(angle, TWO_PI);
  if (single_angle < 0.0f)
  {
    single_angle += TWO_PI;
  }

  _data.radianData.angle_SingleRound = single_angle;
  _data.radianData.angle_MultiRound  = angle;
  _data.radianData.velocity          = velocity;
  _data.radianData.acceleration      = had_previous_feedback
                                         ? (velocity - _last_velocity) * 1000.0f
                                         : 0.0f;
  _last_velocity = velocity;

  _online.refresh_task();
  _statu = Status::OK;
  return _statu;
}


template <MotorType type>
Status DjiMotor<type>::FillData(int16_t output)
{
  if (_bus == nullptr)
  {
    _statu = Status::NOT_INIT;
    return _statu;
  }

  const int16_t limit = output_limit(_control_mode);
  if (output > limit)
  {
    output = limit;
  }
  else if (output < -limit)
  {
    output = static_cast<int16_t>(-limit);
  }

  _statu = _bus->setOutput(_bus_slot, output);
  return _statu;
}


template <MotorType type>
bool DjiMotor<type>::MotorIDValid(uint8_t motor_id)
{
  if (type == Motor6020)
  {
    return (motor_id >= 1U) && (motor_id <= 7U);
  }
  return (motor_id >= 1U) && (motor_id <= 8U);
}


template <MotorType type>
uint32_t DjiMotor<type>::ControlStdID(uint8_t motor_id,
                                     DjiMotorControlMode control_mode)
{
  if (type == Motor6020)
  {
    if (control_mode == DjiMotorControlMode::CURRENT)
    {
      return (motor_id <= 4U) ? 0x1FEU : 0x2FEU;
    }
    return (motor_id <= 4U) ? 0x1FFU : 0x2FFU;
  }
  return (motor_id <= 4U) ? 0x200U : 0x1FFU;
}


template <MotorType type>
uint8_t DjiMotor<type>::ControlSlot(uint8_t motor_id)
{
  return static_cast<uint8_t>((motor_id - 1U) % 4U);
}


template <MotorType type>
int16_t DjiMotor<type>::output_limit(DjiMotorControlMode control_mode)
{
  if (type == Motor3508)
  {
    return CURRENT_LIMIT_FOR_3508;
  }
  if (type == Motor2006)
  {
    return CURRENT_LIMIT_FOR_2006;
  }
  return (control_mode == DjiMotorControlMode::CURRENT)
           ? CURRENT_LIMIT_FOR_6020
           : VOLTAGE_LIMIT_FOR_6020;
}


template <MotorType type>
uint32_t DjiMotor<type>::feedback_std_id(uint8_t motor_id)
{
  return ((type == Motor6020) ? 0x204U : 0x200U) + motor_id;
}


template <MotorType type>
void DjiMotor<type>::register_motor(void)
{
  const ScopedTaskCritical lock;
  if (_tail == nullptr)
  {
    _head = this;
    _tail = this;
  }
  else
  {
    _tail->_next = this;
    _tail        = this;
  }
}


template <MotorType type>
void DjiMotor<type>::unregister_motor(void)
{
  const ScopedTaskCritical lock;
  DjiMotor *previous = nullptr;
  DjiMotor *current  = _head;
  while ((current != nullptr) && (current != this))
  {
    previous = current;
    current  = current->_next;
  }

  if (current == this)
  {
    if (previous == nullptr)
    {
      _head = _next;
    }
    else
    {
      previous->_next = _next;
    }
    if (_tail == this)
    {
      _tail = previous;
    }
  }
  _next = nullptr;
}


template <MotorType type>
Status DjiMotor<type>::dispatch(BspCan &can_item, const CanRxMsg &rx)
{
  for (DjiMotor *motor = _head; motor != nullptr; motor = motor->_next)
  {
    if ((motor->_can_item == &can_item) &&
        (rx.header.Identifier == feedback_std_id(motor->_motor_id)))
    {
      return motor->DataUnpack(rx);
    }
  }
  return Status::BAD_ARG;
}


template <MotorType type>
bool DjiMotor<type>::uses_can(const BspCan &can_item)
{
  for (DjiMotor *motor = _head; motor != nullptr; motor = motor->_next)
  {
    if (motor->_can_item == &can_item)
    {
      return true;
    }
  }
  return false;
}


template <MotorType type>
bool DjiMotor<type>::feedback_in_use(const BspCan &can_item, uint32_t feedback_identifier)
{
  for (DjiMotor *motor = _head; motor != nullptr; motor = motor->_next)
  {
    if ((motor->_can_item == &can_item) &&
        (feedback_std_id(motor->_motor_id) == feedback_identifier))
    {
      return true;
    }
  }
  return false;
}


template <MotorType type>
Status DjiMotor<type>::LvboDataUpdate(void)
{
  return Status::NOT_SUPPORTED;
}


bool dji_motor_uses_can(const BspCan &can_item)
{
  return DjiMotor<Motor3508>::uses_can(can_item) ||
         DjiMotor<Motor6020>::uses_can(can_item) ||
         DjiMotor<Motor2006>::uses_can(can_item);
}


Status dji_motor_dispatch_rx(BspCan &can_item, const CanRxMsg &rx)
{
  Status result = DjiMotor<Motor3508>::dispatch(can_item, rx);
  if (result == Status::OK)
  {
    return result;
  }

  result = DjiMotor<Motor6020>::dispatch(can_item, rx);
  if (result == Status::OK)
  {
    return result;
  }

  return DjiMotor<Motor2006>::dispatch(can_item, rx);
}


bool dji_motor_feedback_in_use(const BspCan &can_item, uint32_t feedback_std_id)
{
  return DjiMotor<Motor3508>::feedback_in_use(can_item, feedback_std_id) ||
         DjiMotor<Motor6020>::feedback_in_use(can_item, feedback_std_id) ||
         DjiMotor<Motor2006>::feedback_in_use(can_item, feedback_std_id);
}


template class DjiMotor<Motor3508>;
template class DjiMotor<Motor6020>;
template class DjiMotor<Motor2006>;
