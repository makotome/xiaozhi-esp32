/*
 * bt_gamepad_server.cc
 * Otto HP Robot 蓝牙游戏手柄服务器 - BLE版本
 * 适用于 ESP32-S3 芯片
 *
 * 使用 Nordic UART Service (NUS) 协议与 Dabble App 通信
 *
 * 注意：本版本使用 BLE (Bluetooth Low Energy)
 *       如需 Classic Bluetooth (SPP) 版本，请参考 bt_gamepad_server.cc.classic_bt_backup
 *
 * 作者: GitHub Copilot
 * 日期: 2025-11-21
 * 版本: 2.0 (BLE)
 */

#include "bt_gamepad_server.h"
#include "light_mcp_controller.h"
#include <esp_log.h>
#include <esp_random.h>
#include <cmath>
#include <cstring>

// ESP32 BLE headers
#include <nvs_flash.h>
#include <esp_bt.h>
#include <esp_gap_ble_api.h>
#include <esp_gatts_api.h>
#include <esp_bt_main.h>
#include <esp_gatt_common_api.h>

#define TAG "BtGamepadServerBLE"

// ==================== Nordic UART Service (NUS) 定义 ====================

// NUS 服务和特征 UUID
static const uint8_t NUS_SERVICE_UUID[16] = {
    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
    0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E};

static const uint8_t NUS_CHAR_TX_UUID[16] = { // TX: 设备发送给App
    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
    0x93, 0xF3, 0xA3, 0xB5, 0x03, 0x00, 0x40, 0x6E};

static const uint8_t NUS_CHAR_RX_UUID[16] = { // RX: 设备接收App数据
    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
    0x93, 0xF3, 0xA3, 0xB5, 0x02, 0x00, 0x40, 0x6E};

// GATT 接口和服务句柄
#define GATTS_APP_ID 0
#define GATTS_NUM_HANDLE 8

// 全局 BLE 句柄（供心跳和响应函数访问）
uint16_t g_gatts_if = ESP_GATT_IF_NONE;
uint16_t g_conn_id = 0xFFFF;
uint16_t g_service_handle = 0;
uint16_t g_char_tx_handle = 0;
uint16_t g_char_rx_handle = 0;
bool g_is_connected = false;

// ==================== 静态成员初始化 ====================

BtGamepadServer *BtGamepadServer::instance_ = nullptr;

// ==================== 构造与析构 ====================

BtGamepadServer::BtGamepadServer()
    : wheel_controller_(nullptr),
      is_running_(false),
      is_connected_(false),
      move_throttler_(100),   // 移动命令 100ms 间隔
      button_throttler_(500), // 按钮命令 500ms 间隔
      current_data_(),
      dance_light_enabled_(false),
      night_light_enabled_(false)
{
    ESP_LOGI(TAG, "蓝牙游戏手柄服务器已创建 (BLE模式)");
}

BtGamepadServer::~BtGamepadServer()
{
    Stop();
    ESP_LOGI(TAG, "蓝牙游戏手柄服务器已销毁");
}

BtGamepadServer &BtGamepadServer::GetInstance()
{
    if (instance_ == nullptr)
    {
        instance_ = new BtGamepadServer();
    }
    return *instance_;
}

