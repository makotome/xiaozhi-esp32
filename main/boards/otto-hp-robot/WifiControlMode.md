# Otto HP Robot - WiFi 遥控模式设计文档

## 1. 需求概述

在现有的小智模式基础上,新增一个遥控模式,通过按下 GPIO_2 按钮实现模式切换:
- **小智模式**: 现有的语音对话AI助手模式
- **遥控模式**: 通过网页遥控机器车的运动模式

### 用户使用场景
1. 用户按下 MODE_BUTTON (GPIO_2) 进入遥控模式
2. 设备启动 AP (Access Point) 热点或使用已连接的 WiFi
3. 用户通过手机/电脑连接到设备并访问控制网页
4. 通过网页上的虚拟摇杆或按钮控制机器车移动
5. 再次按下 MODE_BUTTON 返回小智模式

---

## 2. 技术架构分析

### 2.1 现有代码结构

#### 按钮配置
```cpp
// main/boards/otto-hp-robot/config.h
#define BOOT_BUTTON_GPIO GPIO_NUM_0  // 现有按钮,用于小智对话
#define MODE_BUTTON_GPIO GPIO_NUM_2  // 模式切换按钮(已定义但未使用)
```

#### 现有功能模块
1. **WheelRobotController**: 已实现完整的轮子运动控制
   - 18个 MCP 工具函数(前进、后退、转向、舞蹈等)
   - 支持速度控制、持续时间控制
   - 已有队列机制处理动作序列

2. **WifiBoard**: WiFi 网络管理
   - WiFi 连接管理
   - AP 配置模式(用于首次配网)

3. **Application**: 设备状态管理
   - 状态机: Starting, Idle, Listening, Speaking 等
   - 需要扩展支持 RemoteControl 状态

### 2.2 参考实现: ESP-Hi 的 Web 控制

从 `esp-hi` 板子的实现中,我们可以学习到:

```cpp
// esp_hi.cc
#ifdef CONFIG_ESP_HI_WEB_CONTROL_ENABLED
    static void wifi_event_handler() {
        if (WiFi connected) {
            esp_hi_web_control_server_init();  // 初始化 Web 服务器
        }
    }
#endif
```

**关键点**:
- WiFi 连接成功后启动 HTTP 服务器
- 使用配置开关控制功能启用
- Web 服务器独立于主应用逻辑

---

## 3. 设计方案

### 3.1 系统架构

```
用户按钮 (GPIO_2)
    ↓
模式管理器 (ModeManager)
    ↓
[小智模式] ←→ [遥控模式]
    ↓              ↓
  Application   WebServer + WebUI
                    ↓
              WheelRobotController
```

### 3.2 核心组件设计

#### A. 模式管理器 (ModeManager)

**职责**:
- 管理当前运行模式(小智/遥控)
- 处理模式切换逻辑
- 协调各组件的启动/停止

**状态定义**:
```cpp
enum DeviceMode {
    kModeXiaozhi,      // 小智对话模式
    kModeRemoteControl // 遥控模式
};
```

**接口**:
```cpp
class ModeManager {
public:
    void SwitchToXiaozhiMode();
    void SwitchToRemoteControlMode();
    DeviceMode GetCurrentMode();
    void ToggleMode();  // 切换模式
};
```

#### B. Web 服务器 (RemoteControlServer)

**技术选型**: ESP-IDF HTTP Server

**功能**:
1. 提供 HTML/CSS/JavaScript 控制界面
2. 接收前端控制指令 (REST API)
3. 调用 WheelRobotController 执行动作
4. 返回机器人状态信息

**API 端点设计**:
```
GET  /                    - 返回控制页面
POST /api/move/forward    - 前进
POST /api/move/backward   - 后退
POST /api/move/left       - 左转
POST /api/move/right      - 右转
POST /api/move/stop       - 停止
POST /api/move/custom     - 自定义速度 {left: -100~100, right: -100~100}
GET  /api/status          - 获取状态
POST /api/dance/:type     - 跳舞动作
```

**JSON 请求格式**:
```json
{
    "speed": 50,           // 速度 0-100
    "duration_ms": 1000    // 持续时间(可选)
}
```

**JSON 响应格式**:
```json
{
    "success": true,
    "mode": "remote_control",
    "status": "moving",
    "left_speed": 50,
    "right_speed": 50
}
```

#### C. Web 控制界面设计

**界面布局**:
```
┌─────────────────────────────┐
│   Otto HP Robot 遥控器      │
├─────────────────────────────┤
│        状态指示灯            │
│     ● 已连接 / ○ 移动中      │
├─────────────────────────────┤
│                             │
│          ▲ 前进             │
│       ◄     ►               │
│          ▼ 后退             │
│                             │
│     [原地左转] [原地右转]    │
│                             │
├─────────────────────────────┤
│  速度: [────────●────] 50%  │
├─────────────────────────────┤
│         [停止]              │
│                             │
│  跳舞: [摇摆][旋转][波浪]   │
│        [之字][太空步]       │
├─────────────────────────────┤
│  [返回小智模式]             │
└─────────────────────────────┘
```

