/*
 * bt_gamepad_server.h
 * Otto HP Robot 蓝牙游戏手柄服务器
 *
 * 功能：
 * - 接收 Dabble App 的蓝牙游戏手柄数据
 * - 支持三种控制模式：数字、摇杆、加速度计
 * - 实现万向移动控制
 * - 信号节流保护
 *
 * 作者: GitHub Copilot
 * 日期: 2025-11-21
 * 版本: 1.0
 */

#ifndef BT_GAMEPAD_SERVER_H
#define BT_GAMEPAD_SERVER_H

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_timer.h>
#include "wheel_robot_controller.h"

// BLE 相关头文件
#ifdef CONFIG_BT_BLUEDROID_ENABLED
#include <esp_gap_ble_api.h>
#include <esp_gatts_api.h>
#include <esp_bt_defs.h>
#endif

// 前向声明
class LightMcpController;
extern LightMcpController *GetLightMcpController();
extern WheelRobotController *GetWheelRobotController();

// ==================== Dabble 数据结构定义 ====================

/**
 * Dabble Gamepad 控制模式
 */
enum DabbleGamepadMode
{
    kDabbleModeDigital,      // 数字按键模式（方向键）
    kDabbleModeJoystick,     // 摇杆模式（虚拟摇杆）
    kDabbleModeAccelerometer // 加速度计模式（手机倾斜）
};

/**
 * Dabble 按钮定义（位掩码）
 */
enum DabbleButton
{
    kDabbleButtonUp = (1 << 0),    // 上方向键
    kDabbleButtonDown = (1 << 1),  // 下方向键
    kDabbleButtonLeft = (1 << 2),  // 左方向键
    kDabbleButtonRight = (1 << 3), // 右方向键
    kDabbleButton1 = (1 << 4),     // 按钮1 (△/Y) - 停止
    kDabbleButton2 = (1 << 5),     // 按钮2 (○/B) - 跳舞
    kDabbleButton3 = (1 << 6),     // 按钮3 (×/A) - 跳舞灯光
    kDabbleButton4 = (1 << 7),     // 按钮4 (□/X) - 夜光
    kDabbleButtonStart = (1 << 8), // START - 紧急停止
    kDabbleButtonSelect = (1 << 9) // SELECT
};

/**
 * Dabble Gamepad 数据包结构
 */
struct DabbleGamepadData
{
    DabbleGamepadMode mode; // 当前控制模式

    // 摇杆/加速度计数据（Joystick & Accelerometer 模式）
    int8_t axis_x; // X 轴 (-127 ~ 127)
    int8_t axis_y; // Y 轴 (-127 ~ 127)

    // 数字按钮状态（Digital 模式）
    bool up;    // 上
    bool down;  // 下
    bool left;  // 左
    bool right; // 右

    // 功能按钮（所有模式通用）
    uint16_t buttons; // 按钮位掩码

    // 原始数据
    uint8_t angle;  // 方向角度 (0-360)
    uint8_t radius; // 摇杆力度 (0-7)

    // 构造函数 - 初始化为零值
    DabbleGamepadData()
        : mode(kDabbleModeJoystick),
          axis_x(0), axis_y(0),
          up(false), down(false), left(false), right(false),
          buttons(0), angle(0), radius(0) {}
};

// ==================== 命令节流器 ====================

/**
 * 通用命令节流器
 * 用于限制命令发送频率，避免指令堆积
 */
class CommandThrottler
{
private:
    uint32_t interval_ms_;          // 命令间隔（毫秒）
    uint32_t last_command_time_ms_; // 上次命令时间戳

public:
    /**
     * 构造函数
     * @param interval_ms 最小命令间隔（毫秒）
     */
    explicit CommandThrottler(uint32_t interval_ms)
        : interval_ms_(interval_ms),
          last_command_time_ms_(0) {}

    /**
     * 检查是否可以发送命令
     * @return true - 可以发送；false - 需要等待
     */
    bool CanExecute()
    {
        uint32_t current_time = esp_timer_get_time() / 1000; // 转换为毫秒

        if (current_time - last_command_time_ms_ >= interval_ms_)
        {
            last_command_time_ms_ = current_time;
            return true;
        }

        return false;
    }

    /**
     * 强制重置（用于紧急停止等场景）
     */
    void Reset()
    {
        last_command_time_ms_ = 0;
    }

    /**
     * 设置新的间隔时间
     */
    void SetInterval(uint32_t interval_ms)
    {
        interval_ms_ = interval_ms;
    }

    /**
     * 获取当前间隔时间
     */
    uint32_t GetInterval() const
    {
        return interval_ms_;
    }
};

