/*
    wheel_servo.h
    Otto HP Robot Wheel Servo Header File
    Defines the interface for continuous rotation servo control
    Author: Xumx
    Date: 2024-08-15
    Version: 1.0
*/

#ifndef WHEEL_SERVO_H
#define WHEEL_SERVO_H

class WheelServo
{
public:
    WheelServo(int pin, int channel, int timer); // 添加 timer 参数
    ~WheelServo();

    // 初始化舵机
    bool init();

    // 设置速度 (-100 到 +100，0为停止，正值前进，负值后退)
    void setSpeed(int speed);

    // 停止舵机
    void stop();

    // 获取当前速度
    int getSpeed() const { return _currentSpeed; }

    // 硬件诊断测试（直接PWM控制）
    void runHardwareDiagnostic();

private:
    int _pin;
    int _channel;
    int _timer;        // 每个舵机使用自己的 Timer
    int _currentSpeed; // 当前速度 (-100 到 +100)
    bool _initialized;

    // 将速度转换为PWM占空比
    unsigned int speedToDuty(int speed);
};

#endif // WHEEL_SERVO_H
