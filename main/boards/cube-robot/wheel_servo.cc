/*
    wheel_servo.cc
    Otto HP Robot Wheel Servo Implementation
    Implements continuous rotation servo control for wheels
    Author: Xumx
    Date: 2024-08-15
    Version: 1.0
*/

#include "wheel_servo.h"

#include <driver/ledc.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <algorithm>

#define TAG "WheelServo"

// 连续旋转舵机参数
#define SERVO_FREQ 50                           // 50Hz PWM频率
#define SERVO_PWM_RESOLUTION LEDC_TIMER_13_BIT  // 13位分辨率
#define SERVO_PWM_MAX_DUTY 8191                 // 2^13 - 1

// 连续旋转舵机脉宽范围
#define SERVO_STOP_PULSEWIDTH_US 1500  // 停止脉宽 (1.5ms)
#define SERVO_MIN_PULSEWIDTH_US 1000   // 最小脉宽 (1.0ms, 全速反转)
#define SERVO_MAX_PULSEWIDTH_US 2000   // 最大脉宽 (2.0ms, 全速正转)

WheelServo::WheelServo(int pin, int channel, int timer)
    : _pin(pin),
      _channel(channel),
      _timer(timer),
      _currentSpeed(0),
      _initialized(false) {}

WheelServo::~WheelServo() {
  if (_initialized) {
    stop();
    ledc_stop(LEDC_LOW_SPEED_MODE, static_cast<ledc_channel_t>(_channel), 0);
  }
}

bool WheelServo::init() {
  if (_initialized) {
    ESP_LOGW(TAG, "WheelServo already initialized on channel %d", _channel);
    return true;
  }

  ESP_LOGI(TAG, "Initializing WheelServo on GPIO %d, Channel %d, Timer %d",
           _pin, _channel, _timer);

  // 配置LEDC定时器（每个舵机使用独立的定时器）
  ESP_LOGI(TAG, "Configuring LEDC Timer %d for servo control...", _timer);

  ledc_timer_config_t ledc_timer = {};
  ledc_timer.speed_mode = LEDC_LOW_SPEED_MODE;
  ledc_timer.duty_resolution = SERVO_PWM_RESOLUTION;
  ledc_timer.timer_num = static_cast<ledc_timer_t>(_timer);
  ledc_timer.freq_hz = SERVO_FREQ;
  ledc_timer.clk_cfg = LEDC_AUTO_CLK;

  esp_err_t ret = ledc_timer_config(&ledc_timer);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure LEDC timer %d: %s", _timer,
             esp_err_to_name(ret));
    return false;
  }
  ESP_LOGI(TAG, "✓ LEDC Timer %d configured: 50Hz, 13-bit resolution", _timer);

  // 配置LEDC通道
  ESP_LOGI(TAG, "Configuring LEDC Channel %d on GPIO %d...", _channel, _pin);

  ledc_channel_config_t ledc_channel = {};
  ledc_channel.gpio_num = _pin;
  ledc_channel.speed_mode = LEDC_LOW_SPEED_MODE;
  ledc_channel.channel = static_cast<ledc_channel_t>(_channel);
  ledc_channel.intr_type = LEDC_INTR_DISABLE;
  ledc_channel.timer_sel =
      static_cast<ledc_timer_t>(_timer);  // 使用自己的Timer
  ledc_channel.duty = 0;                  // 初始duty为0
  ledc_channel.hpoint = 0;

  ret = ledc_channel_config(&ledc_channel);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure LEDC channel on pin %d: %s", _pin,
             esp_err_to_name(ret));
    ESP_LOGE(TAG, "  → Channel=%d, Timer=%d, GPIO=%d", _channel, _timer, _pin);
    return false;
  }

  ESP_LOGI(TAG, "✓ LEDC Channel %d configured: GPIO=%d, Timer=%d", _channel,
           _pin, _timer);

  // ========== 验证配置 ==========
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "🔍 验证 LEDC 配置:");
  ESP_LOGI(TAG, "  → GPIO: %d", _pin);
  ESP_LOGI(TAG, "  → Channel: %d", _channel);
  ESP_LOGI(TAG, "  → Timer: %d", _timer);
  ESP_LOGI(TAG, "  → Timer Freq: 50Hz");
  ESP_LOGI(TAG, "  → Resolution: 13-bit (0-8191)");
  ESP_LOGI(TAG, "");

  _initialized = true;
  _currentSpeed = 0;

  // 设置初始停止状态
  stop();

  ESP_LOGI(TAG, "✓ WheelServo initialized successfully on GPIO %d, Channel %d",
           _pin, _channel);
  return true;
}