// ==================== 蓝牙游戏手柄服务器 ====================

/**
 * 蓝牙游戏手柄服务器（单例模式）
 * 负责接收和处理 Dabble App 的蓝牙数据
 */
class BtGamepadServer
{
private:
    // ===== 单例相关 =====
    static BtGamepadServer *instance_;

    // ===== 依赖组件 =====
    WheelRobotController *wheel_controller_;

    // ===== 运行状态 =====
    bool is_running_;
    bool is_connected_;

    // ===== 蓝牙配置 =====
    static constexpr const char *DEVICE_NAME = "Otto Robot";
    static constexpr uint16_t HID_APPEARANCE = 0x03C4; // Gamepad

    // ===== 控制参数 =====
    static constexpr int8_t DEADZONE_THRESHOLD = 10; // 摇杆死区
    static constexpr int DEFAULT_DIGITAL_SPEED = 60; // 数字模式默认速度

    // ===== 命令节流器 =====
    CommandThrottler move_throttler_;   // 移动命令节流（100ms）
    CommandThrottler button_throttler_; // 按钮命令节流（500ms）

    // ===== 当前状态 =====
    DabbleGamepadData current_data_;
    bool dance_light_enabled_;
    bool night_light_enabled_;

    // ===== 私有构造函数（单例模式） =====
    BtGamepadServer();

    // ===== 蓝牙回调（TODO: 实现） =====
    static void BluetoothEventCallback(void *param);
    static void DabbleDataCallback(const uint8_t *data, size_t length);

    // ===== 数据解析 =====
    void ParseDabbleData(const uint8_t *data, size_t length);
    void ProcessGamepadData(const DabbleGamepadData &data);

    // ===== 移动控制（分模式处理） =====
    void HandleDigitalMode(const DabbleGamepadData &data);
    void HandleJoystickMode(const DabbleGamepadData &data);
    void HandleAccelerometerMode(const DabbleGamepadData &data);

    // ===== 万向移动核心 =====
    void MoveWithOmniDirection(int speed, float direction, bool is_forward);
    void StopMovement();

    // ===== 按钮处理 =====
    void HandleButtonPress(uint16_t buttons);
    void OnButton1Press(); // 停止
    void OnButton2Press(); // 跳舞
    void OnButton3Press(); // 跳舞灯光
    void OnButton4Press(); // 夜光
    void OnStartPress();   // 紧急停止

    // ===== 辅助函数 =====
    static int8_t ApplyDeadzone(int8_t value, int8_t threshold);
    static int CalculateSpeedFromXY(int8_t x, int8_t y);
    static float CalculateDirectionFromXY(int8_t x, int8_t y);
    static bool IsMoveBackward(int8_t y);

public:
    // ===== 禁止拷贝和赋值 =====
    BtGamepadServer(const BtGamepadServer &) = delete;
    BtGamepadServer &operator=(const BtGamepadServer &) = delete;

    // ===== 获取单例实例 =====
    static BtGamepadServer &GetInstance();
    static BtGamepadServer *GetInstancePtr() { return instance_; }

    // ===== 析构函数 =====
    ~BtGamepadServer();

    // ===== 服务器控制 =====
    bool Start();
    void Stop();

    // ===== 状态查询 =====
    bool IsRunning() const { return is_running_; }
    bool IsConnected() const { return is_connected_; }
    const char *GetDeviceName() const { return DEVICE_NAME; }

    // ===== 获取当前游戏手柄数据（供显示模块使用） =====
    const DabbleGamepadData &GetCurrentData() const { return current_data_; }
    bool IsDanceLightEnabled() const { return dance_light_enabled_; }
    bool IsNightLightEnabled() const { return night_light_enabled_; }

    // ===== 设置连接状态（由蓝牙回调调用） =====
    void SetConnected(bool connected) { is_connected_ = connected; }

    // ===== BLE 数据接收公共接口（供回调使用） =====
    void OnBleDataReceived(const uint8_t *data, size_t length) { ParseDabbleData(data, length); }

    // ===== BLE 配置函数（仅 BLE 版本） =====
#ifdef CONFIG_BT_BLUEDROID_ENABLED
    static esp_ble_adv_data_t GetAdvData();
    static esp_ble_adv_params_t GetAdvParams();
    static esp_gatt_srvc_id_t GetServiceId();
    static esp_bt_uuid_t GetTxCharUuid();
    static esp_bt_uuid_t GetRxCharUuid();
#endif
};

#endif // BT_GAMEPAD_SERVER_H
