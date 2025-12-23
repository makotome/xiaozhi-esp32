/*
    colorful_light_controller.cc
    Otto HP Robot 彩色灯光控制器实现
*/

#include "colorful_light_controller.h"

#include <esp_log.h>

#include <cmath>

#include "otto_emoji_display.h"

#define TAG "ColorfulLight"

ColorfulLightController::ColorfulLightController(OttoEmojiDisplay* display)
    : display_(display),
      light_overlay_(nullptr),
      light_task_handle_(nullptr),
      current_mode_(MODE_OFF),
      brightness_(80),
      is_running_(false),
      effect_active_(false) {}

ColorfulLightController::~ColorfulLightController() {
  stopAllEffects();

  if (light_task_handle_ != nullptr) {
    vTaskDelete(light_task_handle_);
    light_task_handle_ = nullptr;
  }
}

bool ColorfulLightController::init() {
  if (!display_) {
    ESP_LOGE(TAG, "Display is nullptr");
    return false;
  }

  ESP_LOGI(TAG, "彩色灯光控制器初始化成功");
  return true;
}

void ColorfulLightController::testAllLights() {
  ESP_LOGI(TAG, "测试所有灯光接口");

  // 先创建灯光覆盖层，否则所有的灯光效果都不会显示
  createLightOverlay();

  // 给LVGL一些时间来创建和渲染覆盖层
  vTaskDelay(pdMS_TO_TICKS(100));

  ESP_LOGI(TAG, "测试夜灯效果");
  showNightLight();  // 夜灯
  vTaskDelay(pdMS_TO_TICKS(3000));

  ESP_LOGI(TAG, "测试跳舞派对效果");
  // 跳舞派对需要持续调用才能看到变换效果
  for (int i = 0; i < 60; i++) {  // 3秒，每50ms一次
    showDanceParty();
    vTaskDelay(pdMS_TO_TICKS(50));
  }

  ESP_LOGI(TAG, "测试呼吸灯效果");
  // 呼吸灯也需要持续调用
  for (int i = 0; i < 100; i++) {  // 3秒，每30ms一次
    showBreathing();
    vTaskDelay(pdMS_TO_TICKS(30));
  }

  ESP_LOGI(TAG, "测试彩虹渐变效果");
  // 彩虹渐变需要持续调用
  for (int i = 0; i < 60; i++) {  // 3秒，每50ms一次
    showRainbow();
    vTaskDelay(pdMS_TO_TICKS(50));
  }

  ESP_LOGI(TAG, "测试闪烁效果");
  // 闪烁效果需要持续调用
  for (int i = 0; i < 30; i++) {  // 3秒，每100ms一次
    showFlash();
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  ESP_LOGI(TAG, "测试暖光效果");
  showWarmLight();  // 暖光
  vTaskDelay(pdMS_TO_TICKS(3000));

  ESP_LOGI(TAG, "测试冷光效果");
  showCoolLight();  // 冷光
  vTaskDelay(pdMS_TO_TICKS(3000));

  // 测试完成后停止所有效果并销毁覆盖层
  stopAllEffects();
  ESP_LOGI(TAG, "所有灯光接口测试完成");
}

void ColorfulLightController::setLightMode(LightMode mode) {
  if (current_mode_ == mode) {
    return;
  }

  ESP_LOGI(TAG, "设置灯光模式: %d", mode);
  current_mode_ = mode;

  // 如果是关闭模式，停止所有效果
  if (mode == MODE_OFF) {
    stopAllEffects();
    return;
  }

  // 创建灯光覆盖层（如果还没有创建）
  if (!effect_active_) {
    createLightOverlay();
  }

  // 启动灯光效果任务（如果还没有启动）
  if (light_task_handle_ == nullptr) {
    is_running_ = true;
    xTaskCreate(LightEffectTask, "light_effect", 4096, this, 5,
                &light_task_handle_);
  }
}

void ColorfulLightController::setBrightness(int brightness) {
  brightness_ = std::max(0, std::min(100, brightness));
  ESP_LOGI(TAG, "设置亮度: %d%%", brightness_);
}

void ColorfulLightController::stopAllEffects() {
  ESP_LOGI(TAG, "停止所有灯光效果，恢复正常显示");
  current_mode_ = MODE_OFF;
  is_running_ = false;

  // 等待任务结束
  if (light_task_handle_ != nullptr) {
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  destroyLightOverlay();
}

void ColorfulLightController::createLightOverlay() {
  if (effect_active_ || !display_) {
    ESP_LOGW(TAG, "无法创建灯光覆盖层: effect_active_=%d, display_=%p",
             effect_active_, display_);
    return;
  }

  ESP_LOGI(TAG, "创建灯光覆盖层");

  // 锁定LVGL
  DisplayLockGuard lock(display_);

  // 使用顶层图层而不是普通屏幕，这样可以确保覆盖层在最上面
  lv_obj_t* top_layer = lv_layer_top();
  ESP_LOGI(TAG, "顶层对象: %p", top_layer);

  light_overlay_ = lv_obj_create(top_layer);

  if (light_overlay_ == nullptr) {
    ESP_LOGE(TAG, "创建覆盖层对象失败");
    return;
  }

  // 设置为全屏
  lv_obj_set_size(light_overlay_, LV_PCT(100), LV_PCT(100));
  lv_obj_set_pos(light_overlay_, 0, 0);

  // 关键设置：禁用所有交互，只作为覆盖层
  lv_obj_remove_flag(light_overlay_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(light_overlay_, LV_OBJ_FLAG_CLICKABLE);

  // 移到最顶层，并设置最高层级
  lv_obj_move_foreground(light_overlay_);
  lv_obj_move_to_index(light_overlay_,
                       -1);  // 移到父对象的最后一个子对象位置（最顶层）

  // 移除边框和内边距
  lv_obj_set_style_border_width(light_overlay_, 0, 0);
  lv_obj_set_style_pad_all(light_overlay_, 0, 0);
  lv_obj_set_style_radius(light_overlay_, 0, 0);

  // 设置为不透明，初始颜色为红色以便测试
  lv_obj_set_style_bg_opa(light_overlay_, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(light_overlay_, lv_color_make(255, 0, 0), 0);

  effect_active_ = true;

  ESP_LOGI(TAG, "灯光覆盖层创建成功: overlay=%p, parent=%p", light_overlay_,
           lv_obj_get_parent(light_overlay_));
}
void ColorfulLightController::destroyLightOverlay() {
  if (!effect_active_ || !display_) {
    return;
  }

  ESP_LOGI(TAG, "销毁灯光覆盖层");

  // 锁定LVGL
  DisplayLockGuard lock(display_);

  if (light_overlay_ != nullptr) {
    lv_obj_del(light_overlay_);
    light_overlay_ = nullptr;
  }

  effect_active_ = false;

  // 强制刷新整个屏幕，确保底层内容重新显示
  lv_obj_invalidate(lv_screen_active());

  ESP_LOGI(TAG, "灯光覆盖层已销毁，屏幕已刷新");
}

void ColorfulLightController::setOverlayColor(lv_color_t color,
                                              uint8_t opacity) {
  if (!effect_active_ || !light_overlay_ || !display_) {
    ESP_LOGW(TAG,
             "setOverlayColor失败: effect_active_=%d, light_overlay_=%p, "
             "display_=%p",
             effect_active_, light_overlay_, display_);
    return;
  }

  DisplayLockGuard lock(display_);

  // 确保覆盖层在最顶层（关键修复）
  lv_obj_move_foreground(light_overlay_);

  lv_obj_set_style_bg_color(light_overlay_, color, 0);
  lv_obj_set_style_bg_opa(light_overlay_, opacity, 0);

  // 立即刷新对象以确保显示更新
  lv_obj_invalidate(light_overlay_);

  ESP_LOGI(TAG, "设置覆盖层颜色: R=%d G=%d B=%d, 透明度=%d", color.red,
           color.green, color.blue, opacity);
}

lv_color_t ColorfulLightController::hsvToRgb(float h, float s, float v) {
  // HSV to RGB 转换算法
  float c = v * s;
  float x = c * (1 - fabs(fmod(h / 60.0, 2) - 1));
  float m = v - c;

  float r, g, b;

  if (h < 60) {
    r = c;
    g = x;
    b = 0;
  } else if (h < 120) {
    r = x;
    g = c;
    b = 0;
  } else if (h < 180) {
    r = 0;
    g = c;
    b = x;
  } else if (h < 240) {
    r = 0;
    g = x;
    b = c;
  } else if (h < 300) {
    r = x;
    g = 0;
    b = c;
  } else {
    r = c;
    g = 0;
    b = x;
  }

  return lv_color_make((r + m) * 255, (g + m) * 255, (b + m) * 255);
}

// ==================== 灯光效果实现 ====================

void ColorfulLightController::showNightLight() {
  // 纯白色夜灯 - 完全不透明
  setOverlayColor(lv_color_white(), LV_OPA_COVER);
}

void ColorfulLightController::showDanceParty() {
  // 跳舞派对：五颜六色快速变换，饱和度高，完全不透明
  static float hue = 0;

  // 快速变换颜色，步进更大
  hue += 30.0f;
  if (hue >= 360.0f) {
    hue = 0;
  }

  lv_color_t color = hsvToRgb(hue, 1.0f, 1.0f);
  setOverlayColor(color, LV_OPA_COVER);  // 完全不透明
}

void ColorfulLightController::showBreathing() {
  // 呼吸灯效果：蓝色呼吸
  static float brightness_cycle = 0;
  static float direction = 1.0f;

  brightness_cycle += direction * 0.03f;

  if (brightness_cycle >= 1.0f) {
    brightness_cycle = 1.0f;
    direction = -1.0f;
  } else if (brightness_cycle <= 0.3f) {
    brightness_cycle = 0.3f;
    direction = 1.0f;
  }

  // 使用更亮的蓝色，透明度更高
  uint8_t opacity = (uint8_t)(brightness_cycle * 255);
  lv_color_t color = lv_color_make(80, 120, 255);  // 更亮的蓝色
  setOverlayColor(color, opacity);
}

void ColorfulLightController::showRainbow() {
  // 彩虹渐变：慢速渐变，完全不透明
  static float hue = 0;

  hue += 2.0f;
  if (hue >= 360.0f) {
    hue = 0;
  }

  lv_color_t color = hsvToRgb(hue, 1.0f, 1.0f);
  setOverlayColor(color, LV_OPA_COVER);  // 完全不透明
}

void ColorfulLightController::showFlash() {
  // 闪烁效果 - 白色闪烁，完全不透明
  static bool flash_on = false;
  static int counter = 0;

  counter++;
  if (counter >= 3) {  // 每3帧切换一次
    flash_on = !flash_on;
    counter = 0;
  }

  if (flash_on) {
    setOverlayColor(lv_color_white(), LV_OPA_COVER);
  } else {
    setOverlayColor(lv_color_black(), LV_OPA_TRANSP);
  }
}

void ColorfulLightController::showWarmLight() {
  // 暖光模式：淡黄色，完全不透明
  lv_color_t color = lv_color_make(255, 200, 120);  // 更明显的暖黄色
  setOverlayColor(color, LV_OPA_COVER);
}

void ColorfulLightController::showCoolLight() {
  // 冷光模式：淡蓝色，完全不透明
  lv_color_t color = lv_color_make(180, 220, 255);  // 冷蓝色
  setOverlayColor(color, LV_OPA_COVER);
}

void ColorfulLightController::showBlueFlash() {
  // 蓝光闪烁效果 - 用于蓝牙等待连接
  static bool flash_on = false;
  static int counter = 0;

  counter++;
  if (counter >= 5) {  // 每5帧切换一次（约500ms，因为调用间隔是100ms）
    flash_on = !flash_on;
    counter = 0;
  }

  if (flash_on) {
    // 明亮的蓝色
    lv_color_t color = lv_color_make(0, 100, 255);
    setOverlayColor(color, LV_OPA_COVER);
  } else {
    // 深蓝色（暗淡）
    lv_color_t color = lv_color_make(0, 30, 80);
    setOverlayColor(color, LV_OPA_70);
  }
}

// ==================== 灯光效果任务 ====================

void ColorfulLightController::LightEffectTask(void* arg) {
  ColorfulLightController* controller =
      static_cast<ColorfulLightController*>(arg);

  ESP_LOGI(TAG, "灯光效果任务启动");

  while (controller->is_running_) {
    switch (controller->current_mode_) {
      case MODE_NIGHT_LIGHT:
        controller->showNightLight();
        vTaskDelay(pdMS_TO_TICKS(100));
        break;

      case MODE_DANCE_PARTY:
        controller->showDanceParty();
        vTaskDelay(pdMS_TO_TICKS(50));  // 快速变换
        break;

      case MODE_BREATHING:
        controller->showBreathing();
        vTaskDelay(pdMS_TO_TICKS(30));
        break;

      case MODE_RAINBOW:
        controller->showRainbow();
        vTaskDelay(pdMS_TO_TICKS(50));
        break;

      case MODE_FLASH:
        controller->showFlash();
        vTaskDelay(pdMS_TO_TICKS(100));
        break;

      case MODE_WARM_LIGHT:
        controller->showWarmLight();
        vTaskDelay(pdMS_TO_TICKS(100));
        break;

      case MODE_COOL_LIGHT:
        controller->showCoolLight();
        vTaskDelay(pdMS_TO_TICKS(100));
        break;

      case MODE_BLUE_FLASH:
        controller->showBlueFlash();
        vTaskDelay(pdMS_TO_TICKS(100));
        break;

      case MODE_OFF:
      default:
        vTaskDelay(pdMS_TO_TICKS(100));
        break;
    }
  }

  ESP_LOGI(TAG, "灯光效果任务结束");
  controller->light_task_handle_ = nullptr;
  vTaskDelete(nullptr);
}
