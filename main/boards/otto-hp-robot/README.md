# Otto HP Robot 完整开发文档# Otto HP Robot 板子配置说明



> 基于 ESP32-S3 的两轮机器人，支持表情显示、语音交互、轮子控制和彩色灯光效果## 硬件配置



---### 显示屏

- 类型: ST7789 1.54寸 TFT LCD

## 📋 目录- 分辨率: 240x240

- 接口: SPI

- [硬件配置](#硬件配置)

- [编译部署](#编译部署)### 音频

- [轮子控制](#轮子控制)- 输入采样率: 16000 Hz

- [跳舞动作](#跳舞动作)- 输出采样率: 24000 Hz

- [彩色灯光](#彩色灯光)- 接口: I2S (单工模式)

- [MCP工具列表](#mcp工具列表)

- [故障排除](#故障排除)### 电源管理

- 充电检测: GPIO 21

---- 电池电压检测: ADC2_CH3



## 硬件配置### 舵机控制

- 右腿: GPIO 39

### 显示屏- 右脚: GPIO 38

- 类型: ST7789 1.54寸 TFT LCD- 左腿: GPIO 17

- 分辨率: 240x240- 左脚: GPIO 18

- 接口: SPI- 左手: GPIO 8

- 右手: GPIO 12

### 音频

- 输入采样率: 16000 Hz## 添加板子到项目编译系统

- 输出采样率: 24000 Hz

- 接口: I2S (单工模式)### 1. 修改 `main/Kconfig.projbuild`



### 电源管理在 `BOARD_TYPE_OTTO_ROBOT` 配置项附近添加新的板子配置选项：

- 充电检测: GPIO 21

- 电池电压检测: ADC2_CH3```kconfig

config BOARD_TYPE_OTTO_HP_ROBOT

### 轮子舵机（两轮驱动）    bool "ottoHpRobot"

- 左轮: GPIO 17 (使用LEFT_LEG_PIN)    depends on IDF_TARGET_ESP32S3

- 右轮: GPIO 18 (使用LEFT_FOOT_PIN)```



### 其他舵机（Otto动作）建议添加位置：在第 402-408 行之间，`OTTO_ROBOT` 和 `ELECTRON_BOT` 之间。

- 右腿: GPIO 39

- 右脚: GPIO 38### 2. 修改 `main/CMakeLists.txt`

- 左手: GPIO 8

- 右手: GPIO 12在 `BOARD_TYPE_OTTO_ROBOT` 的 `elseif` 块附近添加新的条件分支：



---```cmake

elseif(CONFIG_BOARD_TYPE_OTTO_HP_ROBOT)

## 编译部署    set(BOARD_TYPE "otto-hp-robot")

    set(BUILTIN_TEXT_FONT font_puhui_16_4)

### 1. 添加板子到项目    set(BUILTIN_ICON_FONT font_awesome_16_4)

```

#### 修改 `main/Kconfig.projbuild`

```kconfig建议添加位置：在 `CONFIG_BOARD_TYPE_OTTO_ROBOT` 配置块之后。

config BOARD_TYPE_OTTO_HP_ROBOT

    bool "ottoHpRobot"### 3. 配置文件说明

    depends on IDF_TARGET_ESP32S3

````config.json` 文件配置：



#### 修改 `main/CMakeLists.txt````json

```cmake{

elseif(CONFIG_BOARD_TYPE_OTTO_HP_ROBOT)    "target": "esp32s3",

    set(BOARD_TYPE "otto-hp-robot")    "builds": [

    set(BUILTIN_TEXT_FONT font_puhui_16_4)        {

    set(BUILTIN_ICON_FONT font_awesome_16_4)            "name": "otto-hp-robot",

```            "sdkconfig_append": [

                "CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"partitions/v1/16m.csv\"",

### 2. 配置文件 `config.json`                "CONFIG_LV_USE_GIF=y",

```json                "CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y",

{                "CONFIG_ESP_CONSOLE_NONE=y"

    "target": "esp32s3",            ]

    "builds": [        }

        {    ]

            "name": "otto-hp-robot",}

            "sdkconfig_append": [```

                "CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"partitions/v1/16m.csv\"",

                "CONFIG_LV_USE_GIF=y",## 编译步骤

                "CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y",

                "CONFIG_ESP_CONSOLE_NONE=y"### 1. 清理之前的配置

            ]```bash

        }idf.py fullclean

    ]```

}

```### 2. 设置目标芯片

```bash

### 3. 编译步骤idf.py set-target esp32s3

```bash```

# 清理配置

idf.py fullclean### 3. 配置项目

```bash

# 设置目标芯片idf.py menuconfig

idf.py set-target esp32s3```

在菜单中选择:

# 配置项目- `Xiaozhi Assistant` -> `Board Type` -> `ottoHpRobot`

idf.py menuconfig

# 选择: Xiaozhi Assistant -> Board Type -> ottoHpRobot### 4. 编译

```bash

# 编译idf.py build

idf.py build```



# 烧录### 5. 烧录

idf.py flash```bash

idf.py flash

# 查看日志```

idf.py monitor

```### 6. 查看日志（可选）

```bash

---idf.py monitor

```

## 轮子控制

## 特性说明

### 基础运动控制

### MCP 控制器集成

#### 文件结构- 通过 MCP (Model Context Protocol) 协议控制 Otto 机器人动作

- `wheel_movements.h/cc` - 底层轮子控制- 支持自定义动作序列和表情显示

- `wheel_servo.h/cc` - 轮子舵机驱动

- `wheel_robot_controller.h/cc` - MCP控制器封装### 表情显示系统

- 使用 `OttoEmojiDisplay` 类实现表情显示

#### 可用MCP工具- 支持在 1.54 寸屏幕上显示各种表情动画



| 工具名称 | 功能 | 参数 |### 电源管理

|---------|------|------|- 实时监测电池电量

| `self.wheel.move_forward` | 前进 | speed: 0-100, duration_ms: 0-60000 |- 充电状态检测

| `self.wheel.move_backward` | 后退 | speed: 0-100, duration_ms: 0-60000 |

| `self.wheel.turn_left` | 左转(差速) | speed: 0-100, duration_ms: 0-60000 |## 故障排除

| `self.wheel.turn_right` | 右转(差速) | speed: 0-100, duration_ms: 0-60000 |

| `self.wheel.spin_left` | 原地左转 | speed: 0-100, duration_ms: 0-60000 |### 显示问题

| `self.wheel.spin_right` | 原地右转 | speed: 0-100, duration_ms: 0-60000 |- 如果屏幕显示异常，检查 SPI 时钟频率是否过高

| `self.wheel.stop` | 停止 | 无 |- 当前配置使用 10MHz，可根据实际情况调整

| `self.wheel.accelerate` | 加速 | target_speed: 0-100, duration_ms: 100-10000 |

| `self.wheel.decelerate` | 减速 | duration_ms: 100-10000 |### 音频问题

| `self.wheel.set_wheel_speeds` | 独立控制 | left_speed: -100~100, right_speed: -100~100 |- 确认 I2S 引脚配置正确

| `self.wheel.get_status` | 获取状态 | 无 |- 检查采样率设置是否匹配硬件

| `self.wheel.get_speeds` | 获取速度 | 无 |

### 舵机问题

#### 语音指令示例- 确认舵机电源供应充足

```- 检查 GPIO 引脚配置是否正确

"往前走"        → self.wheel.move_forward

"后退"          → self.wheel.move_backward## 版本历史

"向左转"        → self.wheel.turn_left

"向右转"        → self.wheel.turn_right- v1.4.4: 初始版本

"停下"          → self.wheel.stop
"加速前进"      → self.wheel.accelerate
```

---

## 跳舞动作

### 5种舞蹈模式

#### 1. 摇摆舞 (Shake) 🎵
- **工具**: `self.wheel.dance_shake`
- **特点**: 三段式节奏，速度渐进 (50→75→85→90)
- **时长**: 约4.8秒
- **效果**: 快速左右摇摆，充满活力

#### 2. 旋转舞 (Spin) 🌀
- **工具**: `self.wheel.dance_spin`
- **特点**: 左右旋转结合，急停效果
- **时长**: 约6.5秒
- **效果**: 360度旋转，速度多变

#### 3. 波浪舞 (Wave) 🌊
- **工具**: `self.wheel.dance_wave`
- **特点**: 5次波浪循环，加入轻微摆动
- **时长**: 约9秒
- **效果**: 前后移动，速度呈波浪变化

#### 4. 之字舞 (Zigzag) ⚡
- **工具**: `self.wheel.dance_zigzag`
- **特点**: Z字路线，包含急转和旋转
- **时长**: 约8秒
- **效果**: 走Z字形，充满动感

#### 5. 太空步 (Moonwalk) 🌙
- **工具**: `self.wheel.dance_moonwalk`
- **特点**: 三段式，包含后退+旋转组合
- **时长**: 约12秒
- **效果**: 模拟MJ经典动作

### 随机跳舞
- **工具**: `self.wheel.dance_random`
- **参数**: `dance_type` (可选, 0-5)
  - 0: 随机选择
  - 1-5: 指定舞蹈类型

### 语音指令示例
```
"跳个舞"        → self.wheel.dance_random (随机)
"跳摇摆舞"      → self.wheel.dance_shake
"跳旋转舞"      → self.wheel.dance_spin
"跳波浪舞"      → self.wheel.dance_wave
"跳之字舞"      → self.wheel.dance_zigzag
"跳太空步"      → self.wheel.dance_moonwalk
```

### 技术实现
```cpp
// 动作枚举定义
ACTION_DANCE_SHAKE = 11,      // 摇摆舞
ACTION_DANCE_SPIN = 12,       // 旋转舞
ACTION_DANCE_WAVE = 13,       // 波浪舞
ACTION_DANCE_ZIGZAG = 14,     // 之字舞
ACTION_DANCE_MOONWALK = 15    // 太空步
```

---

## 彩色灯光

### 灯光系统特性

#### 8种灯光模式

| 模式 | 编号 | 效果 | Emoji |
|------|------|------|-------|
| 关闭 | 0 | 恢复正常显示 | - |
| 夜灯 | 1 | 纯白色柔和灯光 | 💡 |
| 跳舞派对 | 2 | 五彩缤纷快速变换 | 🎉 |
| 呼吸灯 | 3 | 蓝色柔和呼吸 | 🌙 |
| 彩虹 | 4 | 慢速彩虹色渐变 | 🌈 |
| 闪烁 | 5 | 白色快速闪烁 | ⚡ |
| 暖光 | 6 | 淡黄色暖光 | ☀️ |
| 冷光 | 7 | 淡蓝色冷光 | ❄️ |

#### 灯光MCP工具

| 工具名称 | 功能 | 参数 |
|---------|------|------|
| `self.light.set_mode` | 设置模式 | mode: 0-7 |
| `self.light.set_brightness` | 设置亮度 | brightness: 0-100 |
| `self.light.off` | 关闭灯光 | 无 |
| `self.light.get_status` | 获取状态 | 无 |
| `self.light.night_light` | 夜灯 | 无 |
| `self.light.dance_party` | 跳舞派对 | 无 |
| `self.light.breathing` | 呼吸灯 | 无 |
| `self.light.rainbow` | 彩虹灯 | 无 |
| `self.light.flash` | 闪烁灯 | 无 |
| `self.light.warm` | 暖光 | 无 |
| `self.light.cool` | 冷光 | 无 |

### 灯光集成代码

#### 在 `otto_hp_robot.cc` 中

```cpp
// 1. 添加头文件
#include "light_mcp_controller.h"

// 2. 添加初始化函数（private部分）
void InitializeLightController()
{
    ESP_LOGI(TAG, "初始化彩色灯光控制器");
    auto* otto_display = dynamic_cast<OttoEmojiDisplay*>(display_);
    if (otto_display) {
        InitializeLightMcpController(otto_display);
    }
}

// 3. 在构造函数中调用（InitializeLcdDisplay()之后）
InitializeLightController();
```

### 使用场景

#### 场景1: 跳舞表演
```
用户: "跳个舞"
机器人:
  1. self.light.dance_party (开启五彩灯光)
  2. self.wheel.dance_random (开始跳舞)
```

#### 场景2: 夜间模式
```
用户: "打开夜灯"
机器人:
  1. self.light.night_light
  2. self.light.set_brightness (brightness=40)
```

#### 场景3: 氛围营造
```
用户: "制造浪漫氛围"
机器人:
  1. self.light.warm
  2. self.light.set_brightness (brightness=30)
```

### 技术架构

#### 文件结构
- `colorful_light_controller.h/cc` - 核心灯光控制器
- `light_mcp_controller.h/cc` - MCP接口封装

#### 关键技术
- **LVGL覆盖层**: 不影响底层UI
- **FreeRTOS任务**: 独立任务处理动画
- **HSV色彩空间**: 便于实现渐变效果
- **线程安全**: 使用LVGL锁机制

#### 性能指标
- 内存占用: ~4KB
- CPU占用: 静态<1%, 动画1-3%
- 刷新频率: 10-50Hz

---

## MCP工具列表

### 完整工具清单 (共29个)

#### 轮子控制 (12个)
1. `self.wheel.move_forward` - 前进
2. `self.wheel.move_backward` - 后退
3. `self.wheel.turn_left` - 左转
4. `self.wheel.turn_right` - 右转
5. `self.wheel.spin_left` - 原地左转
6. `self.wheel.spin_right` - 原地右转
7. `self.wheel.stop` - 停止
8. `self.wheel.accelerate` - 加速
9. `self.wheel.decelerate` - 减速
10. `self.wheel.set_wheel_speeds` - 独立控制
11. `self.wheel.get_status` - 获取状态
12. `self.wheel.get_speeds` - 获取速度

#### 跳舞动作 (6个)
13. `self.wheel.dance_shake` - 摇摆舞
14. `self.wheel.dance_spin` - 旋转舞
15. `self.wheel.dance_wave` - 波浪舞
16. `self.wheel.dance_zigzag` - 之字舞
17. `self.wheel.dance_moonwalk` - 太空步
18. `self.wheel.dance_random` - 随机跳舞

#### 灯光控制 (11个)
19. `self.light.set_mode` - 设置模式
20. `self.light.set_brightness` - 设置亮度
21. `self.light.off` - 关闭灯光
22. `self.light.get_status` - 获取状态
23. `self.light.night_light` - 夜灯
24. `self.light.dance_party` - 跳舞派对
25. `self.light.breathing` - 呼吸灯
26. `self.light.rainbow` - 彩虹灯
27. `self.light.flash` - 闪烁灯
28. `self.light.warm` - 暖光
29. `self.light.cool` - 冷光

---

## 故障排除

### 显示问题
- **症状**: 屏幕显示异常
- **检查**: SPI 时钟频率是否过高
- **解决**: 当前使用10MHz，可根据实际调整

### 音频问题
- **症状**: 无声音或杂音
- **检查**: I2S 引脚配置和采样率
- **解决**: 确认引脚正确，采样率匹配硬件

### 轮子问题
- **症状**: 轮子不转或异常
- **检查**: 舵机电源、GPIO配置、PWM信号
- **解决**: 
  - 确认电源供应充足
  - 检查GPIO引脚配置
  - 运行硬件诊断: `runHardwareDiagnostics()`

### 灯光问题
- **症状**: 灯光不显示
- **检查**: Display类型、初始化顺序
- **解决**:
  - 确认display_是OttoEmojiDisplay类型
  - InitializeLightController在InitializeLcdDisplay之后
  - 查看日志中的错误信息

### MCP工具问题
- **症状**: 工具调用无响应
- **检查**: RegisterAllMcpTools()是否被调用
- **解决**:
  - 确认在构造函数中调用了RegisterAllMcpTools()
  - 检查控制器是否正确初始化
  - 查看日志中的注册信息

---

## 特性说明

### MCP控制器集成
- 通过 MCP (Model Context Protocol) 协议控制机器人
- 支持轮子运动、跳舞动作、灯光效果
- 统一的工具注册机制

### 表情显示系统
- 使用 `OttoEmojiDisplay` 类实现表情显示
- 支持在 1.54 寸屏幕上显示各种表情动画
- 与灯光效果共存互不干扰

### 电源管理
- 实时监测电池电量
- 充电状态检测
- 低电量保护

### 模块化设计
- 轮子控制独立模块
- 灯光效果独立模块
- 易于扩展和维护

---

## 开发建议

### 添加自定义舞蹈
1. 在 `wheel_movements.cc` 中添加新的舞蹈函数
2. 在 `wheel_movements.h` 中声明函数
3. 在 `wheel_robot_controller.h` 中添加ACTION枚举
4. 在 `wheel_robot_controller.cc` 的switch中添加case
5. 在 `RegisterMcpTools()` 中注册新工具

### 添加自定义灯光效果
1. 在 `colorful_light_controller.h` 中添加模式枚举
2. 在 `colorful_light_controller.cc` 中实现效果函数
3. 在任务的switch中添加case
4. 在 `light_mcp_controller.cc` 中注册新工具

### 性能优化
- 根据需求调整动画刷新频率
- 合理分配FreeRTOS任务优先级
- 注意内存使用，避免内存泄漏

---

## 版本历史

- **v1.4.4**: 初始版本
- **v1.5.0**: 添加两轮驱动支持
- **v1.6.0**: 添加5种跳舞动作
- **v1.7.0**: 添加彩色灯光控制系统
- **v1.8.0**: 优化跳舞动作，增强表现力

---

## 致谢

感谢所有为 Otto HP Robot 项目做出贡献的开发者！

如有问题或建议，请提交 Issue 或 Pull Request。

---

**享受你的炫彩机器人吧！** 🎉✨🤖