// ==================== BLE 事件回调 ====================

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event)
    {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
    {
        ESP_LOGI(TAG, "广播数据设置完成，开始广播");
        esp_ble_adv_params_t adv_params = BtGamepadServer::GetAdvParams();
        esp_ble_gap_start_advertising(&adv_params);
        break;
    }
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGI(TAG, "BLE 广播已启动，等待 Dabble App 连接...");
        }
        else
        {
            ESP_LOGE(TAG, "BLE 广播启动失败: %d", param->adv_start_cmpl.status);
        }
        break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        ESP_LOGI(TAG, "BLE 广播已停止");
        break;

    default:
        break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    if (BtGamepadServer::GetInstancePtr() == nullptr)
        return;

    switch (event)
    {
    case ESP_GATTS_REG_EVT:
    {
        ESP_LOGI(TAG, "GATT 服务器已注册，app_id=%d, status=%d", param->reg.app_id, param->reg.status);
        g_gatts_if = gatts_if;

        // 设置设备名称
        esp_ble_gap_set_device_name(BtGamepadServer::GetInstance().GetDeviceName());

        // 配置广播数据
        esp_ble_adv_data_t adv_data = BtGamepadServer::GetAdvData();
        esp_ble_gap_config_adv_data(&adv_data);

        // 创建 NUS 服务
        esp_gatt_srvc_id_t service_id = BtGamepadServer::GetServiceId();
        esp_ble_gatts_create_service(gatts_if, &service_id, GATTS_NUM_HANDLE);
        break;
    }
    case ESP_GATTS_CREATE_EVT:
    {
        ESP_LOGI(TAG, "NUS 服务已创建，service_handle=%d", param->create.service_handle);
        g_service_handle = param->create.service_handle;

        // 启动服务
        esp_ble_gatts_start_service(g_service_handle);

        // 添加 TX 特征 (Notify)
        esp_bt_uuid_t tx_uuid = BtGamepadServer::GetTxCharUuid();
        esp_ble_gatts_add_char(g_service_handle, &tx_uuid,
                               ESP_GATT_PERM_READ,
                               ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                               nullptr, nullptr);
        break;
    }
    case ESP_GATTS_ADD_CHAR_EVT:
    {
        if (param->add_char.status == ESP_GATT_OK)
        {
            if (memcmp(param->add_char.char_uuid.uuid.uuid128, NUS_CHAR_TX_UUID, 16) == 0)
            {
                g_char_tx_handle = param->add_char.attr_handle;
                ESP_LOGI(TAG, "TX 特征已添加，handle=%d", g_char_tx_handle);

                // 添加 RX 特征 (Write)
                esp_bt_uuid_t rx_uuid = BtGamepadServer::GetRxCharUuid();
                esp_ble_gatts_add_char(g_service_handle, &rx_uuid,
                                       ESP_GATT_PERM_WRITE,
                                       ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_WRITE_NR,
                                       nullptr, nullptr);
            }
            else if (memcmp(param->add_char.char_uuid.uuid.uuid128, NUS_CHAR_RX_UUID, 16) == 0)
            {
                g_char_rx_handle = param->add_char.attr_handle;
                ESP_LOGI(TAG, "RX 特征已添加，handle=%d", g_char_rx_handle);
            }
        }
        break;
    }
    case ESP_GATTS_CONNECT_EVT:
    {
        ESP_LOGI(TAG, "BLE 连接已建立，conn_id=%d", param->connect.conn_id);
        g_conn_id = param->connect.conn_id;
        g_is_connected = true;
        BtGamepadServer::GetInstance().SetConnected(true);

        // 停止蓝光闪烁，恢复正常显示
        auto *light_controller = GetLightMcpController();
        if (light_controller != nullptr)
        {
            auto *colorful_light = light_controller->getLightController();
            if (colorful_light != nullptr)
            {
                colorful_light->stopAllEffects();
                ESP_LOGI(TAG, "蓝光闪烁已停止，恢复正常显示");
            }
        }

        // 更新连接参数
        esp_ble_conn_update_params_t conn_params = {0};
        memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
        conn_params.min_int = 0x10; // 20ms
        conn_params.max_int = 0x20; // 40ms
        conn_params.latency = 0;
        conn_params.timeout = 400; // 4s
        esp_ble_gap_update_conn_params(&conn_params);
        break;
    }
    case ESP_GATTS_DISCONNECT_EVT:
    {
        ESP_LOGI(TAG, "BLE 连接已断开");
        g_is_connected = false;
        g_conn_id = 0xFFFF;
        BtGamepadServer::GetInstance().SetConnected(false);

        // 重新启动蓝光闪烁，提示等待连接
        auto *light_controller = GetLightMcpController();
        if (light_controller != nullptr)
        {
            auto *colorful_light = light_controller->getLightController();
            if (colorful_light != nullptr)
            {
                colorful_light->setLightMode(ColorfulLightController::MODE_BLUE_FLASH);
                ESP_LOGI(TAG, "蓝光闪烁已重启，等待新连接");
            }
        }

        // 重新开始广播
        esp_ble_adv_params_t adv_params = BtGamepadServer::GetAdvParams();
        esp_ble_gap_start_advertising(&adv_params);
        break;
    }
    case ESP_GATTS_WRITE_EVT:
    {
        // 接收到 Dabble 数据
        if (param->write.handle == g_char_rx_handle && param->write.len > 0)
        {
            ESP_LOGI(TAG, "接收到数据: len=%d, handle=%d (rx_handle=%d)",
                     param->write.len, param->write.handle, g_char_rx_handle);

            // 打印原始数据（用于调试）
            ESP_LOG_BUFFER_HEX(TAG, param->write.value, param->write.len);

            // 使用友元访问私有函数
            BtGamepadServer::GetInstance().OnBleDataReceived(param->write.value, param->write.len);
        }
        else if (param->write.len > 0)
        {
            ESP_LOGW(TAG, "写入到错误的句柄: handle=%d (expected rx_handle=%d)",
                     param->write.handle, g_char_rx_handle);
        }

        // 发送写入响应（对于某些 BLE 写操作可能需要）
        if (param->write.need_rsp && g_is_connected)
        {
            esp_ble_gatts_send_response(gatts_if, param->write.conn_id,
                                        param->write.trans_id, ESP_GATT_OK, nullptr);
        }
        break;
    }
    default:
        break;
    }
}

// ==================== 服务器控制 ====================

