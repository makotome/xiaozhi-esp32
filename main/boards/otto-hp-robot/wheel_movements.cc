/*
    wheel_movements.cc
    Otto HP Robot Wheel Movements Implementation
    Implements high-level movement control for the wheeled Otto robot
    Author: Xumx
    Date: 2024-08-15
    Version: 1.0
*/

#include "wheel_movements.h"
#include "config.h"
#include <driver/ledc.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <algorithm>

#define TAG "WheelMovements"

// 轮子舵机通道和定时器定义
// 注意: LEDC_CHANNEL_0 被背光使用, LEDC_CHANNEL_1 可能被 Oscillator 使用
#define LEFT_WHEEL_CHANNEL LEDC_CHANNEL_2  // 左轮使用 Channel 2
#define RIGHT_WHEEL_CHANNEL LEDC_CHANNEL_3 // 右轮使用 Channel 3
#define LEFT_WHEEL_TIMER LEDC_TIMER_2      // 左轮使用 Timer 2
#define RIGHT_WHEEL_TIMER LEDC_TIMER_3     // 右轮使用 Timer 3

// 默认速度参数
#define DEFAULT_SPEED 50
#define MIN_SPEED 0
#define MAX_SPEED 100

WheelMovements::WheelMovements()
    : _leftWheel(nullptr), _rightWheel(nullptr), _initialized(false)
{
}
WheelMovements::~WheelMovements()
{
    if (_initialized)
    {
        stopAll();
        delete _leftWheel;
        delete _rightWheel;
        _leftWheel = nullptr;
        _rightWheel = nullptr;
    }
}

bool WheelMovements::init()
{
    if (_initialized)
    {
        ESP_LOGW(TAG, "WheelMovements already initialized");
        return true;
    }

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║    初始化 WheelMovements (双轮独立Timer配置)      ║");
    ESP_LOGI(TAG, "╚════════════════════════════════════════════════════╝");
    ESP_LOGI(TAG, "");

    // 创建左轮舵机（使用 LEFT_LEG_PIN=GPIO17, Channel 0, Timer 2）
    ESP_LOGI(TAG, "→ 创建左轮: GPIO=%d, Channel=%d, Timer=%d",
             LEFT_LEG_PIN, LEFT_WHEEL_CHANNEL, LEFT_WHEEL_TIMER);
    _leftWheel = new WheelServo(LEFT_LEG_PIN, LEFT_WHEEL_CHANNEL, LEFT_WHEEL_TIMER);
    if (!_leftWheel || !_leftWheel->init())
    {
        ESP_LOGE(TAG, "❌ Failed to initialize left wheel");
        delete _leftWheel;
        _leftWheel = nullptr;
        return false;
    }
    ESP_LOGI(TAG, "✅ 左轮初始化成功");
    ESP_LOGI(TAG, "");

    // 创建右轮舵机（使用 LEFT_FOOT_PIN=GPIO18, Channel 1, Timer 3）
    ESP_LOGI(TAG, "→ 创建右轮: GPIO=%d, Channel=%d, Timer=%d",
             LEFT_FOOT_PIN, RIGHT_WHEEL_CHANNEL, RIGHT_WHEEL_TIMER);
    _rightWheel = new WheelServo(LEFT_FOOT_PIN, RIGHT_WHEEL_CHANNEL, RIGHT_WHEEL_TIMER);
    if (!_rightWheel || !_rightWheel->init())
    {
        ESP_LOGE(TAG, "❌ Failed to initialize right wheel");
        delete _leftWheel;
        delete _rightWheel;
        _leftWheel = nullptr;
        _rightWheel = nullptr;
        return false;
    }
    ESP_LOGI(TAG, "✅ 右轮初始化成功");
    ESP_LOGI(TAG, "");

    _initialized = true;
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║    WheelMovements 初始化完成                       ║");
    ESP_LOGI(TAG, "║    左轮: GPIO17, Ch2, Timer2                       ║");
    ESP_LOGI(TAG, "║    右轮: GPIO18, Ch3, Timer3                       ║");
    ESP_LOGI(TAG, "╚════════════════════════════════════════════════════╝");
    ESP_LOGI(TAG, "");

    // 初始停止状态
    stopAll();
    return true;
}

