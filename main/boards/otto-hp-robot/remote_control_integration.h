/*
 * remote_control_integration.h
 * Otto HP Robot 遥控模式集成接口
 *
 * 这个文件提供简单的集成接口,供 otto_hp_robot.cc 调用
 * 不需要修改现有代码,只需在适当的地方调用这些函数即可
 */

#ifndef REMOTE_CONTROL_INTEGRATION_H
#define REMOTE_CONTROL_INTEGRATION_H

#include "mode_manager.h"
#include "remote_control_server.h"
#include <esp_log.h>

#define RC_TAG "RemoteControlIntegration"

/*
 * 初始化遥控模式功能
 * 应在板子初始化时调用 (例如在构造函数中)
 */
inline void InitializeRemoteControlMode()
{
    ESP_LOGI(RC_TAG, "初始化遥控模式功能...");

    // 1. 初始化模式管理器
    ModeManager::GetInstance().Initialize();

    // 2. 注册模式切换回调
    ModeManager::GetInstance().OnModeChanged([](DeviceMode old_mode, DeviceMode new_mode)
                                             {
        ESP_LOGI(RC_TAG, "模式切换: %s -> %s", 
                 ModeManager::GetModeName(old_mode),
                 ModeManager::GetModeName(new_mode));
        
        if (new_mode == kModeRemoteControl) {
            // 进入遥控模式
            ESP_LOGI(RC_TAG, "进入遥控模式,启动 Web 服务器...");
            
            // 启动 Web 服务器
            if (RemoteControlServer::GetInstance().Start()) {
                ESP_LOGI(RC_TAG, "遥控 Web 服务器已启动: %s", 
                         RemoteControlServer::GetInstance().GetServerUrl());
            } else {
                ESP_LOGE(RC_TAG, "遥控 Web 服务器启动失败");
            }
            
        } else if (new_mode == kModeXiaozhi) {
            // 返回小智模式
            ESP_LOGI(RC_TAG, "返回小智模式,停止 Web 服务器...");
            
            // 停止 Web 服务器
            RemoteControlServer::GetInstance().Stop();
            ESP_LOGI(RC_TAG, "遥控 Web 服务器已停止");
        } });

    ESP_LOGI(RC_TAG, "遥控模式功能初始化完成");
}

/*
 * 处理 MODE_BUTTON 按钮点击
 * 应在 MODE_BUTTON 的 OnClick 回调中调用
 */
inline void HandleModeButtonClick()
{
    ESP_LOGI(RC_TAG, "MODE_BUTTON 被点击");

    // 切换模式
    ModeManager::GetInstance().ToggleMode();

    // 可选: 播放提示音
    // Application::GetInstance().PlaySound(OGG_MODE_SWITCH);
}

/*
 * 获取当前模式
 */
inline DeviceMode GetCurrentMode()
{
    return ModeManager::GetInstance().GetCurrentMode();
}

/*
 * 检查是否在遥控模式
 */
inline bool IsRemoteControlMode()
{
    return ModeManager::GetInstance().GetCurrentMode() == kModeRemoteControl;
}

/*
 * 手动切换到遥控模式
 */
inline void SwitchToRemoteControlMode()
{
    ModeManager::GetInstance().SwitchToRemoteControlMode();
}

/*
 * 手动切换到小智模式
 */
inline void SwitchToXiaozhiMode()
{
    ModeManager::GetInstance().SwitchToXiaozhiMode();
}

/*
 * 获取遥控服务器 URL
 */
inline const char *GetRemoteControlUrl()
{
    if (RemoteControlServer::GetInstance().IsRunning())
    {
        return RemoteControlServer::GetInstance().GetServerUrl();
    }
    return "服务器未运行";
}

#endif // REMOTE_CONTROL_INTEGRATION_H
