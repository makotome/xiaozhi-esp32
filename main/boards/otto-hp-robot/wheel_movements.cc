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

// ==================== 跳舞动作实现 ====================

void WheelMovements::danceShake()
{
    if (!_initialized)
    {
        ESP_LOGW(TAG, "WheelMovements not initialized");
        return;
    }

    ESP_LOGI(TAG, "🕺 Dance: Shake - 左右快速摇摆");

    // 摇摆舞：节奏感强的左右摆动，带速度变化
    // 第一轮：慢速热身摇摆
    for (int i = 0; i < 3; i++)
    {
        turnLeft(50);
        vTaskDelay(pdMS_TO_TICKS(300));
        turnRight(50);
        vTaskDelay(pdMS_TO_TICKS(300));
    }

    // 第二轮：加速摇摆，更有活力
    for (int i = 0; i < 4; i++)
    {
        turnLeft(75);
        vTaskDelay(pdMS_TO_TICKS(250));
        turnRight(75);
        vTaskDelay(pdMS_TO_TICKS(250));
    }

    // 第三轮：超快节奏摇摆
    for (int i = 0; i < 5; i++)
    {
        turnLeft(85);
        vTaskDelay(pdMS_TO_TICKS(180));
        turnRight(85);
        vTaskDelay(pdMS_TO_TICKS(180));
    }

    // 结束动作：大幅度摆动后急停
    turnLeft(90);
    vTaskDelay(pdMS_TO_TICKS(400));
    turnRight(90);
    vTaskDelay(pdMS_TO_TICKS(400));

    stopAll();
    ESP_LOGI(TAG, "✅ Dance Shake completed");
}

void WheelMovements::danceSpin()
{
    if (!_initialized)
    {
        ESP_LOGW(TAG, "WheelMovements not initialized");
        return;
    }

    ESP_LOGI(TAG, "🕺 Dance: Spin - 360度旋转");

    // 旋转舞：多变的旋转组合，包含左右旋转
    // 第一段：右旋加速
    for (int speed = 25; speed <= 80; speed += 11)
    {
        turnRight(speed);
        vTaskDelay(pdMS_TO_TICKS(250));
    }

    // 保持高速右旋
    turnRight(85);
    vTaskDelay(pdMS_TO_TICKS(800));

    // 急停
    stopAll();
    vTaskDelay(pdMS_TO_TICKS(200));

    // 第二段：左旋加速（反向更有趣）
    for (int speed = 25; speed <= 80; speed += 11)
    {
        turnLeft(speed);
        vTaskDelay(pdMS_TO_TICKS(250));
    }

    // 保持高速左旋
    turnLeft(85);
    vTaskDelay(pdMS_TO_TICKS(800));

    // 急停
    stopAll();
    vTaskDelay(pdMS_TO_TICKS(200));

    // 第三段：快速左右交替旋转
    for (int i = 0; i < 3; i++)
    {
        turnRight(90);
        vTaskDelay(pdMS_TO_TICKS(300));
        turnLeft(90);
        vTaskDelay(pdMS_TO_TICKS(300));
    }

    // 最后减速旋转结束
    for (int speed = 70; speed >= 30; speed -= 13)
    {
        turnRight(speed);
        vTaskDelay(pdMS_TO_TICKS(250));
    }

    stopAll();
    ESP_LOGI(TAG, "✅ Dance Spin completed");
}