bool BtGamepadServer::Start()
{
    if (is_running_)
    {
        ESP_LOGW(TAG, "服务器已在运行");
        return true;
    }

    ESP_LOGI(TAG, "启动蓝牙游戏手柄服务器 (BLE模式)...");

    // 获取轮子控制器
    wheel_controller_ = GetWheelRobotController();
    if (wheel_controller_ == nullptr)
    {
        ESP_LOGE(TAG, "无法获取轮子控制器");
        return false;
    }

    // ===== 初始化 BLE 协议栈 =====

    // 1. 初始化 NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. 释放 Classic Bluetooth 内存（只使用 BLE）
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    // 3. 初始化蓝牙控制器
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "蓝牙控制器初始化失败: %s", esp_err_to_name(ret));
        return false;
    }

    // 4. 使能 BLE 模式
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "BLE 控制器使能失败: %s", esp_err_to_name(ret));
        return false;
    }

    // 5. 初始化 Bluedroid 协议栈
    ret = esp_bluedroid_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Bluedroid 初始化失败: %s", esp_err_to_name(ret));
        return false;
    }

    // 6. 使能 Bluedroid
    ret = esp_bluedroid_enable();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Bluedroid 使能失败: %s", esp_err_to_name(ret));
        return false;
    }

    // 7. 注册 GAP 回调
    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "GAP 回调注册失败: %s", esp_err_to_name(ret));
        return false;
    }

    // 8. 注册 GATTS 回调
    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "GATTS 回调注册失败: %s", esp_err_to_name(ret));
        return false;
    }

    // 9. 注册 GATT 应用
    ret = esp_ble_gatts_app_register(GATTS_APP_ID);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "GATT 应用注册失败: %s", esp_err_to_name(ret));
        return false;
    }

    // 10. 设置 MTU
    ret = esp_ble_gatt_set_local_mtu(517);
    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "设置 MTU 失败: %s", esp_err_to_name(ret));
    }

    is_running_ = true;
    ESP_LOGI(TAG, "蓝牙游戏手柄服务器已启动 (BLE)");
    ESP_LOGI(TAG, "设备名称: %s", DEVICE_NAME);
    ESP_LOGI(TAG, "等待 Dabble App 连接...");

    // 启动蓝光闪烁效果，提示等待连接
    auto *light_controller = GetLightMcpController();
    if (light_controller != nullptr)
    {
        auto *colorful_light = light_controller->getLightController();
        if (colorful_light != nullptr)
        {
            colorful_light->setLightMode(ColorfulLightController::MODE_BLUE_FLASH);
            ESP_LOGI(TAG, "蓝光闪烁已启动，提示等待蓝牙连接");
        }
    }

    return true;
}

void BtGamepadServer::Stop()
{
    if (!is_running_)
    {
        return;
    }

    ESP_LOGI(TAG, "停止蓝牙游戏手柄服务器...");

    // 停止所有运动
    StopMovement();

    // 关闭灯光
    auto *light_controller = GetLightMcpController();
    if (light_controller != nullptr)
    {
        auto *colorful_light = light_controller->getLightController();
        if (colorful_light != nullptr)
        {
            colorful_light->stopAllEffects();
        }
    }

    // ===== 清理 BLE 资源 =====

    // 1. 断开连接
    if (g_is_connected && g_conn_id != 0xFFFF && g_gatts_if != ESP_GATT_IF_NONE)
    {
        esp_ble_gatts_close(g_gatts_if, g_conn_id);
    }

    // 2. 停止广播
    esp_ble_gap_stop_advertising();

    // 3. 注销 GATT 应用
    if (g_gatts_if != ESP_GATT_IF_NONE)
    {
        esp_ble_gatts_app_unregister(g_gatts_if);
    }

    // 4. 禁用 Bluedroid
    esp_bluedroid_disable();
    esp_bluedroid_deinit();

    // 5. 禁用蓝牙控制器
    esp_bt_controller_disable();
    esp_bt_controller_deinit();

    is_running_ = false;
    is_connected_ = false;
    g_is_connected = false;
    g_conn_id = 0xFFFF;
    g_gatts_if = ESP_GATT_IF_NONE;
    dance_light_enabled_ = false;
    night_light_enabled_ = false;

    // 重置节流器
    move_throttler_.Reset();
    button_throttler_.Reset();

    ESP_LOGI(TAG, "蓝牙游戏手柄服务器已停止");
}

// ==================== 数据处理 ====================

