/*
 * bt_gamepad_display.cc
 * 蓝牙摇杆模式显示界面实现
 *
 * 作者: GitHub Copilot
 * 日期: 2025-11-21
 * 版本: 1.0
 */

#include "bt_gamepad_display.h"
#include <esp_log.h>
#include <cstring>
#include <cstdio>

#define REFRESH_INTERVAL_MS 500 // 每500ms刷新一次显示

// ==================== 构造与析构 ====================

BtGamepadDisplay::BtGamepadDisplay(Display *display, BtGamepadServer *server)
    : display_(display),
      server_(server),
      refresh_timer_(nullptr),
      is_active_(false),
      last_connected_(false),
      last_mode_(kDabbleModeDigital),
      last_buttons_(0),
      last_dance_light_(false),
      last_night_light_(false)
{
    ESP_LOGI(BT_DISPLAY_TAG, "蓝牙摇杆显示模块已创建");
}

BtGamepadDisplay::~BtGamepadDisplay()
{
    Deactivate();
    ESP_LOGI(BT_DISPLAY_TAG, "蓝牙摇杆显示模块已销毁");
}

// ==================== 激活/停用 ====================

void BtGamepadDisplay::Activate()
{
    if (is_active_)
    {
        ESP_LOGW(BT_DISPLAY_TAG, "显示已激活");
        return;
    }

    ESP_LOGI(BT_DISPLAY_TAG, "激活蓝牙摇杆显示界面");

    // 显示欢迎界面
    ShowWelcomeScreen();

    // 创建定时器
    esp_timer_create_args_t timer_args = {};
    timer_args.callback = RefreshTimerCallback;
    timer_args.arg = this;
    timer_args.name = "bt_display_refresh";

    esp_err_t ret = esp_timer_create(&timer_args, &refresh_timer_);
    if (ret == ESP_OK)
    {
        // 启动定时器 (周期性)
        esp_timer_start_periodic(refresh_timer_, REFRESH_INTERVAL_MS * 1000);
        ESP_LOGD(BT_DISPLAY_TAG, "刷新定时器已启动 (%dms)", REFRESH_INTERVAL_MS);
    }
    else
    {
        ESP_LOGE(BT_DISPLAY_TAG, "创建刷新定时器失败: %s", esp_err_to_name(ret));
    }

    is_active_ = true;
}

void BtGamepadDisplay::Deactivate()
{
    if (!is_active_)
    {
        return;
    }

    ESP_LOGI(BT_DISPLAY_TAG, "停用蓝牙摇杆显示界面");

    // 停止并删除定时器
    if (refresh_timer_ != nullptr)
    {
        esp_timer_stop(refresh_timer_);
        esp_timer_delete(refresh_timer_);
        refresh_timer_ = nullptr;
    }

    // 清除显示内容
    if (display_ != nullptr)
    {
        display_->SetStatus("");
    }

    is_active_ = false;
}

// ==================== 定时器回调 ====================

void BtGamepadDisplay::RefreshTimerCallback(void *arg)
{
    auto *self = static_cast<BtGamepadDisplay *>(arg);
    if (self != nullptr && self->is_active_)
    {
        self->RefreshDisplay();
    }
}

// ==================== 显示更新 ====================

void BtGamepadDisplay::RefreshDisplay()
{
    if (display_ == nullptr || server_ == nullptr)
    {
        return;
    }

    // 获取当前状态
    bool connected = server_->IsConnected();
    DabbleGamepadData current_data = server_->GetCurrentData();

    // 检测状态变化
    bool connection_changed = (connected != last_connected_);
    bool mode_changed = (current_data.mode != last_mode_);
    bool buttons_changed = (current_data.buttons != last_buttons_);

    // 1. 连接状态变化 - 高优先级显示
    if (connection_changed)
    {
        ShowConnectionStatus(connected);
        last_connected_ = connected;
    }

    // 2. 控制模式变化
    if (mode_changed && connected)
    {
        ShowControlMode(current_data.mode);
        last_mode_ = current_data.mode;
    }

    // 3. 按钮反馈 - 即时显示
    if (buttons_changed && connected)
    {
        ShowButtonFeedback(current_data.buttons);
        last_buttons_ = current_data.buttons;
    }

    // 4. 移动指示 - 持续显示
    if (connected && (current_data.mode == kDabbleModeJoystick || current_data.mode == kDabbleModeAccelerometer))
    {
        ShowMovementIndicator(current_data);
    }

    // 5. 灯光状态
    bool dance_light = server_->IsDanceLightEnabled();
    bool night_light = server_->IsNightLightEnabled();
    if (dance_light != last_dance_light_ || night_light != last_night_light_)
    {
        ShowLightStatus(dance_light, night_light);
        last_dance_light_ = dance_light;
        last_night_light_ = night_light;
    }
}

void BtGamepadDisplay::Update()
{
    RefreshDisplay();
}

// ==================== 具体显示功能 ====================

void BtGamepadDisplay::ShowWelcomeScreen()
{
    if (display_ == nullptr || server_ == nullptr)
    {
        return;
    }

    char welcome_msg[128];
    snprintf(welcome_msg, sizeof(welcome_msg),
             "🎮 蓝牙摇杆模式\n"
             "设备名: %s\n"
             "等待连接...",
             server_->GetDeviceName());

    display_->ShowNotification(welcome_msg, 3000);
    display_->SetStatus("🎮 BT摇杆");

    ESP_LOGI(BT_DISPLAY_TAG, "显示欢迎界面");
}