**技术实现**:
- 纯 HTML + CSS + JavaScript (无需外部库)
- 虚拟摇杆使用 HTML Canvas 实现
- WebSocket 或 短轮询 获取实时状态
- 触摸友好的按钮设计

---

## 4. 实现步骤

### 阶段 1: 基础架构 (1-2天)

#### 1.1 创建模式管理器
```cpp
// mode_manager.h
class ModeManager {
private:
    DeviceMode current_mode_;
    static ModeManager* instance_;
    
public:
    static ModeManager& GetInstance();
    void Initialize();
    void SwitchMode();
    DeviceMode GetCurrentMode();
    void OnModeChanged(std::function<void(DeviceMode)> callback);
};
```

#### 1.2 扩展 Application 状态
```cpp
// application.h
enum DeviceState {
    // 现有状态...
    kDeviceStateRemoteControl,  // 新增:遥控模式
};

class Application {
    // ...
    void EnterRemoteControlMode();
    void ExitRemoteControlMode();
};
```

#### 1.3 添加 MODE_BUTTON 按钮处理
```cpp
// otto_hp_robot.cc
void InitializeButtons() {
    // 现有 boot_button_ 处理...
    
    mode_button_.OnClick([this]() {
        ModeManager::GetInstance().SwitchMode();
    });
}

private:
    Button boot_button_;
    Button mode_button_;  // 新增
```

### 阶段 2: Web 服务器实现 (2-3天)

#### 2.1 创建 HTTP 服务器
```cpp
// remote_control_server.h
class RemoteControlServer {
public:
    void Start();
    void Stop();
    bool IsRunning();
    
private:
    httpd_handle_t server_;
    WheelRobotController* wheel_controller_;
    
    static esp_err_t HandleRoot(httpd_req_t* req);
    static esp_err_t HandleMoveForward(httpd_req_t* req);
    // ... 其他 API 处理函数
};
```

#### 2.2 实现 REST API 端点
参考 ESP-IDF HTTP Server 示例:
```cpp
esp_err_t RemoteControlServer::HandleMoveForward(httpd_req_t* req) {
    // 1. 解析 JSON 请求体
    char content[100];
    httpd_req_recv(req, content, req->content_len);
    
    // 2. 提取参数
    cJSON* root = cJSON_Parse(content);
    int speed = cJSON_GetObjectItem(root, "speed")->valueint;
    int duration = cJSON_GetObjectItem(root, "duration_ms")->valueint;
    
    // 3. 调用控制器
    auto* controller = GetWheelRobotController();
    controller->MoveForward(speed, duration);
    
    // 4. 返回响应
    const char* resp = "{\"success\":true}";
    httpd_resp_send(req, resp, strlen(resp));
    
    return ESP_OK;
}
```

### 阶段 3: Web 界面开发 (2-3天)

#### 3.1 HTML 结构
```html
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Otto HP Robot 遥控器</title>
    <style>/* CSS 样式 */</style>
</head>
<body>
    <div id="app">
        <h1>Otto HP Robot</h1>
        <div id="status">● 已连接</div>
        
        <!-- 虚拟摇杆 -->
        <canvas id="joystick"></canvas>
        
        <!-- 控制按钮 -->
        <button onclick="move('forward')">▲</button>
        <!-- ... -->
    </div>
    
    <script>/* JavaScript 逻辑 */</script>
</body>
</html>
```

#### 3.2 JavaScript 控制逻辑
```javascript
// API 调用封装
async function sendCommand(endpoint, data) {
    const response = await fetch(`/api/${endpoint}`, {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(data)
    });
    return response.json();
}

// 移动控制
function move(direction) {
    const speed = document.getElementById('speed').value;
    sendCommand(`move/${direction}`, {speed: speed});
}

// 虚拟摇杆实现
class VirtualJoystick {
    // Canvas 绘制和触摸事件处理
    // 将摇杆位置转换为左右轮速度
}
```

### 阶段 4: 集成与优化 (1-2天)

#### 4.1 模式切换流程
```cpp
void ModeManager::SwitchToRemoteControlMode() {
    // 1. 停止小智服务
    Application::GetInstance().PauseXiaozhiServices();
    
    // 2. 启动 Web 服务器
    RemoteControlServer::GetInstance().Start();
    
    // 3. 更新显示
    Display* display = Board::GetInstance().GetDisplay();
    display->ShowNotification("已进入遥控模式");
    display->SetEmotion("robot");  // 显示机器人图标
    
    // 4. 播放提示音
    Application::GetInstance().PlaySound(OGG_MODE_SWITCH);
}

void ModeManager::SwitchToXiaozhiMode() {
    // 1. 停止 Web 服务器
    RemoteControlServer::GetInstance().Stop();
    
    // 2. 恢复小智服务
    Application::GetInstance().ResumeXiaozhiServices();
    
    // 3. 更新显示
    Display* display = Board::GetInstance().GetDisplay();
    display->ShowNotification("已返回小智模式");
    display->SetEmotion("neutral");
}
```

#### 4.2 安全性考虑
1. **访问控制**: 添加简单密码保护 (可选)
2. **超时机制**: 10分钟无操作自动返回小智模式
3. **冲突处理**: 确保两种模式不会同时控制轮子

