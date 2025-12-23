#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

/* ========================================================================
 * 电源管理模块 (Power Management)
 * ======================================================================== */

/**
 * 充电检测引脚 - GPIO21 (数字输入)
 * 功能: 检测充电器是否插入，通常连接充电IC的状态输出引脚
 *       - 高电平: 正在充电
 *       - 低电平: 未充电/充电完成
 * 特性: RTC_GPIO21, 普通GPIO
 * 位置: Pin23 (芯片右侧中部)
 */
#define POWER_CHARGE_DETECT_PIN GPIO_NUM_21

/**
 * 电池电压检测引脚 - GPIO1 (模拟输入/ADC)
 * 功能: 通过ADC采集电池电压，用于电量计算和低电量报警
 * 特性: RTC_GPIO1, ADC1_CH0, TOUCH1
 * 位置: Pin39 (芯片右下角)
 * 硬件要求: 需要外部分压电路将电池电压(例如4.2V)降到0-2.45V范围
 *          推荐分压比: R1=100kΩ, R2=100kΩ (分压系数0.5)
 *          对于3.7V锂电池: 测量范围约1.5V-2.1V
 * ADC配置:
 *   - ADC单元: ADC_UNIT_1 (ADC1不受Wi-Fi影响)
 *   - ADC通道: ADC_CHANNEL_0
 *   - 衰减: ADC_ATTEN_DB_11 (测量范围0-2.45V)
 *   - 位宽: ADC_WIDTH_BIT_12 (分辨率12位)
 */
#define POWER_VOLTAGE_SENSE_PIN GPIO_NUM_1
#define POWER_ADC_UNIT ADC_UNIT_1
#define POWER_ADC_CHANNEL ADC_CHANNEL_0  // GPIO1 = ADC1_CH0
/**
 * 左侧舵机组 (物理聚类: Pin8-12, 芯片左侧中部)
 * 使用连续的GPIO以便于布线
 */
#define LEFT_LEG_PIN GPIO_NUM_17   // Pin10: 左腿舵机
#define LEFT_FOOT_PIN GPIO_NUM_18  // Pin11: 左脚舵机
#define LEFT_HAND_PIN GPIO_NUM_8   // Pin12: 左手舵机

/**
 * 右侧舵机组 (物理聚类: Pin31-33, 芯片右侧中部)
 * 与左侧对称布局，便于机械结构设计
 */
#define RIGHT_LEG_PIN GPIO_NUM_40   // Pin31: 右腿舵机
#define RIGHT_FOOT_PIN GPIO_NUM_39  // Pin32: 右脚舵机
#define RIGHT_HAND_PIN GPIO_NUM_38  // Pin33: 右手舵机

#define AUDIO_INPUT_SAMPLE_RATE 16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000
#define AUDIO_I2S_METHOD_SIMPLEX

/**
 * 麦克风I2S接口 (物理聚类: Pin4-6, 芯片左上角)
 * I2S0 用于麦克风输入
 * 注意: 这些引脚具有ADC功能，但I2S优先级更高
 */
#define AUDIO_I2S_MIC_GPIO_WS GPIO_NUM_4   // Pin4:  Word Select (LRCK)
#define AUDIO_I2S_MIC_GPIO_SCK GPIO_NUM_5  // Pin5:  Serial Clock (BCLK)
#define AUDIO_I2S_MIC_GPIO_DIN GPIO_NUM_6  // Pin6:  Data In

/**
 * 扬声器I2S接口 (物理聚类: Pin7-9, 芯片左侧上部)
 * I2S1 用于扬声器输出
 * 注意: GPIO15/16为XTAL_32K_P/N，但已实测可用作I2S，不影响功能
 */
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_7   // Pin7:  Data Out
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_15  // Pin8:  Bit Clock
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_16  // Pin9:  Left/Right Clock

/* ========================================================================
 * 显示屏模块 (LCD Display) - SPI接口
 * ======================================================================== */

/**
 * ST7789 240x240 TFT LCD (物理聚类: Pin18-23, 芯片左侧下部)
 * 使用FSPI (GPIO9-14) 用于高速SPI通信
 * 注意: 这些引脚与内部Flash的FSPI复用，但通过片选可以区分
 */
#define DISPLAY_CS_PIN GPIO_NUM_10        // Pin18: Chip Select (FSPICS0)
#define DISPLAY_DC_PIN GPIO_NUM_11        // Pin19: Data/Command (FSPID)
#define DISPLAY_RST_PIN GPIO_NUM_12       // Pin20: Reset (FSPICLK复用)
#define DISPLAY_MOSI_PIN GPIO_NUM_13      // Pin21: MOSI/SDA (FSPIQ)
#define DISPLAY_CLK_PIN GPIO_NUM_14       // Pin22: Clock (FSPIWP复用)
#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_9  // Pin17: 背光PWM控制

#define LCD_TYPE_ST7789_SERIAL
#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 240
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false
#define DISPLAY_INVERT_COLOR true
#define DISPLAY_RGB_ORDER LCD_RGB_ELEMENT_ORDER_RGB
#define DISPLAY_OFFSET_X 0
#define DISPLAY_OFFSET_Y 0
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_SPI_MODE 3

/* ========================================================================
 * 用户接口模块 (User Interface) - 按钮
 * ======================================================================== */

/**
 * Boot按钮 - GPIO0
 * 功能: 系统启动模式选择 / 用户功能按钮
 * 特性: Strapping引脚，上电时电平决定启动模式
 *       - 低电平: 进入Download模式（串口烧录）
 *       - 高电平: 正常启动
 * 位置: Pin27 (芯片右下角)
 * 注意: 需要外部上拉电阻（10kΩ），按下时接地
 */
#define BOOT_BUTTON_GPIO GPIO_NUM_0

/**
 * 模式切换按钮 - GPIO2
 * 功能: 切换设备工作模式（小智模式 <-> 遥控模式/AP模式）
 * 特性: RTC_GPIO2, ADC1_CH1, TOUCH2
 * 位置: Pin38 (芯片右下角)
 * 注意: 需要外部上拉电阻（10kΩ），按下时接地
 */
#define MODE_BUTTON_GPIO GPIO_NUM_2

#define CUBE_ROBOT_VERSION "1.0.0"

#endif  // _BOARD_CONFIG_H_
