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
    : _leftWheel(nullptr), _rightWheel(nullptr), _initialized(false), _dance_interrupted(false)
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

void WheelMovements::moveForwardWithDirection(int speed, float direction)
{
    if (!_initialized)
    {
        ESP_LOGW(TAG, "WheelMovements not initialized");
        return;
    }

    // 限制参数范围
    speed = std::max(MIN_SPEED, std::min(MAX_SPEED, speed));
    direction = std::max(-1.0f, std::min(1.0f, direction)); // -1.0 到 1.0

    // 计算差速控制 - 进一步缩小速度差,让转向更平滑
    // direction = 0: 两轮同速,直线前进
    // direction > 0: 右转,左轮快,右轮慢 (但右轮保持至少70%速度)
    // direction < 0: 左转,左轮慢,右轮快 (但左轮保持至少70%速度)

    // 使用更小的direction系数(0.3倍),让速度差更温和,转向更平滑
    // 例如: direction=-1.0时,慢速轮保持70%速度而不是60%
    const float DIRECTION_FACTOR = 0.3f; // 从0.4降低到0.3,进一步减小转向强度

    int leftSpeed = speed;
    int rightSpeed = speed;

    if (direction > 0)
    {
        // 右转: 减小右轮速度 (最多减30%,保持70%)
        rightSpeed = static_cast<int>(speed * (1.0f - direction * DIRECTION_FACTOR));
    }
    else if (direction < 0)
    {
        // 左转: 减小左轮速度 (最多减30%,保持70%)
        leftSpeed = static_cast<int>(speed * (1.0f + direction * DIRECTION_FACTOR)); // direction是负数
    }

    ESP_LOGI(TAG, "Forward with direction: speed=%d, dir=%.2f → left=%d, right=%d",
             speed, direction, leftSpeed, rightSpeed);

    setWheelSpeeds(leftSpeed, rightSpeed);
}
void WheelMovements::moveBackwardWithDirection(int speed, float direction)
{
    if (!_initialized)
    {
        ESP_LOGW(TAG, "WheelMovements not initialized");
        return;
    }

    ESP_LOGI(TAG, "========== moveBackwardWithDirection 被调用 ==========");
    ESP_LOGI(TAG, "输入参数: speed=%d, direction=%.2f", speed, direction);

    // 限制参数范围
    speed = std::max(MIN_SPEED, std::min(MAX_SPEED, speed));
    direction = std::max(-1.0f, std::min(1.0f, direction));

    ESP_LOGI(TAG, "参数限制后: speed=%d, direction=%.2f", speed, direction);

    // 后退时的差速控制 - 进一步缩小速度差,让转向更平滑
    // direction = 0: 两轮同速,直线后退
    // direction > 0: 后退时右转,左轮快,右轮慢 (但右轮保持至少70%速度)
    // direction < 0: 后退时左转,左轮慢,右轮快 (但左轮保持至少70%速度)

    // 使用相同的direction系数(0.3倍),保持前进后退行为一致
    const float DIRECTION_FACTOR = 0.3f; // 从0.4降低到0.3,进一步减小转向强度

    int leftSpeed = speed;
    int rightSpeed = speed;

    if (direction > 0)
    {
        // 后退右转: 减小右轮速度 (最多减30%,保持70%)
        rightSpeed = static_cast<int>(speed * (1.0f - direction * DIRECTION_FACTOR));
        ESP_LOGI(TAG, "后退右转: direction=%.2f > 0", direction);
    }
    else if (direction < 0)
    {
        // 后退左转: 减小左轮速度 (最多减30%,保持70%)
        leftSpeed = static_cast<int>(speed * (1.0f + direction * DIRECTION_FACTOR));
        ESP_LOGI(TAG, "后退左转: direction=%.2f < 0", direction);
    }
    else
    {
        ESP_LOGI(TAG, "直线后退: direction=%.2f = 0", direction);
    }

    ESP_LOGI(TAG, "计算后速度: leftSpeed=%d, rightSpeed=%d", leftSpeed, rightSpeed);
    ESP_LOGI(TAG, "即将调用: setWheelSpeeds(-%d, -%d) = setWheelSpeeds(%d, %d)",
             leftSpeed, rightSpeed, -leftSpeed, -rightSpeed);

    // 后退时两轮都是负速度
    setWheelSpeeds(-leftSpeed, -rightSpeed);

    ESP_LOGI(TAG, "========== moveBackwardWithDirection 执行完成 ==========");
}