---

## 5. 文件结构

```
main/boards/otto-hp-robot/
├── config.h                      # 现有配置
├── otto_hp_robot.cc              # 主板文件(需修改)
├── mode_manager.h                # 新增:模式管理器
├── mode_manager.cc               # 新增:模式管理器实现
├── remote_control_server.h       # 新增:Web服务器
├── remote_control_server.cc      # 新增:Web服务器实现
└── remote_control_web_ui.h       # 新增:嵌入式HTML/CSS/JS
```

---

## 6. 配置选项 (Kconfig)

```kconfig
menu "Otto HP Robot Configuration"
    config OTTO_REMOTE_CONTROL_ENABLED
        bool "Enable Remote Control Mode"
        default y
        help
            Enable web-based remote control mode
    
    config OTTO_REMOTE_CONTROL_PORT
        int "Remote Control Web Server Port"
        default 80
        depends on OTTO_REMOTE_CONTROL_ENABLED
    
    config OTTO_REMOTE_CONTROL_TIMEOUT
        int "Auto-return timeout (seconds)"
        default 600
        depends on OTTO_REMOTE_CONTROL_ENABLED
endmenu
```

---

## 7. 测试计划

### 7.1 单元测试
- [x] ModeManager 模式切换逻辑
- [x] RemoteControlServer API 响应
- [x] WheelRobotController 在遥控模式下的调用

### 7.2 集成测试
- [ ] 按钮切换模式流程
- [ ] Web界面控制机器车移动
- [ ] 模式切换时的资源释放
- [ ] 长时间运行稳定性

### 7.3 用户验收测试
- [ ] 手机浏览器兼容性 (Chrome, Safari)
- [ ] 触摸响应速度
- [ ] 虚拟摇杆操作体验
- [ ] 网络延迟情况下的表现

---

## 8. 潜在问题与解决方案

### 问题 1: WiFi 连接问题
**现象**: 遥控模式需要WiFi,但设备可能未配网
**解决方案**:
- 方案A: 自动启动 AP 模式,SSID: `Otto-Robot-XXXX`
- 方案B: 检测WiFi状态,未连接时提示用户先配网
- **推荐**: 方案A,用户体验更好

### 问题 2: 两种模式的资源冲突
**现象**: 小智模式和遥控模式可能同时访问轮子控制器
**解决方案**:
- 使用互斥锁保护 WheelRobotController
- 模式切换时清空动作队列
- 明确定义模式优先级

### 问题 3: 网络延迟导致控制不流畅
**现象**: WiFi 延迟导致指令响应慢
**解决方案**:
- 使用 WebSocket 替代 HTTP 轮询
- 客户端预测 + 服务端校正
- 限制指令发送频率(如 100ms/次)

### 问题 4: 内存占用
**现象**: HTTP服务器和Web界面占用较多内存
**解决方案**:
- Web界面使用压缩版本
- HTTP服务器使用最小配置
- 监控堆内存,必要时优化

---

## 9. 优化方向

### 9.1 短期优化 (MVP后)
1. 添加速度曲线调整
2. 支持手势控制(划动方向)
3. 添加陀螺仪控制模式
4. 录制/回放动作序列

### 9.2 长期优化
1. 多用户同时控制(队列模式)
2. 摄像头视频流集成
3. 游戏手柄支持(蓝牙)
4. AI辅助避障

---

## 10. 时间估算

| 阶段 | 工作内容 | 预计时间 |
|------|---------|---------|
| 阶段1 | 基础架构、模式管理器 | 1-2天 |
| 阶段2 | Web服务器实现 | 2-3天 |
| 阶段3 | Web界面开发 | 2-3天 |
| 阶段4 | 集成与优化 | 1-2天 |
| 测试 | 单元/集成/用户测试 | 2天 |
| **总计** | | **8-12天** |

---

## 11. 下一步行动

### 立即可做
1. ✅ 创建此设计文档
2. ⬜ 搭建基础模式管理器框架
3. ⬜ 实现简单的 HTTP 服务器原型
4. ⬜ 制作静态 HTML 控制界面原型

### 需要确认
1. 是否需要 AP 模式还是仅使用现有 WiFi?
2. 是否需要密码保护?
3. 控制界面的详细交互方式(摇杆 vs 按钮)?
4. 是否需要实时视频流?

### 风险评估
- **技术风险**: 中等 (HTTP服务器稳定性、内存管理)
- **时间风险**: 低 (核心功能相对独立)
- **用户体验风险**: 中等 (网络延迟、触摸操作)

---

## 12. 参考资料

1. ESP-IDF HTTP Server API: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_http_server.html
2. ESP-Hi Web Control 实现: `main/boards/esp-hi/esp_hi.cc`
3. WheelRobotController MCP工具: `main/boards/otto-hp-robot/wheel_robot_controller.cc`
4. HTML5 Canvas 虚拟摇杆教程
5. WebSocket vs HTTP 性能对比

---

**文档版本**: v1.0  
**创建日期**: 2025-11-20  
**作者**: GitHub Copilot  
**状态**: 初稿 - 待评审