void BtGamepadDisplay::ShowConnectionStatus(bool connected)
{
    if (display_ == nullptr)
    {
        return;
    }

    if (connected)
    {
        display_->ShowNotification("✅ 蓝牙已连接", 2000);
        display_->SetStatus("🎮 已连接");
        ESP_LOGI(BT_DISPLAY_TAG, "显示: 蓝牙已连接");

        // 延迟显示按钮提示
        vTaskDelay(pdMS_TO_TICKS(2000));
        ShowButtonGuide();
    }
    else
    {
        display_->ShowNotification("❌ 蓝牙已断开\n等待重连...", 3000);
        display_->SetStatus("🎮 未连接");
        ESP_LOGI(BT_DISPLAY_TAG, "显示: 蓝牙已断开");
    }
}

void BtGamepadDisplay::ShowControlMode(DabbleGamepadMode mode)
{
    if (display_ == nullptr)
    {
        return;
    }

    const char *mode_name = GetModeName(mode);
    char mode_msg[64];
    snprintf(mode_msg, sizeof(mode_msg), "📡 模式: %s", mode_name);

    display_->ShowNotification(mode_msg, 1500);

    ESP_LOGI(BT_DISPLAY_TAG, "显示控制模式: %s", mode_name);
}

void BtGamepadDisplay::ShowMovementIndicator(const DabbleGamepadData &data)
{
    if (display_ == nullptr)
    {
        return;
    }

    // 构建移动指示字符串
    char indicator[64];

    if (data.mode == kDabbleModeJoystick)
    {
        // 摇杆模式: 显示角度和速度
        int speed_percent = (data.radius * 100) / 7; // radius: 0-7
        snprintf(indicator, sizeof(indicator),
                 "🕹️ %d° | %d%%",
                 data.angle, speed_percent);
    }
    else if (data.mode == kDabbleModeAccelerometer)
    {
        // 加速度计模式: 显示倾斜方向
        const char *direction = "水平";
        if (data.axis_y > 30)
            direction = "前倾";
        else if (data.axis_y < -30)
            direction = "后倾";
        else if (data.axis_x > 30)
            direction = "右倾";
        else if (data.axis_x < -30)
            direction = "左倾";

        snprintf(indicator, sizeof(indicator), "📱 %s", direction);
    }

    display_->SetStatus(indicator);
}

void BtGamepadDisplay::ShowButtonFeedback(uint16_t buttons)
{
    if (display_ == nullptr || buttons == 0)
    {
        return;
    }

    // 检测按钮按下（与上次不同的按钮）
    uint16_t new_buttons = buttons & ~last_buttons_;

    if (new_buttons == 0)
    {
        return; // 没有新按钮
    }

    // 找到第一个按下的按钮并显示
    const char *button_name = nullptr;
    if (new_buttons & kDabbleButtonStart)
        button_name = "⏹️ STOP";
    else if (new_buttons & kDabbleButton1)
        button_name = "🛑 停止";
    else if (new_buttons & kDabbleButton2)
        button_name = "💃 跳舞";
    else if (new_buttons & kDabbleButton3)
        button_name = "✨ 派对灯";
    else if (new_buttons & kDabbleButton4)
        button_name = "💡 夜光";

    if (button_name != nullptr)
    {
        display_->ShowNotification(button_name, 1000);
        ESP_LOGI(BT_DISPLAY_TAG, "按钮反馈: %s", button_name);
    }
}

void BtGamepadDisplay::ShowLightStatus(bool dance_light, bool night_light)
{
    if (display_ == nullptr)
    {
        return;
    }

    char light_msg[64];

    if (dance_light && night_light)
    {
        snprintf(light_msg, sizeof(light_msg), "✨💡 灯光: 派对+夜光");
    }
    else if (dance_light)
    {
        snprintf(light_msg, sizeof(light_msg), "✨ 派对灯光: 开启");
    }
    else if (night_light)
    {
        snprintf(light_msg, sizeof(light_msg), "💡 夜光: 开启");
    }
    else
    {
        snprintf(light_msg, sizeof(light_msg), "💡 灯光: 关闭");
    }

    display_->ShowNotification(light_msg, 1500);
    ESP_LOGI(BT_DISPLAY_TAG, "灯光状态: %s", light_msg);
}

void BtGamepadDisplay::ShowButtonGuide()
{
    if (display_ == nullptr)
    {
        return;
    }

    const char *guide =
        "🎮 按钮功能:\n"
        "❌ 停止移动\n"
        "⭕ 跳舞\n"
        "🔺 派对灯\n"
        "🟦 夜光\n"
        "START = 紧急停止";

    display_->ShowNotification(guide, 5000);
    ESP_LOGI(BT_DISPLAY_TAG, "显示按钮提示");
}

// ==================== 辅助函数 ====================

const char *BtGamepadDisplay::GetModeName(DabbleGamepadMode mode)
{
    switch (mode)
    {
    case kDabbleModeDigital:
        return "数字键";
    case kDabbleModeJoystick:
        return "摇杆";
    case kDabbleModeAccelerometer:
        return "加速度计";
    default:
        return "未知";
    }
}

const char *BtGamepadDisplay::GetButtonName(uint16_t button)
{
    switch (button)
    {
    case kDabbleButtonStart:
        return "START";
    case kDabbleButton1:
        return "CROSS (停止)";
    case kDabbleButton2:
        return "CIRCLE (跳舞)";
    case kDabbleButton3:
        return "TRIANGLE (派对灯)";
    case kDabbleButton4:
        return "SQUARE (夜光)";
    default:
        return "未知按钮";
    }
}
