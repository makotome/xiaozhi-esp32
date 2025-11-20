/*
 * mode_manager.cc
 * Otto HP Robot 模式管理器实现
 */

#include "mode_manager.h"
#include <esp_log.h>

#define TAG "ModeManager"

// 静态成员初始化
ModeManager *ModeManager::instance_ = nullptr;

// 私有构造函数
ModeManager::ModeManager()
    : current_mode_(kModeXiaozhi),
      mode_mutex_(nullptr)
{
}

// 析构函数
ModeManager::~ModeManager()
{
    if (mode_mutex_ != nullptr)
    {
        vSemaphoreDelete(mode_mutex_);
        mode_mutex_ = nullptr;
    }
}

// 获取单例实例
ModeManager &ModeManager::GetInstance()
{
    if (instance_ == nullptr)
    {
        instance_ = new ModeManager();
    }
    return *instance_;
}

// 初始化模式管理器
void ModeManager::Initialize()
{
    ESP_LOGI(TAG, "初始化模式管理器...");

    // 创建互斥锁
    mode_mutex_ = xSemaphoreCreateMutex();
    if (mode_mutex_ == nullptr)
    {
        ESP_LOGE(TAG, "创建互斥锁失败");
        return;
    }

    // 默认从小智模式开始
    current_mode_ = kModeXiaozhi;

    ESP_LOGI(TAG, "模式管理器初始化完成，当前模式: %s", GetModeName(current_mode_));
}

// 获取当前模式
DeviceMode ModeManager::GetCurrentMode() const
{
    return current_mode_;
}

// 通知所有监听者模式已改变
void ModeManager::NotifyModeChanged(DeviceMode old_mode, DeviceMode new_mode)
{
    ESP_LOGI(TAG, "模式切换: %s -> %s", GetModeName(old_mode), GetModeName(new_mode));

    // 调用所有注册的回调函数
    for (auto &callback : callbacks_)
    {
        if (callback)
        {
            callback(old_mode, new_mode);
        }
    }
}

// 切换到小智模式
void ModeManager::SwitchToXiaozhiMode()
{
    if (mode_mutex_ == nullptr)
    {
        ESP_LOGE(TAG, "模式管理器未初始化");
        return;
    }

    xSemaphoreTake(mode_mutex_, portMAX_DELAY);

    if (current_mode_ == kModeXiaozhi)
    {
        ESP_LOGW(TAG, "已经在小智模式，无需切换");
        xSemaphoreGive(mode_mutex_);
        return;
    }

    DeviceMode old_mode = current_mode_;
    current_mode_ = kModeXiaozhi;

    xSemaphoreGive(mode_mutex_);

    // 通知监听者
    NotifyModeChanged(old_mode, current_mode_);
}

// 切换到遥控模式
void ModeManager::SwitchToRemoteControlMode()
{
    if (mode_mutex_ == nullptr)
    {
        ESP_LOGE(TAG, "模式管理器未初始化");
        return;
    }

    xSemaphoreTake(mode_mutex_, portMAX_DELAY);

    if (current_mode_ == kModeRemoteControl)
    {
        ESP_LOGW(TAG, "已经在遥控模式，无需切换");
        xSemaphoreGive(mode_mutex_);
        return;
    }

    DeviceMode old_mode = current_mode_;
    current_mode_ = kModeRemoteControl;

    xSemaphoreGive(mode_mutex_);

    // 通知监听者
    NotifyModeChanged(old_mode, current_mode_);
}

// 切换模式 (在两种模式间切换)
void ModeManager::ToggleMode()
{
    if (mode_mutex_ == nullptr)
    {
        ESP_LOGE(TAG, "模式管理器未初始化");
        return;
    }

    xSemaphoreTake(mode_mutex_, portMAX_DELAY);
    DeviceMode old_mode = current_mode_;

    // 切换到另一个模式
    if (current_mode_ == kModeXiaozhi)
    {
        current_mode_ = kModeRemoteControl;
    }
    else
    {
        current_mode_ = kModeXiaozhi;
    }

    xSemaphoreGive(mode_mutex_);

    // 通知监听者
    NotifyModeChanged(old_mode, current_mode_);
}

// 注册模式切换回调
void ModeManager::OnModeChanged(std::function<void(DeviceMode, DeviceMode)> callback)
{
    if (callback)
    {
        callbacks_.push_back(callback);
        ESP_LOGD(TAG, "注册模式切换回调，当前回调数量: %d", callbacks_.size());
    }
}

// 获取模式名称字符串
const char *ModeManager::GetModeName(DeviceMode mode)
{
    switch (mode)
    {
    case kModeXiaozhi:
        return "小智模式";
    case kModeRemoteControl:
        return "遥控模式";
    default:
        return "未知模式";
    }
}
