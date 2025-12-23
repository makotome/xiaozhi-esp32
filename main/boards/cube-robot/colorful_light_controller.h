/*
    colorful_light_controller.h
    Otto HP Robot 彩色灯光控制器
    利用LCD彩屏实现各种灯光效果
    Author: Xumx
    Date: 2025-11-14
    Version: 1.0
*/

#ifndef COLORFUL_LIGHT_CONTROLLER_H
#define COLORFUL_LIGHT_CONTROLLER_H

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lvgl.h>

class OttoEmojiDisplay;  // 前向声明

/**
 * @brief 彩色灯光效果控制器
 *
 * 使用LCD彩屏实现各种灯光效果，支持：
 * - 夜灯模式（纯白色）
 * - 跳舞模式（五彩缤纷）
 * - 呼吸灯效果
 * - 彩虹渐变效果
 * - 闪烁效果
 *
 * 不影响原有的表情显示功能
 */
class ColorfulLightController {
 public:
  // 灯光模式枚举
  enum LightMode {
    MODE_OFF = 0,      // 关闭灯光效果（显示正常界面）
    MODE_NIGHT_LIGHT,  // 夜灯模式（纯白色）
    MODE_DANCE_PARTY,  // 跳舞派对模式（五彩缤纷）
    MODE_BREATHING,    // 呼吸灯效果
    MODE_RAINBOW,      // 彩虹渐变
    MODE_FLASH,        // 闪烁效果
    MODE_WARM_LIGHT,   // 暖光模式（淡黄色）
    MODE_COOL_LIGHT,   // 冷光模式（淡蓝色）
    MODE_BLUE_FLASH    // 蓝光闪烁（蓝牙等待连接）
  };

  ColorfulLightController(OttoEmojiDisplay* display);
  ~ColorfulLightController();

  // 初始化灯光控制器
  bool init();

  // 设置灯光模式
  void setLightMode(LightMode mode);

  // 获取当前模式
  LightMode getCurrentMode() const { return current_mode_; }

  // 设置亮度 (0-100)
  void setBrightness(int brightness);

  // 获取当前亮度
  int getBrightness() const { return brightness_; }

  // 停止所有灯光效果，恢复正常显示
  void stopAllEffects();

  // 测试所有灯光效果
  void testAllLights();

 private:
  OttoEmojiDisplay* display_;
  lv_obj_t* light_overlay_;  // 灯光效果覆盖层
  TaskHandle_t light_task_handle_;
  LightMode current_mode_;
  int brightness_;  // 亮度 0-100
  bool is_running_;
  bool effect_active_;  // 是否有灯光效果激活

  // 灯光效果任务
  static void LightEffectTask(void* arg);

  // 各种灯光效果实现
  void showNightLight();
  void showDanceParty();
  void showBreathing();
  void showRainbow();
  void showFlash();
  void showWarmLight();
  void showCoolLight();
  void showBlueFlash();  // 蓝光闪烁（蓝牙等待连接）

  // 辅助函数
  void createLightOverlay();
  void destroyLightOverlay();
  void setOverlayColor(lv_color_t color, uint8_t opacity);
  lv_color_t hsvToRgb(float h, float s, float v);
};

#endif  // COLORFUL_LIGHT_CONTROLLER_H