void WheelMovements::danceWave()
{
    if (!_initialized)
    {
        ESP_LOGW(TAG, "WheelMovements not initialized");
        return;
    }

    ESP_LOGI(TAG, "🕺 Dance: Wave - 波浪式前后移动");

    // 波浪舞：前后移动，速度呈波浪变化，加入左右摆动
    // 5个完整的波浪循环（原来3个）
    for (int wave = 0; wave < 5; wave++)
    {
        // 前进波浪：速度从慢到快到慢，加入轻微摆动
        for (int speed = 25; speed <= 75; speed += 12)
        {
            moveForward(speed);
            vTaskDelay(pdMS_TO_TICKS(150));
        }

        // 前进到最快时加入小幅度左右摆动
        setWheelSpeeds(70, 80); // 轻微右偏
        vTaskDelay(pdMS_TO_TICKS(120));
        setWheelSpeeds(80, 70); // 轻微左偏
        vTaskDelay(pdMS_TO_TICKS(120));

        for (int speed = 75; speed >= 25; speed -= 12)
        {
            moveForward(speed);
            vTaskDelay(pdMS_TO_TICKS(150));
        }

        // 短暂停顿，加入小动作
        stopAll();
        vTaskDelay(pdMS_TO_TICKS(150));
        turnLeft(30);
        vTaskDelay(pdMS_TO_TICKS(100));
        turnRight(30);
        vTaskDelay(pdMS_TO_TICKS(100));
        stopAll();
        vTaskDelay(pdMS_TO_TICKS(100));

        // 后退波浪：速度从慢到快到慢
        for (int speed = 25; speed <= 75; speed += 12)
        {
            moveBackward(speed);
            vTaskDelay(pdMS_TO_TICKS(150));
        }

        // 后退到最快时加入小幅度摆动
        setWheelSpeeds(-70, -80); // 后退时轻微左偏
        vTaskDelay(pdMS_TO_TICKS(120));
        setWheelSpeeds(-80, -70); // 后退时轻微右偏
        vTaskDelay(pdMS_TO_TICKS(120));

        for (int speed = 75; speed >= 25; speed -= 12)
        {
            moveBackward(speed);
            vTaskDelay(pdMS_TO_TICKS(150));
        }

        // 短暂停顿
        stopAll();
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    // 结束动作：快速前进后急停
    moveForward(85);
    vTaskDelay(pdMS_TO_TICKS(400));
    stopAll();

    ESP_LOGI(TAG, "✅ Dance Wave completed");
}

void WheelMovements::danceZigzag()
{
    if (!_initialized)
    {
        ESP_LOGW(TAG, "WheelMovements not initialized");
        return;
    }

    ESP_LOGI(TAG, "🕺 Dance: Zigzag - Z字形移动");

    // 之字舞：走更复杂的Z字形路线，加入速度变化和急转
    // 重复3次完整的Z字（原来2次）
    for (int i = 0; i < 3; i++)
    {
        // 第一段：加速向右前方移动（左轮快）
        for (int speed = 40; speed <= 70; speed += 15)
        {
            setWheelSpeeds(speed + 10, speed - 20); // 左快右慢 -> 右转前进
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        setWheelSpeeds(80, 50);
        vTaskDelay(pdMS_TO_TICKS(400));

        // 急转：原地右转来增加动感
        turnRight(75);
        vTaskDelay(pdMS_TO_TICKS(250));

        // 第二段：直线快速前进
        moveForward(75);
        vTaskDelay(pdMS_TO_TICKS(500));

        // 小幅度左右摆动增加趣味
        setWheelSpeeds(80, 60);
        vTaskDelay(pdMS_TO_TICKS(150));
        setWheelSpeeds(60, 80);
        vTaskDelay(pdMS_TO_TICKS(150));

        // 急转：原地左转
        turnLeft(75);
        vTaskDelay(pdMS_TO_TICKS(250));

        // 第三段：加速向左前方移动（右轮快）
        for (int speed = 40; speed <= 70; speed += 15)
        {
            setWheelSpeeds(speed - 20, speed + 10); // 左慢右快 -> 左转前进
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        setWheelSpeeds(50, 80);
        vTaskDelay(pdMS_TO_TICKS(400));

        // Z字完成，短暂停顿并加入小动作
        stopAll();
        vTaskDelay(pdMS_TO_TICKS(200));

        // 原地快速旋转180度，准备下一个Z字
        turnRight(85);
        vTaskDelay(pdMS_TO_TICKS(350));
        stopAll();
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    // 结束动作：快速前进后漂亮地停止
    moveForward(80);
    vTaskDelay(pdMS_TO_TICKS(400));
    stopAll();
    vTaskDelay(pdMS_TO_TICKS(100));

    // 最后小转身
    turnLeft(60);
    vTaskDelay(pdMS_TO_TICKS(200));

    stopAll();
    ESP_LOGI(TAG, "✅ Dance Zigzag completed");
}

void WheelMovements::danceMoonwalk()
{
    if (!_initialized)
    {
        ESP_LOGW(TAG, "WheelMovements not initialized");
        return;
    }

    ESP_LOGI(TAG, "🕺 Dance: Moonwalk - 太空步");

    // 太空步：模拟Michael Jackson的标志性动作
    // 后退时带有节奏感的停顿和加速，更多变化

    // 第一段：经典太空步节奏（重复5次，原来3次）
    for (int i = 0; i < 5; i++)
    {
        // 快速后退
        moveBackward(75);
        vTaskDelay(pdMS_TO_TICKS(450));

        // 突然停顿（关键动作）
        stopAll();
        vTaskDelay(pdMS_TO_TICKS(200));

        // 慢速后退（滑动感）
        moveBackward(35);
        vTaskDelay(pdMS_TO_TICKS(300));

        // 再次快速后退
        moveBackward(85);
        vTaskDelay(pdMS_TO_TICKS(400));

        // 停顿
        stopAll();
        vTaskDelay(pdMS_TO_TICKS(200));

        // 加入左右摆动（更有节奏感）
        turnLeft(50);
        vTaskDelay(pdMS_TO_TICKS(180));
        turnRight(50);
        vTaskDelay(pdMS_TO_TICKS(180));
        stopAll();
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // 第二段：加速后退组合
    for (int speed = 30; speed <= 80; speed += 16)
    {
        moveBackward(speed);
        vTaskDelay(pdMS_TO_TICKS(250));
    }
    moveBackward(90);
    vTaskDelay(pdMS_TO_TICKS(500));

    // 急停
    stopAll();
    vTaskDelay(pdMS_TO_TICKS(250));

    // 第三段：带旋转的后退（更花哨）
    for (int i = 0; i < 3; i++)
    {
        // 后退
        moveBackward(70);
        vTaskDelay(pdMS_TO_TICKS(350));

        // 快速小旋转
        turnRight(80);
        vTaskDelay(pdMS_TO_TICKS(200));

        // 继续后退
        moveBackward(70);
        vTaskDelay(pdMS_TO_TICKS(350));

        // 反向小旋转
        turnLeft(80);
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    // 最后的华丽结束：大旋转
    stopAll();
    vTaskDelay(pdMS_TO_TICKS(200));

    // 加速旋转
    for (int speed = 40; speed <= 90; speed += 16)
    {
        turnRight(speed);
        vTaskDelay(pdMS_TO_TICKS(150));
    }

    // 高速旋转
    turnRight(95);
    vTaskDelay(pdMS_TO_TICKS(700));

    // 减速停止
    for (int speed = 90; speed >= 40; speed -= 16)
    {
        turnRight(speed);
        vTaskDelay(pdMS_TO_TICKS(150));
    }

    stopAll();

    // 最后的点缀：快速左右摆动
    vTaskDelay(pdMS_TO_TICKS(150));
    turnLeft(70);
    vTaskDelay(pdMS_TO_TICKS(150));
    turnRight(70);
    vTaskDelay(pdMS_TO_TICKS(150));

    stopAll();
    ESP_LOGI(TAG, "✅ Dance Moonwalk completed");
}