void BtGamepadServer::ParseDabbleData(const uint8_t *data, size_t length)
{
    // 统计接收频率（用于诊断）
    static uint32_t last_recv_time = 0;
    static uint32_t recv_count = 0;
    uint32_t now = esp_timer_get_time() / 1000; // 毫秒

    recv_count++;
    if (now - last_recv_time > 1000) // 每秒统计一次
    {
        ESP_LOGI(TAG, "📊 数据接收频率: %d 包/秒", recv_count);
        recv_count = 0;
        last_recv_time = now;
    }

    // ===== Dabble BLE 协议格式说明 =====
    //
    // 完整 Dabble 协议 (串口/Classic BT):
    //   [0xFF][MODULE_ID][FUNCTION_ID][ARG_COUNT][ARG_LEN][DATA...][0x00]
    //
    // 但是 BLE NUS 传输使用简化格式:
    //   [0xFF][MODULE_ID][FUNCTION_ID][value0][value][...]
    //
    // 实际接收数据: ff 00 03 00 00 00
    //   [0] = 0xFF  - START_OF_FRAME
    //   [1] = 0x00  - MODULE_ID (0x00=Dabble主控制器, 0x01=GamePad模块)
    //   [2] = 0x03  - FUNCTION_ID (0x01=Digital, 0x02=Joystick, 0x03=Accelerometer)
    //   [3] = 0x00  - value0 (特殊按钮: START/SELECT/TRIANGLE/CIRCLE/CROSS/SQUARE)
    //   [4] = 0x00  - value (方向键或摇杆编码数据)
    //   [5] = 0x00  - 可能是 END_OF_FRAME 或填充字节
    //
    // 按钮位映射 (value0 字节):
    //   Bit0 = START
    //   Bit1 = SELECT
    //   Bit2 = TRIANGLE
    //   Bit3 = CIRCLE
    //   Bit4 = CROSS
    //   Bit5 = SQUARE
    //
    // 方向数据 (value 字节, Digital 模式):
    //   Bit0 = UP
    //   Bit1 = DOWN
    //   Bit2 = LEFT
    //   Bit3 = RIGHT
    //
    // 摇杆数据 (value 字节, Joystick/Accelerometer 模式):
    //   高5位 (>>3) * 15 = 角度 (0~345°, 步进15°)
    //   低3位 (&0x07) = 半径 (0~7)

    if (data == nullptr || length < 4)
    {
        ESP_LOGW(TAG, "数据包太短: length=%d", length);
        return;
    }

    // 打印原始数据用于调试
    ESP_LOGI(TAG, "📡 接收Dabble数据 (%d字节):", length);
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, data, length, ESP_LOG_INFO);

    // 验证帧头
    if (data[0] != 0xFF)
    {
        ESP_LOGW(TAG, "❌ 错误的帧头: 0x%02X (期望 0xFF)", data[0]);
        return;
    }

    uint8_t module_id = data[1];

    // **特殊处理**: Dabble 控制命令 (MODULE_ID=0x00 时可能是系统命令)
    if (module_id == 0x00 && length >= 3)
    {
        uint8_t function_id = data[2];

        // 0x01 = CHECK_CONNECTION - 连接检查（心跳）
        if (function_id == 0x01)
        {
            ESP_LOGI(TAG, "💓 收到心跳检查，发送确认响应");
            SendHeartbeatResponse();
            return; // 心跳命令不需要继续处理
        }
        // 0x03 = BOARDID_REQUEST - 板卡ID请求
        else if (function_id == 0x03)
        {
            ESP_LOGI(TAG, "📋 收到板卡ID请求，发送ESP32-S3标识");
            SendBoardIdResponse();
            return; // 板卡ID请求不需要继续处理
        }
    }

    // **关键修正**: Dabble BLE 通过 NUS 传输时,数据包格式简化为:
    // [0xFF][MODULE_ID][FUNCTION_ID][value0][value][...]
    //
    // 实际接收: ff 00 03 00 00 00
    //   data[0] = 0xFF (START_OF_FRAME)
    //   data[1] = 0x00 (MODULE_ID, 0x00=Dabble控制器, 0x01=GamePad)
    //   data[2] = 0x03 (FUNCTION_ID, 1=Digital, 2=Joystick, 3=Accelerometer)
    //   data[3] = 0x00 (value0 - 特殊按钮)
    //   data[4] = 0x00 (value - 方向/摇杆数据)
    //   data[5] = 0x00 (可能是结束符或填充)

    // MODULE_ID 可能是 0x00 (Dabble) 或 0x01 (GamePad), 都接受
    if (module_id != 0x00 && module_id != 0x01)
    {
        ESP_LOGW(TAG, "⚠️ 未知模块 (module_id=0x%02X), 忽略", module_id);
        ESP_LOGW(TAG, "   如果您在操作遥控器但收到此消息，请报告完整数据包");
        return;
    }

    if (length < 5)
    {
        ESP_LOGW(TAG, "数据包长度不足: %d (期望至少5字节)", length);
        return;
    }

    // **最终修正: Dabble BLE 实际格式**
    // [0xFF][MODULE_ID][FUNCTION_ID][LENGTH][MODE][按钮][方向][填充]
    //    0      1          2           3      4     5     6     7
    //
    // 实际数据验证:
    //   ff 01 01 01 02 00 01 00  ← UP    (data[6]=0x01)
    //   ff 01 01 01 02 00 02 00  ← DOWN  (data[6]=0x02)
    //   ff 01 01 01 02 00 04 00  ← LEFT  (data[6]=0x04)
    //   ff 01 01 01 02 00 08 00  ← RIGHT (data[6]=0x08)
    //   ff 01 01 01 02 04 00 00  ← 按钮  (data[5]=0x04)
    //   ff 01 01 01 02 08 00 00  ← 按钮  (data[5]=0x08)
    //
    // 结论:
    //   data[3] = 0x01 = LENGTH (固定)
    //   data[4] = 0x02 = MODE (固定，可能表示Digital模式)
    //   data[5] = 按钮状态 (START/SELECT/TRIANGLE/CIRCLE/CROSS/SQUARE)
    //   data[6] = 方向状态 (UP/DOWN/LEFT/RIGHT)

    uint8_t function_id = data[2]; // FUNCTION_ID (1/2/3)

    if (length < 7)
    {
        ESP_LOGW(TAG, "数据包长度不足: %d (期望至少7字节)", length);
        return;
    }

    // **最终修正**: 按钮在 data[5], 方向在 data[6]
    uint8_t value0 = data[5]; // 按钮状态 (START/SELECT/TRIANGLE/CIRCLE/CROSS/SQUARE)
    uint8_t value = data[6];  // 方向键或摇杆数据

    ESP_LOGI(TAG, "🎮 Dabble GamePad 解析:");
    ESP_LOGI(TAG, "   MODULE_ID   = 0x%02X (%s)", module_id,
             module_id == 0x00 ? "Dabble主控" : "GamePad模块");
    ESP_LOGI(TAG, "   FUNCTION_ID = 0x%02X (%s)", function_id,
             function_id == 0x01 ? "Digital" : function_id == 0x02 ? "Joystick"
                                           : function_id == 0x03   ? "Accelerometer"
                                                                   : "Unknown");
    ESP_LOGI(TAG, "   value0      = 0x%02X (按钮状态)", value0);
    ESP_LOGI(TAG, "   value       = 0x%02X (方向/摇杆)", value);

    DabbleGamepadData gamepad_data;
    uint16_t buttons = 0;

    // 解析特殊按钮 (value0)
    // 位映射来自 GamePadModule.h:
    // Bit0=START, Bit1=SELECT, Bit2=TRIANGLE, Bit3=CIRCLE, Bit4=CROSS, Bit5=SQUARE
    if (value0 & (1 << 0)) // START_BIT
    {
        buttons |= kDabbleButtonStart;
        ESP_LOGI(TAG, "✓ START 按钮按下");
    }
    if (value0 & (1 << 1)) // SELECT_BIT
    {
        buttons |= kDabbleButtonSelect;
        ESP_LOGI(TAG, "✓ SELECT 按钮按下");
    }
    if (value0 & (1 << 2)) // TRIANGLE_BIT
    {
        buttons |= kDabbleButton3; // 映射到按钮3
        ESP_LOGI(TAG, "✓ TRIANGLE 按钮按下");
    }
    if (value0 & (1 << 3)) // CIRCLE_BIT
    {
        buttons |= kDabbleButton4; // 映射到按钮4
        ESP_LOGI(TAG, "✓ CIRCLE 按钮按下");
    }
    if (value0 & (1 << 4)) // CROSS_BIT
    {
        buttons |= kDabbleButton1; // 映射到按钮1
        ESP_LOGI(TAG, "✓ CROSS 按钮按下");
    }
    if (value0 & (1 << 5)) // SQUARE_BIT
    {
        buttons |= kDabbleButton2; // 映射到按钮2
        ESP_LOGI(TAG, "✓ SQUARE 按钮按下");
    }

    gamepad_data.buttons = buttons;

    // 根据 FUNCTION_ID 解析方向/摇杆数据
    if (function_id == 0x01) // GAMEPAD_DIGITAL
    {
        // Digital 模式: value 的位映射
        // Bit0=UP, Bit1=DOWN, Bit2=LEFT, Bit3=RIGHT

        gamepad_data.mode = kDabbleModeDigital;
        gamepad_data.up = !!(value & (1 << 0));
        gamepad_data.down = !!(value & (1 << 1));
        gamepad_data.left = !!(value & (1 << 2));
        gamepad_data.right = !!(value & (1 << 3));

        ESP_LOGI(TAG, "Digital 方向键: UP=%d DOWN=%d LEFT=%d RIGHT=%d",
                 gamepad_data.up, gamepad_data.down, gamepad_data.left, gamepad_data.right);
    }
    else if (function_id == 0x02 || function_id == 0x03) // GAMEPAD_ANALOG or GAMEPAD_ACCL
    {
        // Joystick/Accelerometer 模式
        // value 编码格式: XXXXXYYY
        //   XXXXX (高5位) * 15 = 角度 (0~360度)
        //   YYY (低3位) = 半径 (0~7)

        gamepad_data.mode = (function_id == 0x02) ? kDabbleModeJoystick : kDabbleModeAccelerometer;

        uint8_t angle_index = (value >> 3) & 0x1F; // 提取高5位
        uint8_t radius = value & 0x07;             // 提取低3位

        uint16_t angle_deg = angle_index * 15; // 0~345度 (步进15度)
        gamepad_data.angle = angle_deg;
        gamepad_data.radius = radius;

        // 转换为 X/Y 坐标 (-7 到 +7)
        float angle_rad = angle_deg * M_PI / 180.0f;
        float x_float = radius * cosf(angle_rad);
        float y_float = radius * sinf(angle_rad);

        gamepad_data.axis_x = static_cast<int8_t>(x_float * 18.14f); // 归一化到 -127~127
        gamepad_data.axis_y = static_cast<int8_t>(y_float * 18.14f);

        ESP_LOGI(TAG, "%s: angle=%d° radius=%d → x=%d y=%d",
                 (function_id == 0x02) ? "Joystick" : "Accelerometer",
                 angle_deg, radius, gamepad_data.axis_x, gamepad_data.axis_y);
    }
    else
    {
        ESP_LOGW(TAG, "未知的 FUNCTION_ID: 0x%02X", function_id);
        return;
    }

    ESP_LOGI(TAG, "✓ 解析完成: mode=%d buttons=0x%04X",
             static_cast<int>(gamepad_data.mode), gamepad_data.buttons);

    // 处理数据
    ProcessGamepadData(gamepad_data);

    // 可选：发送心跳响应（防止自动断开）
    // SendHeartbeat();
}