void WheelMovements::moveForward(int speed)
{
    if (!_initialized)
    {
        ESP_LOGW(TAG, "WheelMovements not initialized");
        return;
    }

    // 限制速度范围
    speed = std::max(MIN_SPEED, std::min(MAX_SPEED, speed));

    ESP_LOGI(TAG, "Moving forward at speed %d", speed);
    ESP_LOGI(TAG, "  → Left wheel: +%d, Right wheel: -%d (mirrored to forward)", speed, speed);

    // 两轮同向前进
    // 注意：根据舵机安装方向，可能需要调整正负号
    _leftWheel->setSpeed(speed);
    _rightWheel->setSpeed(-speed); // 右轮反向（因为镜像安装）
}

void WheelMovements::moveBackward(int speed)
{
    if (!_initialized)
    {
        ESP_LOGW(TAG, "WheelMovements not initialized");
        return;
    }

    // 限制速度范围
    speed = std::max(MIN_SPEED, std::min(MAX_SPEED, speed));

    ESP_LOGI(TAG, "Moving backward at speed %d", speed);
    ESP_LOGI(TAG, "  → Left wheel: -%d, Right wheel: +%d (mirrored to backward)", speed, speed);

    // 两轮同向后退
    _leftWheel->setSpeed(-speed);
    _rightWheel->setSpeed(speed); // 右轮反向（因为镜像安装）
}

void WheelMovements::stopAll()
{
    if (!_initialized)
    {
        return;
    }

    _leftWheel->stop();
    _rightWheel->stop();

    ESP_LOGI(TAG, "Stopped all wheels");
}

void WheelMovements::turnLeft(int speed)
{
    if (!_initialized)
    {
        ESP_LOGW(TAG, "WheelMovements not initialized");
        return;
    }

    // 限制速度范围
    speed = std::max(MIN_SPEED, std::min(MAX_SPEED, speed));

    ESP_LOGI(TAG, "Turning left at speed %d (spin in place)", speed);
    ESP_LOGI(TAG, "  → Logical: Left=-%d (backward), Right=+%d (forward)", speed, speed);
    ESP_LOGI(TAG, "  → After mirror: both servos will get -%d PWM signal", speed);

    // 左转：左轮后退，右轮前进（原地转向）
    // setWheelSpeeds会对右轮进行镜像转换：-rightSpeed
    // 所以传入right=50会变成-50，这对镜像安装的右轮来说是前进
    setWheelSpeeds(-speed, speed);
}

void WheelMovements::turnRight(int speed)
{
    if (!_initialized)
    {
        ESP_LOGW(TAG, "WheelMovements not initialized");
        return;
    }

    // 限制速度范围
    speed = std::max(MIN_SPEED, std::min(MAX_SPEED, speed));

    ESP_LOGI(TAG, "Turning right at speed %d (spin in place)", speed);
    ESP_LOGI(TAG, "  → Logical: Left=+%d (forward), Right=-%d (backward)", speed, speed);
    ESP_LOGI(TAG, "  → After mirror: both servos will get +%d PWM signal", speed);

    // 右转：左轮前进，右轮后退（原地转向）
    // setWheelSpeeds会对右轮进行镜像转换：-rightSpeed
    // 所以传入right=-50会变成+50，这对镜像安装的右轮来说是后退
    setWheelSpeeds(speed, -speed);
}

