#include "app_test.hpp"

#include "FreeRTOS.h" // IWYU pragma: keep
#include "bsp_cfg.hpp"
#include "dji_motor.hpp"
#include "task.h"

#include <stdint.h>


#if APP_TEST_DJI_MOTOR_ENABLED

DjiMotor<Motor3508> dji_motor_test_motor(bsp_can1, 2U);

volatile uint32_t dji_motor_test_stage = 0U;
volatile uint32_t dji_motor_test_passed = 0U;
volatile Status dji_motor_test_object_status = Status::NOT_INIT;
volatile Status dji_motor_test_fill_status = Status::NOT_INIT;
volatile Status dji_motor_test_online_status = Status::NOT_INIT;
volatile int16_t dji_motor_test_speed_rpm = 0;
volatile int16_t dji_motor_test_given_current = 0;
volatile uint8_t dji_motor_test_temperature = 0U;
volatile uint16_t dji_motor_test_peak_abs_speed_rpm = 0U;


namespace
{
void sample_motor_feedback(void)
{
  const MotorData &data = dji_motor_test_motor.data();
  dji_motor_test_speed_rpm      = data.rawData.rpm;
  dji_motor_test_given_current  = data.rawData.TorqueCurrent;
  dji_motor_test_temperature    = data.rawData.Temperature;
  dji_motor_test_online_status  = dji_motor_test_motor.online().isOnline();

  const int32_t speed = data.rawData.rpm;
  const uint16_t absolute_speed =
    static_cast<uint16_t>(speed < 0 ? -speed : speed);
  if (absolute_speed > dji_motor_test_peak_abs_speed_rpm)
  {
    dji_motor_test_peak_abs_speed_rpm = absolute_speed;
  }
}
} // namespace


extern "C" void dji_motor_test_task(void *argument)
{
  (void)argument; // 测试任务不需要外部参数。
  constexpr int16_t TEST_CURRENT = 512; // 约 0.625 A

  dji_motor_test_object_status = dji_motor_test_motor.statu();
  dji_motor_test_fill_status = dji_motor_test_motor.FillData(0);
  dji_motor_test_stage = 1U;

  for (uint32_t elapsed = 0U; elapsed < 2000U; ++elapsed)
  {
    sample_motor_feedback();
    vTaskDelay(pdMS_TO_TICKS(1U));
  }

  dji_motor_test_fill_status = dji_motor_test_motor.FillData(TEST_CURRENT);
  dji_motor_test_stage = 2U;
  for (uint32_t elapsed = 0U; elapsed < 1000U; ++elapsed)
  {
    sample_motor_feedback();
    vTaskDelay(pdMS_TO_TICKS(1U));
  }

  dji_motor_test_fill_status = dji_motor_test_motor.FillData(0);
  dji_motor_test_stage = 3U;
  for (uint32_t elapsed = 0U; elapsed < 100U; ++elapsed)
  {
    sample_motor_feedback();
    vTaskDelay(pdMS_TO_TICKS(1U));
  }

  dji_motor_test_passed =
    ((dji_motor_test_object_status == Status::OK) &&
     (dji_motor_test_fill_status == Status::OK) &&
     (dji_motor_test_online_status == Status::OK) &&
     (dji_motor_test_peak_abs_speed_rpm > 0U))
      ? 1U
      : 0U;
  dji_motor_test_stage = 4U;

  // 输出保持为零；挂起测试任务，DJI 维护任务继续周期发送安全零指令。
  vTaskSuspend(nullptr);
}

#endif // APP_TEST_DJI_MOTOR_ENABLED