void BtGamepadServer::ProcessGamepadData(const DabbleGamepadData &data)
{
    // 保存当前数据
    current_data_ = data;

    // 优先处理按钮（包括紧急停止）
    // 注意：如果有按钮按下，不再处理方向键，避免冲突
    if (data.buttons != 0)
    {
        HandleButtonPress(data.buttons);
        return; // ← 修复：按钮处理后立即返回，不处理移动
    }

    // 根据模式处理移动（只有在没有按钮按下时才执行）
    switch (data.mode)
    {
    case kDabbleModeDigital:
        HandleDigitalMode(data);
        break;

    case kDabbleModeJoystick:
        HandleJoystickMode(data);
        break;

    case kDabbleModeAccelerometer:
        HandleAccelerometerMode(data);
        break;

    default:
        ESP_LOGW(TAG, "未知的控制模式: %d", data.mode);
        break;
    }
}

// ==================== 移动控制 - 三种模式 ====================

void BtGamepadServer::HandleDigitalMode(const DabbleGamepadData &data)
{
    // 节流检查
    if (!move_throttler_.CanExecute())
    {
        return;
    }

    const int speed = DEFAULT_DIGITAL_SPEED;

    if (data.up)
    {
        // 前进（直线）
        MoveWithOmniDirection(speed, 0.0f, true);
        ESP_LOGD(TAG, "[Digital] 前进");
    }
    else if (data.down)
    {
        // 后退（直线）
        MoveWithOmniDirection(speed, 0.0f, false);
        ESP_LOGD(TAG, "[Digital] 后退");
    }
    else if (data.left)
    {
        // 左转（前进+左满方向）
        MoveWithOmniDirection(speed, -1.0f, true);
        ESP_LOGD(TAG, "[Digital] 左转");
    }
    else if (data.right)
    {
        // 右转（前进+右满方向）
        MoveWithOmniDirection(speed, 1.0f, true);
        ESP_LOGD(TAG, "[Digital] 右转");
    }
    else
    {
        // 无方向键按下，停止
        StopMovement();
    }
}

