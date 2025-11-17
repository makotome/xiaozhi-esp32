/*
    integration_patch.h
    集成彩色灯光控制器的示例代码

    将这些代码片段添加到 otto_hp_robot.cc 中
*/

// ============================================================
// 第1步: 在文件顶部添加头文件引用
// ============================================================
#include "light_mcp_controller.h"

// ============================================================
// 第2步: 在 OttoHpRobot 类的 private 部分添加初始化函数
// ============================================================
private:
void InitializeLightController()
{
    ESP_LOGI(TAG, "初始化彩色灯光控制器");
    // 确保在display_初始化后调用
    auto *otto_display = dynamic_cast<OttoEmojiDisplay *>(display_);
    if (otto_display)
    {
        InitializeLightMcpController(otto_display);
    }
    else
    {
        ESP_LOGW(TAG, "Display is not OttoEmojiDisplay, skip light controller");
    }
}

// ============================================================
// 第3步: 在构造函数中调用初始化函数
// 在 InitializeLcdDisplay() 之后，其他初始化之前调用
// ============================================================
public:
OttoHpRobot() : boot_button_(BOOT_BUTTON_GPIO)
{
    InitializeSpi();
    InitializeLcdDisplay();
    InitializeButtons();
    InitializePowerManager();

    // ⭐ 在这里添加灯光控制器初始化（在display_初始化之后）
    InitializeLightController();

#if WHEEL_ROBOT_ENABLED
    InitializeWheelRobotController();
#else
    InitializeOttoController();
#endif
    GetBacklight()->RestoreBrightness();
}

// ============================================================
// 完成！现在灯光控制器已经集成，可以通过MCP工具使用了
// ============================================================
