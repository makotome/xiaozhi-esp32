/*
 * bt_gamepad_integration.h
 * 蓝牙摇杆模式集成接口
 *
 * 提供简洁的调用接口，方便在主程序中集成
 *
 * 作者: GitHub Copilot
 * 日期: 2025-11-21
 * 版本: 1.0
 */

#ifndef BT_GAMEPAD_INTEGRATION_H
#define BT_GAMEPAD_INTEGRATION_H

#include "mode_manager.h"
#include "bt_gamepad_server.h"
#include "bt_gamepad_display.h"
#include "remote_control_server.h"
#include "application.h"
#include <esp_log.h>

#define BT_INTEGRATION_TAG "BtGamepadIntegration"

// 全局显示对象指针（在 InitializeBtGamepadMode 中初始化）
static BtGamepadDisplay *g_bt_gamepad_display = nullptr;

/**
 * 初始化蓝牙摇杆模式
 *
 * 功能：
 * 1. 注册模式切换回调
 * 2. 自动管理蓝牙服务器的启动/停止
 * 3. 确保与 WiFi 遥控模式互斥
 * 4. 管理显示界面（可选）
 *
 * 使用方法：
 * 在主程序构造函数中调用一次即可
 *
 * @param display 显示对象指针（可选，传入nullptr则不启用显示）
 */
inline void InitializeBtGamepadMode(Display *display = nullptr)
{
    ESP_LOGI(BT_INTEGRATION_TAG, "初始化蓝牙摇杆模式...");

    // 创建显示对象（如果提供了display指针）
    if (display != nullptr)
    {
        g_bt_gamepad_display = new BtGamepadDisplay(display, &BtGamepadServer::GetInstance());
        ESP_LOGI(BT_INTEGRATION_TAG, "显示界面已创建");
    }

    // 注册模式切换回调
    ModeManager::GetInstance().OnModeChanged(
        [](DeviceMode old_mode, DeviceMode new_mode)
        {
            ESP_LOGI(BT_INTEGRATION_TAG, "模式切换: %s -> %s",
                     ModeManager::GetModeName(old_mode),
                     ModeManager::GetModeName(new_mode));

            // ===== 进入蓝牙摇杆模式 =====
            if (new_mode == kModeBtGamepad)
            {
                ESP_LOGI(BT_INTEGRATION_TAG, "→ 启动蓝牙游戏手柄服务器");

                // 停止 WiFi 遥控服务器（如果在运行）
                if (old_mode == kModeRemoteControl)
                {
                    ESP_LOGI(BT_INTEGRATION_TAG, "  停止 WiFi 遥控服务器");
                    RemoteControlServer::GetInstance().Stop();
                }

                // 【关键】禁用小智的唤醒词检测，避免资源冲突
                auto &app = Application::GetInstance();
                app.GetAudioService().EnableWakeWordDetection(false);
                ESP_LOGI(BT_INTEGRATION_TAG, "  已禁用语音唤醒检测");

                // 启动蓝牙服务器
                if (BtGamepadServer::GetInstance().Start())
                {
                    ESP_LOGI(BT_INTEGRATION_TAG, "✓ 蓝牙摇杆模式已激活");
                    ESP_LOGI(BT_INTEGRATION_TAG, "  设备名称: %s",
                             BtGamepadServer::GetInstance().GetDeviceName());
                    ESP_LOGI(BT_INTEGRATION_TAG, "  请在 Dabble App 中搜索并连接");

                    // 激活显示界面
                    if (g_bt_gamepad_display != nullptr)
                    {
                        g_bt_gamepad_display->Activate();
                        ESP_LOGI(BT_INTEGRATION_TAG, "  显示界面已激活");
                    }
                }
                else
                {
                    ESP_LOGE(BT_INTEGRATION_TAG, "✗ 蓝牙服务器启动失败");
                    // 启动失败时恢复唤醒词检测
                    app.GetAudioService().EnableWakeWordDetection(true);
                }
            }

            // ===== 离开蓝牙摇杆模式 =====
            else if (old_mode == kModeBtGamepad)
            {
                ESP_LOGI(BT_INTEGRATION_TAG, "→ 停止蓝牙游戏手柄服务器");

                // 停用显示界面
                if (g_bt_gamepad_display != nullptr)
                {
                    g_bt_gamepad_display->Deactivate();
                    ESP_LOGI(BT_INTEGRATION_TAG, "  显示界面已停用");
                }

                BtGamepadServer::GetInstance().Stop();
                ESP_LOGI(BT_INTEGRATION_TAG, "✓ 蓝牙摇杆模式已关闭");

                // 【关键】恢复小智的唤醒词检测
                if (new_mode == kModeXiaozhi)
                {
                    auto &app = Application::GetInstance();
                    app.GetAudioService().EnableWakeWordDetection(true);
                    ESP_LOGI(BT_INTEGRATION_TAG, "  已恢复语音唤醒检测");
                }
            }
        });

    ESP_LOGI(BT_INTEGRATION_TAG, "蓝牙摇杆模式初始化完成");
}

/**
 * 检查是否在蓝牙摇杆模式
 * @return true - 当前在蓝牙摇杆模式；false - 其他模式
 */
inline bool IsBtGamepadMode()
{
    return ModeManager::GetInstance().GetCurrentMode() == kModeBtGamepad;
}

/**
 * 获取蓝牙设备名称
 * @return 设备名称字符串
 */
inline const char *GetBtDeviceName()
{
    return BtGamepadServer::GetInstance().GetDeviceName();
}

/**
 * 检查蓝牙是否已连接
 * @return true - 已连接；false - 未连接
 */
inline bool IsBtGamepadConnected()
{
    return BtGamepadServer::GetInstance().IsConnected();
}

/**
 * 获取当前模式的友好名称
 * @return 模式名称字符串
 */
inline const char *GetCurrentModeName()
{
    DeviceMode mode = ModeManager::GetInstance().GetCurrentMode();
    return ModeManager::GetModeName(mode);
}

#endif // BT_GAMEPAD_INTEGRATION_H