void BtGamepadServer::HandleJoystickMode(const DabbleGamepadData &data)
{
    // 应用死区
    int8_t x = ApplyDeadzone(data.axis_x, DEADZONE_THRESHOLD);
    int8_t y = ApplyDeadzone(data.axis_y, DEADZONE_THRESHOLD);

    // 摇杆在中心，停止
    if (x == 0 && y == 0)
    {
        StopMovement();
        return;
    }

    // 节流检查
    if (!move_throttler_.CanExecute())
    {
        return;
    }

    // 计算速度和方向
    int speed = CalculateSpeedFromXY(x, y);
    float direction = CalculateDirectionFromXY(x, y);
    bool is_forward = !IsMoveBackward(y);

    // 执行万向移动
    MoveWithOmniDirection(speed, direction, is_forward);

    ESP_LOGD(TAG, "[Joystick] %s: speed=%d, direction=%.2f (x=%d, y=%d)",
             is_forward ? "前进" : "后退", speed, direction, x, y);
}

void BtGamepadServer::HandleAccelerometerMode(const DabbleGamepadData &data)
{
    // 加速度计模式复用摇杆逻辑
    // axis_x 和 axis_y 来自手机倾斜角度
    HandleJoystickMode(data);
}

// ==================== 万向移动核心 ====================

void BtGamepadServer::MoveWithOmniDirection(int speed, float direction, bool is_forward)
{
    if (wheel_controller_ == nullptr)
    {
        ESP_LOGW(TAG, "轮子控制器未初始化");
        return;
    }

    // 限制参数范围
    if (speed < 0)
        speed = 0;
    if (speed > 100)
        speed = 100;
    if (direction < -1.0f)
        direction = -1.0f;
    if (direction > 1.0f)
        direction = 1.0f;

    // 调用底层万向移动接口
    if (is_forward)
    {
        wheel_controller_->GetWheels().moveForwardWithDirection(speed, direction);
    }
    else
    {
        wheel_controller_->GetWheels().moveBackwardWithDirection(speed, direction);
    }
}

void BtGamepadServer::StopMovement()
{
    if (wheel_controller_ != nullptr)
    {
        wheel_controller_->GetWheels().stopAll();
        ESP_LOGD(TAG, "停止移动");
    }
}

// ==================== 按钮处理 ====================

void BtGamepadServer::HandleButtonPress(uint16_t buttons)
{
    // START 按钮 - 紧急停止（无节流）
    if (buttons & kDabbleButtonStart)
    {
        OnStartPress();
        return; // START 优先级最高，立即返回
    }

    // 其他按钮需要节流
    if (!button_throttler_.CanExecute())
    {
        return;
    }

    if (buttons & kDabbleButton1)
    {
        OnButton1Press();
    }
    else if (buttons & kDabbleButton2)
    {
        OnButton2Press();
    }
    else if (buttons & kDabbleButton3)
    {
        OnButton3Press();
    }
    else if (buttons & kDabbleButton4)
    {
        OnButton4Press();
    }
}

void BtGamepadServer::OnButton1Press()
{
    ESP_LOGI(TAG, "按钮1: 停止移动");
    StopMovement();
}

void BtGamepadServer::OnButton2Press()
{
    ESP_LOGI(TAG, "按钮2: 执行跳舞");

    if (wheel_controller_ == nullptr)
    {
        return;
    }

    // 随机选择跳舞动作
    int dance_type = esp_random() % 5;

    switch (dance_type)
    {
    case 0:
        wheel_controller_->GetWheels().danceShake();
        ESP_LOGI(TAG, "执行: 摇摆舞");
        break;
    case 1:
        wheel_controller_->GetWheels().danceSpin();
        ESP_LOGI(TAG, "执行: 旋转舞");
        break;
    case 2:
        wheel_controller_->GetWheels().danceWave();
        ESP_LOGI(TAG, "执行: 波浪舞");
        break;
    case 3:
        wheel_controller_->GetWheels().danceZigzag();
        ESP_LOGI(TAG, "执行: 之字舞");
        break;
    case 4:
        wheel_controller_->GetWheels().danceMoonwalk();
        ESP_LOGI(TAG, "执行: 太空步");
        break;
    }
}

