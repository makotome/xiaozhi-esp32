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

    // 万向移动控制 (direction范围: -1.0到1.0)
    // -1.0 = 完全左转, 0.0 = 直线, 1.0 = 完全右转
    void moveForwardWithDirection(int speed, float direction);  // 前进+方向控制
    void moveBackwardWithDirection(int speed, float direction); // 后退+方向控制

    // 高级控制
    void accelerate(int targetSpeed, int duration_ms);  // 加速
    void decelerate(int duration_ms);                   // 减速
    void setWheelSpeeds(int leftSpeed, int rightSpeed); // 独立控制左右轮

    // 获取状态
    int getLeftSpeed() const;
    int getRightSpeed() const;

    void runHardwareDiagnostics();

    // 跳舞动作
    void danceShake();    // 摇摆舞：左右快速摆动
    void danceSpin();     // 旋转舞：360度旋转
    void danceWave();     // 波浪舞：前后波浪式移动
    void danceZigzag();   // 之字舞：Z字形移动
    void danceMoonwalk(); // 太空步：后退时的花式移动

    // 中断跳舞
    void interruptDance(); // 中断正在进行的舞蹈

private:
    WheelServo *_leftWheel;
    WheelServo *_rightWheel;
    bool _initialized;
    volatile bool _dance_interrupted; // 舞蹈中断标志
};

#endif // WHEEL_MOVEMENTS_H
