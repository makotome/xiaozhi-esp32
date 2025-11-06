# Otto HP Robot 板子配置说明

## 硬件配置

### 显示屏
- 类型: ST7789 1.54寸 TFT LCD
- 分辨率: 240x240
- 接口: SPI

### 音频
- 输入采样率: 16000 Hz
- 输出采样率: 24000 Hz
- 接口: I2S (单工模式)

### 电源管理
- 充电检测: GPIO 21
- 电池电压检测: ADC2_CH3

### 舵机控制
- 右腿: GPIO 39
- 右脚: GPIO 38
- 左腿: GPIO 17
- 左脚: GPIO 18
- 左手: GPIO 8
- 右手: GPIO 12

## 添加板子到项目编译系统

### 1. 修改 `main/Kconfig.projbuild`

在 `BOARD_TYPE_OTTO_ROBOT` 配置项附近添加新的板子配置选项：

```kconfig
config BOARD_TYPE_OTTO_HP_ROBOT
    bool "ottoHpRobot"
    depends on IDF_TARGET_ESP32S3
```

建议添加位置：在第 402-408 行之间，`OTTO_ROBOT` 和 `ELECTRON_BOT` 之间。

### 2. 修改 `main/CMakeLists.txt`

在 `BOARD_TYPE_OTTO_ROBOT` 的 `elseif` 块附近添加新的条件分支：

```cmake
elseif(CONFIG_BOARD_TYPE_OTTO_HP_ROBOT)
    set(BOARD_TYPE "otto-hp-robot")
    set(BUILTIN_TEXT_FONT font_puhui_16_4)
    set(BUILTIN_ICON_FONT font_awesome_16_4)
```

建议添加位置：在 `CONFIG_BOARD_TYPE_OTTO_ROBOT` 配置块之后。

### 3. 配置文件说明

`config.json` 文件配置：

```json
{
    "target": "esp32s3",
    "builds": [
        {
            "name": "otto-hp-robot",
            "sdkconfig_append": [
                "CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"partitions/v1/16m.csv\"",
                "CONFIG_LV_USE_GIF=y",
                "CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y",
                "CONFIG_ESP_CONSOLE_NONE=y"
            ]
        }
    ]
}
```

## 编译步骤

### 1. 清理之前的配置
```bash
idf.py fullclean
```

### 2. 设置目标芯片
```bash
idf.py set-target esp32s3
```

### 3. 配置项目
```bash
idf.py menuconfig
```
在菜单中选择:
- `Xiaozhi Assistant` -> `Board Type` -> `ottoHpRobot`

### 4. 编译
```bash
idf.py build
```

### 5. 烧录
```bash
idf.py flash
```

### 6. 查看日志（可选）
```bash
idf.py monitor
```

## 特性说明

### MCP 控制器集成
- 通过 MCP (Model Context Protocol) 协议控制 Otto 机器人动作
- 支持自定义动作序列和表情显示

### 表情显示系统
- 使用 `OttoEmojiDisplay` 类实现表情显示
- 支持在 1.54 寸屏幕上显示各种表情动画

### 电源管理
- 实时监测电池电量
- 充电状态检测

## 故障排除

### 显示问题
- 如果屏幕显示异常，检查 SPI 时钟频率是否过高
- 当前配置使用 10MHz，可根据实际情况调整

### 音频问题
- 确认 I2S 引脚配置正确
- 检查采样率设置是否匹配硬件

### 舵机问题
- 确认舵机电源供应充足
- 检查 GPIO 引脚配置是否正确

## 版本历史

- v1.4.4: 初始版本
