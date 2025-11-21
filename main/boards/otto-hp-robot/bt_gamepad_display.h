/*
 * bt_gamepad_display.h
 * 蓝牙摇杆模式显示界面
 *
 * 提供蓝牙连接状态、控制模式、按钮状态等可视化反馈
 *
 * 作者: GitHub Copilot
 * 日期: 2025-11-21
 * 版本: 1.0
 */

#ifndef BT_GAMEPAD_DISPLAY_H
#define BT_GAMEPAD_DISPLAY_H

#include "display/display.h"
#include "bt_gamepad_server.h"
#include <esp_timer.h>

#define BT_DISPLAY_TAG "BtGamepadDisplay"

/**
 * 蓝牙摇杆模式显示管理器
 *
 * 功能：
 * 1. 显示蓝牙连接状态 (未连接/已连接)
 * 2. 显示当前控制模式 (数字/摇杆/加速度计)
 * 3. 显示移动方向和速度指示
 * 4. 显示按钮按下反馈
 * 5. 显示灯光状态
 */
class BtGamepadDisplay
{
private:
    Display *display_;
    BtGamepadServer *server_;
    esp_timer_handle_t refresh_timer_;
    bool is_active_;

    // 当前显示状态
    bool last_connected_;
    DabbleGamepadMode last_mode_;
    uint16_t last_buttons_;
    bool last_dance_light_;
    bool last_night_light_;

    // 定时器回调 - 定期刷新显示
    static void RefreshTimerCallback(void *arg);

    // 刷新显示内容
    void RefreshDisplay();

    // 显示连接状态
    void ShowConnectionStatus(bool connected);

    // 显示控制模式
    void ShowControlMode(DabbleGamepadMode mode);

    // 显示移动指示（摇杆/加速度计模式）
    void ShowMovementIndicator(const DabbleGamepadData &data);

    // 显示按钮反馈
    void ShowButtonFeedback(uint16_t buttons);

    // 显示灯光状态
    void ShowLightStatus(bool dance_light, bool night_light);

    // 获取模式名称
    const char *GetModeName(DabbleGamepadMode mode);

    // 获取按钮名称
    const char *GetButtonName(uint16_t button);

public:
    BtGamepadDisplay(Display *display, BtGamepadServer *server);
    ~BtGamepadDisplay();

    // 激活显示（进入蓝牙模式时调用）
    void Activate();

    // 停用显示（离开蓝牙模式时调用）
    void Deactivate();

    // 手动更新显示
    void Update();

    // 显示欢迎界面
    void ShowWelcomeScreen();

    // 显示按钮提示
    void ShowButtonGuide();
};

#endif // BT_GAMEPAD_DISPLAY_H