void WheelMovements::stopAll()
{
    if (!_initialized)
    {
        return;
    }

    // 中断正在进行的舞蹈
    _dance_interrupted = true;

    _leftWheel->stop();
    _rightWheel->stop();

    ESP_LOGI(TAG, "Stopped all wheels");
}

void WheelMovements::interruptDance()
{
    _dance_interrupted = true;
    ESP_LOGI(TAG, "Dance interrupted by user");
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

    // 重置中断标志
    _dance_interrupted = false;

    // 摇摆舞：节奏感强的左右摆动，适合桌面小幅度动作
    // 第一轮：慢速热身摇摆 (速度降低，时间缩短)
    for (int i = 0; i < 4 && !_dance_interrupted; i++)
    {
        turnLeft(30);
        vTaskDelay(pdMS_TO_TICKS(200));
        if (_dance_interrupted)
            break;
        turnRight(30);
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    if (_dance_interrupted)
    {
        stopAll();
        return;
    }

    // 第二轮：中速摇摆，增加节奏感
    for (int i = 0; i < 5 && !_dance_interrupted; i++)
    {
        turnLeft(45);
        vTaskDelay(pdMS_TO_TICKS(160));
        if (_dance_interrupted)
            break;
        turnRight(45);
        vTaskDelay(pdMS_TO_TICKS(160));
    }

    if (_dance_interrupted)
    {
        stopAll();
        return;
    }

    // 第三轮：快速小幅摇摆
    for (int i = 0; i < 6 && !_dance_interrupted; i++)
    {
        turnLeft(50);
        vTaskDelay(pdMS_TO_TICKS(120));
        if (_dance_interrupted)
            break;
        turnRight(50);
        vTaskDelay(pdMS_TO_TICKS(120));
    }

    if (_dance_interrupted)
    {
        stopAll();
        return;
    }

    // 结束动作：小幅度摆动后急停
    turnLeft(55);
    vTaskDelay(pdMS_TO_TICKS(250));
    if (_dance_interrupted)
    {
        stopAll();
        return;
    }
    turnRight(55);
    vTaskDelay(pdMS_TO_TICKS(250));

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
    _dance_interrupted = false; // 重置中断标志

    // 旋转舞：多变的小幅旋转组合，适合桌面
    // 第一段：右旋加速 (降低速度和时间)
    for (int speed = 25; speed <= 50 && !_dance_interrupted; speed += 8)
    {
        turnRight(speed);
        vTaskDelay(pdMS_TO_TICKS(180));
    }

    if (_dance_interrupted)
    {
        stopAll();
        return;
    }

    // 保持中速右旋
    turnRight(52);
    vTaskDelay(pdMS_TO_TICKS(500));

    // 急停
    stopAll();
    vTaskDelay(pdMS_TO_TICKS(150));

    // 第二段：左旋加速（反向更有趣）
    for (int speed = 25; speed <= 50; speed += 8)
    {
        turnLeft(speed);
        vTaskDelay(pdMS_TO_TICKS(180));
    }

    // 保持中速左旋
    turnLeft(52);
    vTaskDelay(pdMS_TO_TICKS(500));

    // 急停
    stopAll();
    vTaskDelay(pdMS_TO_TICKS(150));

    // 第三段：快速左右交替旋转 (增加次数，降低速度)
    for (int i = 0; i < 5; i++)
    {
        turnRight(55);
        vTaskDelay(pdMS_TO_TICKS(200));
        turnLeft(55);
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    // 最后减速旋转结束
    for (int speed = 45; speed >= 25; speed -= 10)
    {
        turnRight(speed);
        vTaskDelay(pdMS_TO_TICKS(180));
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
    _dance_interrupted = false; // 重置中断标志

    // 波浪舞：小幅度前后移动，速度呈波浪变化，适合桌面
    // 6个完整的波浪循环（增加循环次数，降低每次幅度）
    for (int wave = 0; wave < 6 && !_dance_interrupted; wave++)
    {
        // 前进波浪：速度从慢到快到慢，加入轻微摆动
        for (int speed = 25; speed <= 45 && !_dance_interrupted; speed += 10)
        {
            moveForward(speed);
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        // 前进到最快时加入小幅度左右摆动
        setWheelSpeeds(42, 48); // 轻微右偏
        vTaskDelay(pdMS_TO_TICKS(80));
        setWheelSpeeds(48, 42); // 轻微左偏
        vTaskDelay(pdMS_TO_TICKS(80));

        for (int speed = 45; speed >= 25; speed -= 10)
        {
            moveForward(speed);
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        // 短暂停顿，加入小动作
        stopAll();
        vTaskDelay(pdMS_TO_TICKS(100));
        turnLeft(28);
        vTaskDelay(pdMS_TO_TICKS(80));
        turnRight(28);
        vTaskDelay(pdMS_TO_TICKS(80));
        stopAll();
        vTaskDelay(pdMS_TO_TICKS(80));

        // 后退波浪：速度从慢到快到慢
        for (int speed = 25; speed <= 45; speed += 10)
        {
            moveBackward(speed);
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        // 后退到最快时加入小幅度摆动
        setWheelSpeeds(-42, -48); // 后退时轻微左偏
        vTaskDelay(pdMS_TO_TICKS(80));
        setWheelSpeeds(-48, -42); // 后退时轻微右偏
        vTaskDelay(pdMS_TO_TICKS(80));

        for (int speed = 45; speed >= 25; speed -= 10)
        {
            moveBackward(speed);
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        // 短暂停顿
        stopAll();
        vTaskDelay(pdMS_TO_TICKS(120));
    }

    // 结束动作：小幅前进后急停
    moveForward(50);
    vTaskDelay(pdMS_TO_TICKS(250));
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
    _dance_interrupted = false; // 重置中断标志

    // 之字舞：小幅Z字形路线，适合桌面，增加动作丰富度
    // 重复4次完整的Z字（增加次数，降低幅度）
    for (int i = 0; i < 4 && !_dance_interrupted; i++)
    {
        // 第一段：加速向右前方移动（左轮快）
        for (int speed = 30; speed <= 45; speed += 8)
        {
            setWheelSpeeds(speed + 8, speed - 8); // 左快右慢 -> 右转前进
            vTaskDelay(pdMS_TO_TICKS(150));
        }
        setWheelSpeeds(50, 38);
        vTaskDelay(pdMS_TO_TICKS(250));

        // 急转：原地右转来增加动感
        turnRight(45);
        vTaskDelay(pdMS_TO_TICKS(180));

        // 第二段：直线快速前进
        moveForward(48);
        vTaskDelay(pdMS_TO_TICKS(300));

        // 小幅度左右摆动增加趣味
        setWheelSpeeds(48, 38);
        vTaskDelay(pdMS_TO_TICKS(100));
        setWheelSpeeds(38, 48);
        vTaskDelay(pdMS_TO_TICKS(100));

        // 急转：原地左转
        turnLeft(45);
        vTaskDelay(pdMS_TO_TICKS(180));

        // 第三段：加速向左前方移动（右轮快）
        for (int speed = 30; speed <= 45; speed += 8)
        {
            setWheelSpeeds(speed - 8, speed + 8); // 左慢右快 -> 左转前进
            vTaskDelay(pdMS_TO_TICKS(150));
        }
        setWheelSpeeds(38, 50);
        vTaskDelay(pdMS_TO_TICKS(250));

        // Z字完成，短暂停顿并加入小动作
        stopAll();
        vTaskDelay(pdMS_TO_TICKS(150));

        // 原地小幅旋转，准备下一个Z字
        turnRight(50);
        vTaskDelay(pdMS_TO_TICKS(200));
        stopAll();
        vTaskDelay(pdMS_TO_TICKS(120));
    }

    // 结束动作：小幅前进后漂亮地停止
    moveForward(48);
    vTaskDelay(pdMS_TO_TICKS(250));
    stopAll();
    vTaskDelay(pdMS_TO_TICKS(80));

    // 最后小转身
    turnLeft(40);
    vTaskDelay(pdMS_TO_TICKS(150));

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
    _dance_interrupted = false; // 重置中断标志

    // 太空步：模拟Michael Jackson的标志性动作，桌面小幅版本
    // 后退时带有节奏感的停顿和加速，更多变化

    // 第一段：经典太空步节奏（重复6次，增加次数降低幅度）
    for (int i = 0; i < 6 && !_dance_interrupted; i++)
    {
        // 快速后退
        moveBackward(45);
        vTaskDelay(pdMS_TO_TICKS(280));

        // 突然停顿（关键动作）
        stopAll();
        vTaskDelay(pdMS_TO_TICKS(150));

        // 慢速后退（滑动感）
        moveBackward(28);
        vTaskDelay(pdMS_TO_TICKS(200));

        // 再次快速后退
        moveBackward(50);
        vTaskDelay(pdMS_TO_TICKS(250));

        // 停顿
        stopAll();
        vTaskDelay(pdMS_TO_TICKS(150));

        // 加入左右摆动（更有节奏感）
        turnLeft(38);
        vTaskDelay(pdMS_TO_TICKS(120));
        turnRight(38);
        vTaskDelay(pdMS_TO_TICKS(120));
        stopAll();
        vTaskDelay(pdMS_TO_TICKS(80));
    }

    // 第二段：加速后退组合
    for (int speed = 28; speed <= 48; speed += 10)
    {
        moveBackward(speed);
        vTaskDelay(pdMS_TO_TICKS(180));
    }
    moveBackward(52);
    vTaskDelay(pdMS_TO_TICKS(320));

    // 急停
    stopAll();
    vTaskDelay(pdMS_TO_TICKS(180));

    // 第三段：带旋转的后退（更花哨）
    for (int i = 0; i < 4; i++)
    {
        // 后退
        moveBackward(42);
        vTaskDelay(pdMS_TO_TICKS(220));

        // 快速小旋转
        turnRight(48);
        vTaskDelay(pdMS_TO_TICKS(140));

        // 继续后退
        moveBackward(42);
        vTaskDelay(pdMS_TO_TICKS(220));

        // 反向小旋转
        turnLeft(48);
        vTaskDelay(pdMS_TO_TICKS(140));
    }

    // 最后的华丽结束：小幅旋转
    stopAll();
    vTaskDelay(pdMS_TO_TICKS(150));

    // 加速旋转
    for (int speed = 32; speed <= 55; speed += 12)
    {
        turnRight(speed);
        vTaskDelay(pdMS_TO_TICKS(120));
    }

    // 中速旋转
    turnRight(58);
    vTaskDelay(pdMS_TO_TICKS(450));

    // 减速停止
    for (int speed = 55; speed >= 32; speed -= 12)
    {
        turnRight(speed);
        vTaskDelay(pdMS_TO_TICKS(120));
    }

    stopAll();

    // 最后的点缀：快速左右摆动
    vTaskDelay(pdMS_TO_TICKS(100));
    turnLeft(42);
    vTaskDelay(pdMS_TO_TICKS(100));
    turnRight(42);
    vTaskDelay(pdMS_TO_TICKS(100));

    stopAll();
    ESP_LOGI(TAG, "✅ Dance Moonwalk completed");
}