void BtGamepadServer::OnButton3Press()
{
    ESP_LOGI(TAG, "按钮3: 切换跳舞灯光");

    dance_light_enabled_ = !dance_light_enabled_;

    auto *light_controller = GetLightMcpController();
    if (light_controller != nullptr)
    {
        auto *colorful_light = light_controller->getLightController();
        if (colorful_light != nullptr)
        {
            if (dance_light_enabled_)
            {
                // 启动跳舞派对灯光效果
                colorful_light->setLightMode(ColorfulLightController::MODE_DANCE_PARTY);
                ESP_LOGI(TAG, "跳舞灯光: 开启 (五彩缤纷模式)");
            }
            else
            {
                // 停止灯光效果
                colorful_light->stopAllEffects();
                ESP_LOGI(TAG, "跳舞灯光: 关闭");
            }
        }
        else
        {
            ESP_LOGW(TAG, "彩色灯光控制器未初始化");
        }
    }
    else
    {
        ESP_LOGW(TAG, "灯光MCP控制器未初始化");
    }
}

void BtGamepadServer::OnButton4Press()
{
    ESP_LOGI(TAG, "按钮4: 切换夜光模式");

    night_light_enabled_ = !night_light_enabled_;

    auto *light_controller = GetLightMcpController();
    if (light_controller != nullptr)
    {
        auto *colorful_light = light_controller->getLightController();
        if (colorful_light != nullptr)
        {
            if (night_light_enabled_)
            {
                // 开启夜光 (纯白光模式)
                colorful_light->setLightMode(ColorfulLightController::MODE_NIGHT_LIGHT);
                ESP_LOGI(TAG, "夜光模式: 开启 (纯白光)");
            }
            else
            {
                // 关闭夜光
                colorful_light->stopAllEffects();
                ESP_LOGI(TAG, "夜光模式: 关闭");
            }
        }
        else
        {
            ESP_LOGW(TAG, "彩色灯光控制器未初始化");
        }
    }
    else
    {
        ESP_LOGW(TAG, "灯光MCP控制器未初始化");
    }
}

void BtGamepadServer::OnStartPress()
{
    ESP_LOGI(TAG, "START: 紧急停止 + 关闭所有灯光");

    // 立即停止移动
    StopMovement();

    // 关闭所有灯光
    auto *light_controller = GetLightMcpController();
    if (light_controller != nullptr)
    {
        auto *colorful_light = light_controller->getLightController();
        if (colorful_light != nullptr)
        {
            colorful_light->stopAllEffects();
            ESP_LOGI(TAG, "所有灯光效果已关闭");
        }
    }

    // 重置状态
    dance_light_enabled_ = false;
    night_light_enabled_ = false;

    // 重置节流器（允许立即再次发送命令）
    move_throttler_.Reset();
    button_throttler_.Reset();

    ESP_LOGI(TAG, "紧急停止完成");
}

// ==================== 辅助函数 ====================

int8_t BtGamepadServer::ApplyDeadzone(int8_t value, int8_t threshold)
{
    if (std::abs(value) < threshold)
    {
        return 0;
    }
    return value;
}

int BtGamepadServer::CalculateSpeedFromXY(int8_t x, int8_t y)
{
    // 使用向量长度作为速度
    float magnitude = std::sqrt(static_cast<float>(x * x + y * y));

    // 映射到 0-100 范围
    // 摇杆最大值约为 127*sqrt(2) ≈ 180
    int speed = static_cast<int>((magnitude * 100.0f) / 127.0f);

    // 限制范围
    if (speed > 100)
        speed = 100;
    if (speed < 0)
        speed = 0;

    return speed;
}

float BtGamepadServer::CalculateDirectionFromXY(int8_t x, int8_t y)
{
    // x 代表左右方向
    // -127 (左) -> -1.0
    //    0 (中) ->  0.0
    //  127 (右) ->  1.0

    float direction = static_cast<float>(x) / 127.0f;

    // 限制范围
    if (direction > 1.0f)
        direction = 1.0f;
    if (direction < -1.0f)
        direction = -1.0f;

    return direction;
}

bool BtGamepadServer::IsMoveBackward(int8_t y)
{
    // Y 轴正值为前进，负值为后退
    return y < 0;
}

// ==================== BLE 心跳和系统响应 ====================

/**
 * 发送心跳响应
 *
 * 原理：
 * - Dabble App 定期发送 CHECK_CONNECTION 命令 (0xFF 0x00 0x01)
 * - 设备收到后需要回复确认，证明连接仍然活跃
 * - 如果长时间不响应，App 可能认为设备断开并关闭连接
 *
 * 数据格式：
 *   [0xFF][0x00][0x01][0x00]
 *   ↑     ↑     ↑     ↑
 *   帧头  模块  功能  结束
 */
