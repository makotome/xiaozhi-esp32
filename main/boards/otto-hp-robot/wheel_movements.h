/*
    wheel_movements.h
    Otto HP Robot Wheel Movements Header File
    Defines the interface for controlling the movements of the Otto HP Robot.
    Author: Xumx
    Date: 2024-08-15
    Version: 1.0
*/

#ifndef WHEEL_MOVEMENTS_H
#define WHEEL_MOVEMENTS_H

#include "wheel_servo.h"

class WheelMovements
{
public:
    WheelMovements();
    ~WheelMovements();

    // 初始化双轮系统
    bool init();

    // 基本运动控制
    void moveForward(int speed = 50);  // 前进
    void moveBackward(int speed = 50); // 后退
    void stopAll();                    // 停止
    void turnLeft(int speed = 50);     // 左转
    void turnRight(int speed = 50);    // 右转

    // 高级控制
    void accelerate(int targetSpeed, int duration_ms);  // 加速
    void decelerate(int duration_ms);                   // 减速
    void setWheelSpeeds(int leftSpeed, int rightSpeed); // 独立控制左右轮

    // 获取状态
    int getLeftSpeed() const;
    int getRightSpeed() const;

    void runHardwareDiagnostics();

private:
    WheelServo *_leftWheel;
    WheelServo *_rightWheel;
    bool _initialized;
};

#endif // WHEEL_MOVEMENTS_H
