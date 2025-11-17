/*
    light_mcp_controller.h
    彩色灯光的MCP控制接口
    将灯光控制功能注册为MCP工具
*/

#ifndef LIGHT_MCP_CONTROLLER_H
#define LIGHT_MCP_CONTROLLER_H

#include "colorful_light_controller.h"

class OttoEmojiDisplay;

class LightMcpController
{
public:
    LightMcpController(OttoEmojiDisplay *display);
    ~LightMcpController();

    // 初始化并注册MCP工具
    bool init();

    // 注册灯光相关的MCP工具
    void RegisterMcpTools();

    // 获取灯光控制器
    ColorfulLightController *getLightController() { return light_controller_; }

private:
    ColorfulLightController *light_controller_;
};

// 全局初始化函数
void InitializeLightMcpController(OttoEmojiDisplay *display);

// 获取全局实例
LightMcpController *GetLightMcpController();

#endif // LIGHT_MCP_CONTROLLER_H