void WheelServo::setSpeed(int speed) {
  if (!_initialized) {
    ESP_LOGW(TAG, "WheelServo not initialized");
    return;
  }

  // 限制速度范围 -100 到 +100
  speed = std::max(-100, std::min(100, speed));
  _currentSpeed = speed;

  unsigned int duty = speedToDuty(speed);

  ESP_LOGI(TAG, "🔧 [GPIO %d Ch %d T %d] Setting speed=%d, duty=%u", _pin,
           _channel, _timer, speed, duty);

  esp_err_t ret = ledc_set_duty(LEDC_LOW_SPEED_MODE,
                                static_cast<ledc_channel_t>(_channel), duty);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "❌ ledc_set_duty FAILED for pin %d ch %d: %s", _pin,
             _channel, esp_err_to_name(ret));
    return;
  }

  ret = ledc_update_duty(LEDC_LOW_SPEED_MODE,
                         static_cast<ledc_channel_t>(_channel));
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "❌ ledc_update_duty FAILED for pin %d ch %d: %s", _pin,
             _channel, esp_err_to_name(ret));
    return;
  }

  ESP_LOGI(TAG, "✅ [GPIO %d Ch %d T %d] PWM updated: speed=%d, duty=%u", _pin,
           _channel, _timer, speed, duty);
}

void WheelServo::stop() { setSpeed(0); }

unsigned int WheelServo::speedToDuty(int speed) {
  // SG90S 连续旋转舵机:
  // 1.5ms (90度) = 停止
  // 1.0ms (0度) = 最大反向速度
  // 2.0ms (180度) = 最大正向速度

  // 对于50Hz信号，周期 = 20ms
  // 占空比 = (脉宽 / 周期) * 最大占空比
  // 最大占空比 = 2^13 - 1 = 8191

  float pulse_ms;
  if (speed == 0) {
    pulse_ms = 1.5f;  // 停止 (1500us)
  } else if (speed > 0) {
    // 正向: 1.5ms ~ 2.0ms
    // speed=100 -> pulse=2.0ms
    // speed=50  -> pulse=1.75ms
    pulse_ms = 1.5f + (speed / 100.0f) * 0.5f;
  } else {
    // 反向: 1.0ms ~ 1.5ms
    // speed=-100 -> pulse=1.0ms
    // speed=-50  -> pulse=1.25ms
    // 注意：speed 已经是负数，所以直接相加
    pulse_ms = 1.5f + (speed / 100.0f) * 0.5f;
  }

  // 转换为占空比
  // duty_cycle = pulse_ms / 20.0ms
  float duty_cycle = pulse_ms / 20.0f;
  unsigned int duty = (unsigned int)(duty_cycle * SERVO_PWM_MAX_DUTY);

  // 简化日志，减少输出
  // ESP_LOGI(TAG, "Speed %d -> Pulse %.3fms -> Duty %u", speed, pulse_ms,
  // duty);
  return duty;
}

void WheelServo::runHardwareDiagnostic() {
  if (!_initialized) {
    ESP_LOGE(TAG, "Cannot run diagnostic: servo not initialized");
    return;
  }

  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "▶▶▶ GPIO %d 硬件诊断测试（直接PWM） ◀◀◀", _pin);
  ESP_LOGI(TAG, "");

  // 测试1: 停止位 (1.5ms)
  ESP_LOGI(TAG, "[1/5] Duty=614 (1.5ms停止) - 持续2秒");
  ledc_set_duty(LEDC_LOW_SPEED_MODE, static_cast<ledc_channel_t>(_channel),
                614);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, static_cast<ledc_channel_t>(_channel));
  vTaskDelay(pdMS_TO_TICKS(2000));

  // 测试2: 最大正转 (2.0ms)
  ESP_LOGI(TAG, "[2/5] Duty=819 (2.0ms满速正转) - 持续2秒");
  ESP_LOGI(TAG, "      → 舵机应该开始旋转!");
  ledc_set_duty(LEDC_LOW_SPEED_MODE, static_cast<ledc_channel_t>(_channel),
                819);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, static_cast<ledc_channel_t>(_channel));
  vTaskDelay(pdMS_TO_TICKS(2000));

  // 测试3: 返回停止
  ESP_LOGI(TAG, "[3/5] Duty=614 (1.5ms停止) - 持续2秒");
  ledc_set_duty(LEDC_LOW_SPEED_MODE, static_cast<ledc_channel_t>(_channel),
                614);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, static_cast<ledc_channel_t>(_channel));
  vTaskDelay(pdMS_TO_TICKS(2000));

  // 测试4: 最大反转 (1.0ms)
  ESP_LOGI(TAG, "[4/5] Duty=409 (1.0ms满速反转) - 持续2秒");
  ESP_LOGI(TAG, "      → 舵机应该反向旋转!");
  ledc_set_duty(LEDC_LOW_SPEED_MODE, static_cast<ledc_channel_t>(_channel),
                409);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, static_cast<ledc_channel_t>(_channel));
  vTaskDelay(pdMS_TO_TICKS(2000));

  // 测试5: 最终停止
  ESP_LOGI(TAG, "[5/5] Duty=614 (1.5ms停止)");
  ledc_set_duty(LEDC_LOW_SPEED_MODE, static_cast<ledc_channel_t>(_channel),
                614);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, static_cast<ledc_channel_t>(_channel));

  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "✓ GPIO %d 硬件诊断测试完成", _pin);
  ESP_LOGI(TAG, "");

  stop();
}
