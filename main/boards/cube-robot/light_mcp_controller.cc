/*
    light_mcp_controller.cc
    彩色灯光的MCP控制接口实现
*/

#include "light_mcp_controller.h"

#include <esp_log.h>

#include "mcp_server.h"
#include "otto_emoji_display.h"

#define TAG "LightMcpController"

// 全局实例
static LightMcpController* g_light_mcp_controller = nullptr;

LightMcpController::LightMcpController(OttoEmojiDisplay* display)
    : light_controller_(nullptr) {
  light_controller_ = new ColorfulLightController(display);
}

LightMcpController::~LightMcpController() {
  if (light_controller_) {
    delete light_controller_;
    light_controller_ = nullptr;
  }
}

bool LightMcpController::init() {
  if (!light_controller_) {
    ESP_LOGE(TAG, "Light controller is nullptr");
    return false;
  }

  if (!light_controller_->init()) {
    ESP_LOGE(TAG, "Failed to initialize light controller");
    return false;
  }

  ESP_LOGI(TAG, "灯光MCP控制器初始化成功");

  return true;
}

void LightMcpController::RegisterMcpTools() {
  auto& mcp_server = McpServer::GetInstance();

  ESP_LOGI(TAG, "开始注册灯光MCP工具...");

  // 1. 夜灯模式
  mcp_server.AddTool("self.light.night_light", "打开夜灯模式（纯白色灯光）",
                     PropertyList(),
                     [](const PropertyList& properties) -> ReturnValue {
                       auto* controller = GetLightMcpController();
                       if (!controller || !controller->getLightController()) {
                         return "灯光控制器未初始化";
                       }
                       controller->getLightController()->setLightMode(
                           ColorfulLightController::MODE_NIGHT_LIGHT);
                       return "夜灯已开启 💡";
                     });

  // 2. 跳舞灯光
  mcp_server.AddTool("self.light.dance_party",
                     "开启跳舞派对灯光（五彩缤纷快速变换）", PropertyList(),
                     [](const PropertyList& properties) -> ReturnValue {
                       auto* controller = GetLightMcpController();
                       if (!controller || !controller->getLightController()) {
                         return "灯光控制器未初始化";
                       }
                       controller->getLightController()->setLightMode(
                           ColorfulLightController::MODE_DANCE_PARTY);
                       return "跳舞派对灯光已开启 🎉";
                     });

  // 3. 呼吸灯
  mcp_server.AddTool("self.light.breathing", "开启呼吸灯效果（蓝色呼吸）",
                     PropertyList(),
                     [](const PropertyList& properties) -> ReturnValue {
                       auto* controller = GetLightMcpController();
                       if (!controller || !controller->getLightController()) {
                         return "灯光控制器未初始化";
                       }
                       controller->getLightController()->setLightMode(
                           ColorfulLightController::MODE_BREATHING);
                       return "呼吸灯效果已开启 🌙";
                     });

  // 4. 彩虹灯
  mcp_server.AddTool("self.light.rainbow", "开启彩虹渐变效果", PropertyList(),
                     [](const PropertyList& properties) -> ReturnValue {
                       auto* controller = GetLightMcpController();
                       if (!controller || !controller->getLightController()) {
                         return "灯光控制器未初始化";
                       }
                       controller->getLightController()->setLightMode(
                           ColorfulLightController::MODE_RAINBOW);
                       return "彩虹灯光已开启 🌈";
                     });

  // 5. 闪烁灯
  mcp_server.AddTool("self.light.flash", "开启闪烁效果", PropertyList(),
                     [](const PropertyList& properties) -> ReturnValue {
                       auto* controller = GetLightMcpController();
                       if (!controller || !controller->getLightController()) {
                         return "灯光控制器未初始化";
                       }
                       controller->getLightController()->setLightMode(
                           ColorfulLightController::MODE_FLASH);
                       return "闪烁灯光已开启 ⚡";
                     });

  // 6. 暖光
  mcp_server.AddTool("self.light.warm", "开启暖光模式（淡黄色）",
                     PropertyList(),
                     [](const PropertyList& properties) -> ReturnValue {
                       auto* controller = GetLightMcpController();
                       if (!controller || !controller->getLightController()) {
                         return "灯光控制器未初始化";
                       }
                       controller->getLightController()->setLightMode(
                           ColorfulLightController::MODE_WARM_LIGHT);
                       return "暖光模式已开启 ☀️";
                     });

  // 7. 冷光
  mcp_server.AddTool("self.light.cool", "开启冷光模式（淡蓝色）",
                     PropertyList(),
                     [](const PropertyList& properties) -> ReturnValue {
                       auto* controller = GetLightMcpController();
                       if (!controller || !controller->getLightController()) {
                         return "灯光控制器未初始化";
                       }
                       controller->getLightController()->setLightMode(
                           ColorfulLightController::MODE_COOL_LIGHT);
                       return "冷光模式已开启 ❄️";
                     });

  // 8. 关闭灯光
  mcp_server.AddTool("self.light.off", "关闭所有灯光效果，恢复正常显示",
                     PropertyList(),
                     [](const PropertyList& properties) -> ReturnValue {
                       auto* controller = GetLightMcpController();
                       if (!controller || !controller->getLightController()) {
                         return "灯光控制器未初始化";
                       }
                       controller->getLightController()->stopAllEffects();
                       return "灯光已关闭，恢复正常显示";
                     });

  // 9. 获取灯光状态
  mcp_server.AddTool(
      "self.light.get_status", "获取当前灯光状态（模式和亮度）", PropertyList(),
      [](const PropertyList& properties) -> ReturnValue {
        auto* controller = GetLightMcpController();
        if (!controller || !controller->getLightController()) {
          return "{\"error\":\"灯光控制器未初始化\"}";
        }

        int mode = static_cast<int>(
            controller->getLightController()->getCurrentMode());
        int brightness = controller->getLightController()->getBrightness();

        const char* mode_names[] = {"关闭",     "夜灯", "跳舞派对", "呼吸灯",
                                    "彩虹渐变", "闪烁", "暖光",     "冷光"};

        std::string result = "{\"mode\":" + std::to_string(mode) +
                             ",\"mode_name\":\"" + mode_names[mode] + "\"" +
                             ",\"brightness\":" + std::to_string(brightness) +
                             "}";
        return result;
      });

  ESP_LOGI(TAG, "灯光MCP工具注册完成 - 共11个工具");
}

// ==================== 全局函数 ====================

void InitializeLightMcpController(OttoEmojiDisplay* display) {
  if (g_light_mcp_controller == nullptr && display != nullptr) {
    g_light_mcp_controller = new LightMcpController(display);
    g_light_mcp_controller->init();
    ESP_LOGI(TAG, "全局灯光MCP控制器已创建并初始化");
  }
}

LightMcpController* GetLightMcpController() { return g_light_mcp_controller; }
