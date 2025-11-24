#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include <wifi_station.h>

#include "application.h"
#include "codecs/no_audio_codec.h"
#include "button.h"
#include "config.h"
#include "display/lcd_display.h"
#include "lamp_controller.h"
#include "led/single_led.h"
#include "light_mcp_controller.h"
#include "mcp_server.h"
#include "otto_emoji_display.h"
#include "power_manager.h"
#include "system_reset.h"
#include "wifi_board.h"
#include "wheel_robot_controller.h"
#include "remote_control_integration.h"
#include "bt_gamepad_integration.h"

#define TAG "OttoHpRobot"

#define WHEEL_ROBOT_ENABLED 1

extern void InitializeOttoController();
extern void InitializeWheelRobotController();

// 前向声明：获取全局控制器实例
class LightMcpController;
class WheelRobotController;
extern LightMcpController *GetLightMcpController();
extern WheelRobotController *GetWheelRobotController();

class OttoHpRobot : public WifiBoard
{
private:
    LcdDisplay *display_;
    PowerManager *power_manager_;
    Button boot_button_;
    Button mode_button_;

    void InitializePowerManager()
    {
        power_manager_ =
            new PowerManager(POWER_CHARGE_DETECT_PIN, POWER_ADC_UNIT, POWER_ADC_CHANNEL);
    }

    void InitializeSpi()
    {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_CLK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeLcdDisplay()
    {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN; // GPIO_NUM_NC，表示CS直接接地
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = 10 * 1000 * 1000; // 降低到 10MHz 提高兼容性
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;

        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));

        esp_lcd_panel_reset(panel);
        vTaskDelay(pdMS_TO_TICKS(100)); // 增加复位延时
        esp_lcd_panel_init(panel);

        // 1.54寸 ST7789 特定初始化命令
        uint8_t cmd_11[] = {0x11}; // 睡眠唤醒
        esp_lcd_panel_io_tx_param(panel_io, 0x11, cmd_11, sizeof(cmd_11));
        vTaskDelay(pdMS_TO_TICKS(120));

        // 设置列地址 (CASET) - 针对1.54寸屏幕的偏移
        uint8_t caset[] = {0x00, 0x00, 0x00, 0xEF}; // 0-239
        esp_lcd_panel_io_tx_param(panel_io, 0x2A, caset, sizeof(caset));

        // 设置行地址 (RASET) - 针对1.54寸屏幕，无Y偏移 (0-239)
        uint8_t raset[] = {0x00, 0x00, 0x00, 0xEF}; // 0-239
        esp_lcd_panel_io_tx_param(panel_io, 0x2B, raset, sizeof(raset));

        uint8_t cmd_29[] = {0x29}; // 开启显示
        esp_lcd_panel_io_tx_param(panel_io, 0x29, cmd_29, sizeof(cmd_29));

        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);

        display_ = new OttoEmojiDisplay(
            panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
            DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeButtons()
    {
        boot_button_.OnClick([this]()
                             {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting &&
                !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState(); });

        // MODE_BUTTON 点击切换模式（三模式循环）
        mode_button_.OnClick([this]()
                             {
            // 循环切换: 小智 -> WiFi遥控 -> 蓝牙摇杆 -> 小智
            ModeManager::GetInstance().ToggleMode();

            auto current_mode = ModeManager::GetInstance().GetCurrentMode();
            const char *mode_name = ModeManager::GetModeName(current_mode);

            ESP_LOGI(TAG, "=== 已切换到: %s ===", mode_name);

            // 在显示屏上显示当前模式
            if (display_) {
                display_->ShowNotification(mode_name);
            }

            // 根据模式显示额外信息
            if (current_mode == kModeRemoteControl)
            {
                ESP_LOGI(TAG, "访问地址: %s", GetRemoteControlUrl());
                if (display_) {
                    display_->ShowNotification(GetRemoteControlUrl());
                }
            }
            else if (current_mode == kModeBtGamepad)
            {
                ESP_LOGI(TAG, "蓝牙设备名: %s", GetBtDeviceName());
                if (display_) {
                    display_->ShowNotification("请在手机蓝牙中搜索:");
                    display_->ShowNotification(GetBtDeviceName());
                }
            } });
    }

    void InitializeOttoController()
    {
        ESP_LOGI(TAG, "初始化Otto机器人MCP控制器");
        ::InitializeOttoController();
    }

    void InitializeWheelRobotController()
    {
        ESP_LOGI(TAG, "初始化Otto机器人轮子控制器");
        ::InitializeWheelRobotController();
    }

    void InitializeLightController()
    {
        ESP_LOGI(TAG, "初始化彩色灯光控制器");
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

    void RegisterAllMcpTools()
    {
        ESP_LOGI(TAG, "统一注册所有MCP工具");

        // 注册灯光控制器的MCP工具
        auto *light_controller = GetLightMcpController();
        if (light_controller)
        {
            light_controller->RegisterMcpTools();
            ESP_LOGI(TAG, "灯光MCP工具注册完成");
        }

        // 注册轮子控制器的MCP工具
        auto *wheel_controller = GetWheelRobotController();
        if (wheel_controller)
        {
            wheel_controller->RegisterMcpTools();
            ESP_LOGI(TAG, "轮子MCP工具注册完成");
        }

        ESP_LOGI(TAG, "所有MCP工具注册完成");
    }

public:
    OttoHpRobot() : boot_button_(BOOT_BUTTON_GPIO), mode_button_(MODE_BUTTON_GPIO)
    {
        InitializeSpi();
        InitializeLcdDisplay();
        InitializeButtons();
        InitializePowerManager();
        InitializeLightController();
#if WHEEL_ROBOT_ENABLED
        InitializeWheelRobotController();
#else
        InitializeOttoController();
#endif
        RegisterAllMcpTools(); // 在所有控制器初始化后注册MCP工具

        // 初始化遥控模式功能
        InitializeRemoteControlMode();

        // 初始化蓝牙摇杆模式（传入显示对象以启用UI）
        InitializeBtGamepadMode(display_);

        GetBacklight()->RestoreBrightness();

        ESP_LOGI(TAG, "Otto HP Robot 初始化完成");
        ESP_LOGI(TAG, "按 MODE_BUTTON (GPIO_%d) 切换模式", MODE_BUTTON_GPIO);
        ESP_LOGI(TAG, "模式循环: 小智 -> WiFi遥控 -> 蓝牙摇杆");
    }

    virtual AudioCodec *GetAudioCodec() override
    {
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                               AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK,
                                               AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK,
                                               AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
        return &audio_codec;
    }

    virtual Display *GetDisplay() override { return display_; }

    virtual Backlight *GetBacklight() override
    {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        backlight.SetBrightness(100); // 强制设置最大亮度，排除背光问题
        return &backlight;
    }

    virtual bool GetBatteryLevel(int &level, bool &charging, bool &discharging) override
    {
        charging = power_manager_->IsCharging();
        discharging = !charging;
        level = power_manager_->GetBatteryLevel();
        return true;
    }
};

DECLARE_BOARD(OttoHpRobot);