void BtGamepadServer::SendHeartbeatResponse()
{
    if (!is_connected_)
    {
        ESP_LOGW(TAG, "未连接，无法发送心跳响应");
        return;
    }

    // Dabble 心跳响应格式
    uint8_t heartbeat[] = {0xFF, 0x00, 0x01, 0x00};

    // 通过 BLE TX 特征发送数据
    if (g_gatts_if != ESP_GATT_IF_NONE && g_conn_id != 0xFFFF && g_char_tx_handle != 0)
    {
        esp_err_t ret = esp_ble_gatts_send_indicate(
            g_gatts_if,
            g_conn_id,
            g_char_tx_handle,
            sizeof(heartbeat),
            heartbeat,
            false // 不需要确认
        );

        if (ret == ESP_OK)
        {
            ESP_LOGD(TAG, "💓 心跳响应已发送");
        }
        else
        {
            ESP_LOGW(TAG, "心跳响应发送失败: %s", esp_err_to_name(ret));
        }
    }
    else
    {
        ESP_LOGW(TAG, "BLE 句柄无效，无法发送心跳");
    }
}

/**
 * 发送板卡ID响应
 *
 * 原理：
 * - Dabble App 连接时会查询设备类型（Arduino、ESP32等）
 * - 设备需要回复板卡ID，App 据此调整界面和功能
 * - ESP32 板卡ID = 4
 *
 * 数据格式：
 *   [0xFF][0x00][0x03][0x01][0x04][BoardID][1][5][1][0x00]
 *   ↑     ↑     ↑     ↑     ↑     ↑        ↑ ↑ ↑ ↑
 *   帧头  模块  功能  长度  板类   ESP32    固定参数  结束
 */
void BtGamepadServer::SendBoardIdResponse()
{
    if (!is_connected_)
    {
        ESP_LOGW(TAG, "未连接，无法发送板卡ID响应");
        return;
    }

    // Dabble 板卡ID响应格式
    // Board IDs: Mega=1, Uno=2, Nano=3, ESP32=4, ESP8266=5
    uint8_t board_id_response[] = {
        0xFF, // 帧头
        0x00, // 模块ID (Dabble主控)
        0x03, // 功能ID (BOARDID_RESPONSE)
        0x01, // 数据长度
        0x04, // 板卡类型 (ESP32=4)
        0x04, // 板卡ID (重复)
        0x01, // 固定参数1
        0x05, // 固定参数2
        0x01, // 固定参数3
        0x00  // 结束符
    };

    // 通过 BLE TX 特征发送数据
    if (g_gatts_if != ESP_GATT_IF_NONE && g_conn_id != 0xFFFF && g_char_tx_handle != 0)
    {
        esp_err_t ret = esp_ble_gatts_send_indicate(
            g_gatts_if,
            g_conn_id,
            g_char_tx_handle,
            sizeof(board_id_response),
            board_id_response,
            false // 不需要确认
        );

        if (ret == ESP_OK)
        {
            ESP_LOGI(TAG, "📋 板卡ID响应已发送 (ESP32-S3)");
        }
        else
        {
            ESP_LOGW(TAG, "板卡ID响应发送失败: %s", esp_err_to_name(ret));
        }
    }
    else
    {
        ESP_LOGW(TAG, "BLE 句柄无效，无法发送板卡ID");
    }
}

// ==================== BLE 配置（静态成员）====================

esp_ble_adv_data_t BtGamepadServer::GetAdvData()
{
    static uint8_t adv_service_uuid128[16] = {
        0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
        0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E};

    static esp_ble_adv_data_t adv_data = {
        .set_scan_rsp = false,
        .include_name = true,
        .include_txpower = true,
        .min_interval = 0x0006, // 7.5ms
        .max_interval = 0x0010, // 20ms
        .appearance = 0x03C4,   // Gamepad
        .manufacturer_len = 0,
        .p_manufacturer_data = nullptr,
        .service_data_len = 0,
        .p_service_data = nullptr,
        .service_uuid_len = sizeof(adv_service_uuid128),
        .p_service_uuid = adv_service_uuid128,
        .flag = ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT,
    };

    return adv_data;
}

esp_ble_adv_params_t BtGamepadServer::GetAdvParams()
{
    static esp_ble_adv_params_t adv_params = {
        .adv_int_min = 0x20, // 20ms
        .adv_int_max = 0x40, // 40ms
        .adv_type = ADV_TYPE_IND,
        .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
        .channel_map = ADV_CHNL_ALL,
        .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
    };

    return adv_params;
}

esp_gatt_srvc_id_t BtGamepadServer::GetServiceId()
{
    static esp_gatt_srvc_id_t service_id;
    service_id.is_primary = true;
    service_id.id.inst_id = 0;
    service_id.id.uuid.len = ESP_UUID_LEN_128;
    memcpy(service_id.id.uuid.uuid.uuid128, NUS_SERVICE_UUID, ESP_UUID_LEN_128);

    return service_id;
}

esp_bt_uuid_t BtGamepadServer::GetTxCharUuid()
{
    static esp_bt_uuid_t tx_uuid = {
        .len = ESP_UUID_LEN_128,
        .uuid = {.uuid128 = {
                     0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
                     0x93, 0xF3, 0xA3, 0xB5, 0x03, 0x00, 0x40, 0x6E}},
    };

    return tx_uuid;
}

esp_bt_uuid_t BtGamepadServer::GetRxCharUuid()
{
    static esp_bt_uuid_t rx_uuid = {
        .len = ESP_UUID_LEN_128,
        .uuid = {.uuid128 = {
                     0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
                     0x93, 0xF3, 0xA3, 0xB5, 0x02, 0x00, 0x40, 0x6E}},
    };

    return rx_uuid;
}