void WheelMovements::accelerate(int targetSpeed, int duration_ms)
{
    if (!_initialized)
    {
        ESP_LOGW(TAG, "WheelMovements not initialized");
        return;
    }

    // 限制目标速度
    targetSpeed = std::max(MIN_SPEED, std::min(MAX_SPEED, targetSpeed));

    // 获取当前速度（取左轮速度作为参考）
    int currentSpeed = std::abs(_leftWheel->getSpeed());

    if (targetSpeed <= currentSpeed)
    {
        ESP_LOGW(TAG, "Target speed %d is not greater than current speed %d",
                 targetSpeed, currentSpeed);
        return;
    }

    // 计算加速步数
    int speedDiff = targetSpeed - currentSpeed;
    int steps = duration_ms / 50; // 每50ms更新一次
    if (steps < 1)
        steps = 1;

    int speedIncrement = speedDiff / steps;
    if (speedIncrement < 1)
        speedIncrement = 1;

    ESP_LOGI(TAG, "Accelerating from %d to %d over %dms",
             currentSpeed, targetSpeed, duration_ms);

    // 渐进加速
    for (int speed = currentSpeed; speed < targetSpeed; speed += speedIncrement)
    {
        moveForward(speed);
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    // 确保达到目标速度
    moveForward(targetSpeed);
}

void WheelMovements::decelerate(int duration_ms)
{
    if (!_initialized)
    {
        ESP_LOGW(TAG, "WheelMovements not initialized");
        return;
    }

    // 获取当前速度（取左轮速度作为参考）
    int currentSpeed = std::abs(_leftWheel->getSpeed());

    if (currentSpeed == 0)
    {
        ESP_LOGW(TAG, "Already stopped");
        return;
    }

    // 计算减速步数
    int steps = duration_ms / 50; // 每50ms更新一次
    if (steps < 1)
        steps = 1;

    int speedDecrement = currentSpeed / steps;
    if (speedDecrement < 1)
        speedDecrement = 1;

    ESP_LOGI(TAG, "Decelerating from %d to 0 over %dms", currentSpeed, duration_ms);

    // 渐进减速
    for (int speed = currentSpeed; speed > 0; speed -= speedDecrement)
    {
        moveForward(speed);
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    // 完全停止
    stopAll();
}

void WheelMovements::setWheelSpeeds(int leftSpeed, int rightSpeed)
{
    if (!_initialized)
    {
        ESP_LOGW(TAG, "WheelMovements not initialized");
        return;
    }

    // 限制速度范围 -100 到 +100
    leftSpeed = std::max(-MAX_SPEED, std::min(MAX_SPEED, leftSpeed));
    rightSpeed = std::max(-MAX_SPEED, std::min(MAX_SPEED, rightSpeed));

    ESP_LOGI(TAG, "Set wheel speeds: left=%d, right=%d (logical values)", leftSpeed, rightSpeed);

    // 独立控制左右轮（考虑安装方向）
    _leftWheel->setSpeed(leftSpeed);
    _rightWheel->setSpeed(-rightSpeed); // 右轮镜像安装

    ESP_LOGI(TAG, "  → Actual servo commands: left=%d, right=%d (after mirror)", leftSpeed, -rightSpeed);
}

int WheelMovements::getLeftSpeed() const
{
    if (!_initialized || !_leftWheel)
    {
        return 0;
    }
    return _leftWheel->getSpeed();
}

int WheelMovements::getRightSpeed() const
{
    if (!_initialized || !_rightWheel)
    {
        return 0;
    }
    // 返回实际右轮速度（考虑镜像安装）
    return -_rightWheel->getSpeed();
}

void WheelMovements::runHardwareDiagnostics()
{
    if (!_initialized)
    {
        ESP_LOGW(TAG, "WheelMovements not initialized");
        return;
    }

    ESP_LOGI(TAG, "Running hardware diagnostics on left wheel...");
    _leftWheel->runHardwareDiagnostic();

    ESP_LOGI(TAG, "Running hardware diagnostics on right wheel...");
    _rightWheel->runHardwareDiagnostic();
}