/*
 * mode_manager.h
 * Otto HP Robot 模式管理器
 * 负责管理小智模式和遥控模式之间的切换
 */

#ifndef MODE_MANAGER_H
#define MODE_MANAGER_H

#include <functional>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// 设备运行模式枚举
enum DeviceMode
{
    kModeXiaozhi,       // 小智对话模式 (默认)
    kModeRemoteControl, // WiFi 遥控模式
    kModeBtGamepad      // 蓝牙摇杆模式
};

// 模式管理器 - 单例模式
class ModeManager
{
private:
    static ModeManager *instance_;
    DeviceMode current_mode_;
    SemaphoreHandle_t mode_mutex_;

    // 模式切换回调函数列表
    std::vector<std::function<void(DeviceMode, DeviceMode)>> callbacks_;

    // 私有构造函数 (单例模式)
    ModeManager();

    // 通知所有监听者模式已改变
    void NotifyModeChanged(DeviceMode old_mode, DeviceMode new_mode);

public:
    // 禁止拷贝和赋值
    ModeManager(const ModeManager &) = delete;
    ModeManager &operator=(const ModeManager &) = delete;

    // 获取单例实例
    static ModeManager &GetInstance();

    // 析构函数
    ~ModeManager();

    // 初始化模式管理器
    void Initialize();

    // 获取当前模式
    DeviceMode GetCurrentMode() const;

    // 切换到小智模式
    void SwitchToXiaozhiMode();

    // 切换到遥控模式
    void SwitchToRemoteControlMode();

    // 切换到蓝牙摇杆模式
    void SwitchToBtGamepadMode();

    // 切换模式 (在三种模式间循环切换)
    void ToggleMode();

    // 注册模式切换回调
    // 回调参数: (旧模式, 新模式)
    void OnModeChanged(std::function<void(DeviceMode, DeviceMode)> callback);

    // 获取模式名称字符串
    static const char *GetModeName(DeviceMode mode);
};

#endif // MODE_MANAGER_H
