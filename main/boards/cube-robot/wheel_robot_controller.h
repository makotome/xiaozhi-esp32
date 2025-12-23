/*
    wheel_robot_controller.h
    两轮机器人控制器头文件
    定义两轮机器人的 MCP 控制接口
*/

#ifndef WHEEL_ROBOT_CONTROLLER_H
#define WHEEL_ROBOT_CONTROLLER_H

#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "wheel_movements.h"

class WheelRobotController {
 private:
  WheelMovements wheels_;
  TaskHandle_t action_task_handle_;
  QueueHandle_t action_queue_;
  bool is_action_in_progress_;
  esp_timer_handle_t speaking_gesture_timer_;  // 讲话动作定时器
  bool was_speaking_;                          // 记录上次是否在讲话状态

  // 动作参数结构
  struct WheelActionParams {
    int action_type;
    int speed;         // 速度 (0-100)
    int duration_ms;   // 持续时间（毫秒）
    int left_speed;    // 左轮速度 (-100 到 100)
    int right_speed;   // 右轮速度 (-100 到 100)
    int target_speed;  // 目标速度（用于加速）
    float direction;   // 方向 (-1.0 到 1.0)，用于万向移动
  };

  // 动作类型枚举
  enum ActionType {
    ACTION_FORWARD = 1,              // 前进
    ACTION_BACKWARD = 2,             // 后退
    ACTION_TURN_LEFT = 3,            // 左转
    ACTION_TURN_RIGHT = 4,           // 右转
    ACTION_STOP = 5,                 // 停止
    ACTION_ACCELERATE = 6,           // 加速
    ACTION_DECELERATE = 7,           // 减速
    ACTION_CUSTOM_SPEED = 8,         // 自定义左右轮速度
    ACTION_SPIN_LEFT = 9,            // 原地左转
    ACTION_SPIN_RIGHT = 10,          // 原地右转
    ACTION_DANCE_SHAKE = 11,         // 跳舞：摇摆舞
    ACTION_DANCE_SPIN = 12,          // 跳舞：旋转舞
    ACTION_DANCE_WAVE = 13,          // 跳舞：波浪舞
    ACTION_DANCE_ZIGZAG = 14,        // 跳舞：之字舞
    ACTION_DANCE_MOONWALK = 15,      // 跳舞：太空步
    ACTION_FORWARD_DIRECTION = 16,   // 前进+方向控制
    ACTION_BACKWARD_DIRECTION = 17,  // 后退+方向控制
    ACTION_SPEAKING_GESTURE = 18     // 讲话时的自然动作
  };

  // 私有方法
  static void ActionTask(void* arg);
  static void SpeakingGestureTimerCallback(void* arg);  // 定时器回调
  void StartActionTaskIfNeeded();
  void QueueAction(int action_type, int speed = 0, int duration_ms = 0,
                   int left_speed = 0, int right_speed = 0,
                   int target_speed = 0, float direction = 0.0f);

 public:
  WheelRobotController();
  ~WheelRobotController();

  // 初始化讲话动作定时器
  void InitializeSpeakingGestureTimer();

  // 注册 MCP 工具
  void RegisterMcpTools();

  // 触发讲话时的自然动作（由外部调用，比如音频输出回调）
  void TriggerSpeakingGesture();

  // 获取轮子控制器引用（用于测试）
  WheelMovements& GetWheels() { return wheels_; }
};

// 全局初始化函数
void InitializeWheelRobotController();

// 获取全局控制器实例
WheelRobotController* GetWheelRobotController();

#endif  // WHEEL_ROBOT_CONTROLLER_H
