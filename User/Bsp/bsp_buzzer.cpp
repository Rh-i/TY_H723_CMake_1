#include "bsp_buzzer.hpp"


/* ==================== 构造与初始化 ==================== */

BspBuzzer::BspBuzzer(const Config &cfg)
  : _config(cfg)
{
}

void BspBuzzer::init(const Config &cfg)
{
  _config = cfg;
}


/* ==================== 公共接口 ==================== */

/**
 * @brief 以指定频率和音量启动 PWM
 *
 * @note 频率公式：freq = base_clk / (ARR + 1)
 *       → ARR = base_clk / freq - 1
 *
 * @param freq_hz    频率 (Hz)，自动限幅到 Config::freq_min ~ freq_max
 * @param volume_pct 占空比 (%)
 */
void BspBuzzer::tone(uint32_t freq_hz, uint32_t volume_pct)
{
  if (freq_hz < _config.freq_min) freq_hz = _config.freq_min;
  if (freq_hz > _config.freq_max) freq_hz = _config.freq_max;
  if (volume_pct > 100) volume_pct = 100;

  uint32_t arr = (_config.base_clk / freq_hz) - 1;
  if (arr < 10) arr = 10;     // 防 0/过小（避免产生超出可听范围的怪声）
  if (arr > 65535) arr = 65535; // 16 位 ARR 寄存器硬件上限

  set_arr_and_ccr(arr, volume_pct);
  HAL_TIM_PWM_Start(_config.htim, _config.channel);
}


/**
 * @brief 停止 PWM
 */
void BspBuzzer::off()
{
  HAL_TIM_PWM_Stop(_config.htim, _config.channel);
}


/**
 * @brief 鸣叫指定时长后自动关闭（阻塞）
 * @param freq_hz     频率 (Hz)
 * @param duration_ms 时长 (ms)
 */
void BspBuzzer::beep(uint32_t freq_hz, uint32_t duration_ms)
{
  tone(freq_hz);
  vTaskDelay(pdMS_TO_TICKS(duration_ms));
  off();
}


/**
 * @brief 短鸣（通过提示）
 */
void BspBuzzer::beep_pass_short()
{
  beep(_config.default_freq, _config.short_ms);
}


/**
 * @brief 长鸣（开始提示）
 */
void BspBuzzer::beep_start_long()
{
  beep(_config.default_freq, _config.long_ms);
  vTaskDelay(pdMS_TO_TICKS(_config.gap_ms));
}


/**
 * @brief 三短鸣（失败提示）
 */
void BspBuzzer::beep_fail()
{
  for (int i = 0; i < 3; i++)
  {
    beep(_config.default_freq, _config.short_ms);
    vTaskDelay(pdMS_TO_TICKS(_config.gap_ms));
  }
}


/* ==================== 私有方法 ==================== */

/**
 * @brief 设置自动重装载值和比较值
 * @param arr        自动重装载值
 * @param volume_pct 占空比 (%)
 */
void BspBuzzer::set_arr_and_ccr(uint32_t arr, uint32_t volume_pct)
{
  __HAL_TIM_SET_AUTORELOAD(_config.htim, arr);
  __HAL_TIM_SET_COMPARE(_config.htim, _config.channel, arr * volume_pct / 100);
}
