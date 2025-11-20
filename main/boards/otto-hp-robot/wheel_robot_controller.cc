/*
    wheel_robot_controller.cc
    两轮机器人控制器实现
    基于 wheel_movements 实现两轮机器人的运动控制
*/

#include "wheel_robot_controller.h"

#include <esp_log.h>
#include <esp_random.h>

#include "mcp_server.h"

#define TAG "WheelRobotController"

// 静态任务函数 - 处理队列中的动作
void WheelRobotController::ActionTask(void *arg)
{
    WheelRobotController *controller = static_cast<WheelRobotController *>(arg);
    WheelActionParams params;

    while (true)
    {
        if (xQueueReceive(controller->action_queue_, &params, portMAX_DELAY) == pdTRUE)
        {
            controller->is_action_in_progress_ = true;

            switch (params.action_type)
            {
            case ACTION_FORWARD:
                controller->wheels_.moveForward(params.speed);
                break;

            case ACTION_BACKWARD:
                controller->wheels_.moveBackward(params.speed);
                break;

            case ACTION_TURN_LEFT:
                controller->wheels_.turnLeft(params.speed);
                break;

            case ACTION_TURN_RIGHT:
                controller->wheels_.turnRight(params.speed);
                break;

            case ACTION_SPIN_LEFT:
                // 原地左转 - turnLeft 已经实现了原地转向
                controller->wheels_.turnLeft(params.speed);
                break;

            case ACTION_SPIN_RIGHT:
                // 原地右转 - turnRight 已经实现了原地转向
                controller->wheels_.turnRight(params.speed);
                break;

            case ACTION_STOP:
                controller->wheels_.stopAll();
                break;

            case ACTION_ACCELERATE:
                controller->wheels_.accelerate(params.target_speed, params.duration_ms);
                break;

            case ACTION_DECELERATE:
                controller->wheels_.decelerate(params.duration_ms);
                break;

            case ACTION_CUSTOM_SPEED:
                controller->wheels_.setWheelSpeeds(params.left_speed, params.right_speed);
                break;

            case ACTION_DANCE_SHAKE:
                controller->wheels_.danceShake();
                break;

            case ACTION_DANCE_SPIN:
                controller->wheels_.danceSpin();
                break;

            case ACTION_DANCE_WAVE:
                controller->wheels_.danceWave();
                break;

            case ACTION_DANCE_ZIGZAG:
                controller->wheels_.danceZigzag();
                break;

            case ACTION_DANCE_MOONWALK:
                controller->wheels_.danceMoonwalk();
                break;

            case ACTION_FORWARD_DIRECTION:
                controller->wheels_.moveForwardWithDirection(params.speed, params.direction);
                break;

            case ACTION_BACKWARD_DIRECTION:
                controller->wheels_.moveBackwardWithDirection(params.speed, params.direction);
                break;

            default:
                ESP_LOGW(TAG, "未知动作类型: %d", params.action_type);
                break;
            }

            // 如果指定了持续时间，等待后停止
            if (params.duration_ms > 0 && params.action_type != ACTION_STOP)
            {
                vTaskDelay(pdMS_TO_TICKS(params.duration_ms));
                controller->wheels_.stopAll();
            }

            controller->is_action_in_progress_ = false;
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
}

// 启动动作任务（如果还未启动）
void WheelRobotController::StartActionTaskIfNeeded()
{
    if (action_task_handle_ == nullptr)
    {
        xTaskCreate(ActionTask, "wheel_action", 4096, this, configMAX_PRIORITIES - 1,
                    &action_task_handle_);
    }
}

// 将动作加入队列
void WheelRobotController::QueueAction(int action_type, int speed, int duration_ms,
                                       int left_speed, int right_speed, int target_speed,
                                       float direction)
{
    ESP_LOGI(TAG, "队列动作: 类型=%d, 速度=%d, 持续=%dms, 左=%d, 右=%d, 目标=%d, 方向=%.2f",
             action_type, speed, duration_ms, left_speed, right_speed, target_speed, direction);

    WheelActionParams params = {
        action_type, speed, duration_ms, left_speed, right_speed, target_speed, direction};

    xQueueSend(action_queue_, &params, portMAX_DELAY);
    StartActionTaskIfNeeded();
}

// 构造函数
WheelRobotController::WheelRobotController()
    : action_task_handle_(nullptr), action_queue_(nullptr), is_action_in_progress_(false)
{
    ESP_LOGI(TAG, "初始化两轮机器人控制器...");

    // 1. 创建动作队列（必须先创建，因为后面会用到）
    action_queue_ = xQueueCreate(10, sizeof(WheelActionParams));
    if (action_queue_ == nullptr)
    {
        ESP_LOGE(TAG, "创建队列失败");
        return;
    }

    // 2. 初始化轮子控制
    if (!wheels_.init())
    {
        ESP_LOGE(TAG, "轮子初始化失败");
        return;
    }

    // 4. 设置初始停止状态（最后设置，确保一切就绪）
    QueueAction(ACTION_STOP);

    ESP_LOGI(TAG, "两轮机器人控制器初始化成功");
}

// 析构函数
WheelRobotController::~WheelRobotController()
{
    if (action_task_handle_ != nullptr)
    {
        vTaskDelete(action_task_handle_);
        action_task_handle_ = nullptr;
    }

    if (action_queue_ != nullptr)
    {
        vQueueDelete(action_queue_);
        action_queue_ = nullptr;
    }
}

// 注册 MCP 工具
void WheelRobotController::RegisterMcpTools()
{
    auto &mcp_server = McpServer::GetInstance();

    ESP_LOGI(TAG, "开始注册MCP工具...");

    // 1. 前进
    mcp_server.AddTool(
        "self.wheel.move_forward",
        "前进。speed: 速度(0-100); duration_ms: 持续时间(毫秒，0表示持续运动)",
        PropertyList({Property("speed", kPropertyTypeInteger, 50, 0, 100),
                      Property("duration_ms", kPropertyTypeInteger, 0, 0, 60000)}),
        [this](const PropertyList &properties) -> ReturnValue
        {
            int speed = properties["speed"].value<int>();
            int duration_ms = properties["duration_ms"].value<int>();
            QueueAction(ACTION_FORWARD, speed, duration_ms);
            return true;
        });

    // 2. 后退
    mcp_server.AddTool(
        "self.wheel.move_backward",
        "后退。speed: 速度(0-100); duration_ms: 持续时间(毫秒，0表示持续运动)",
        PropertyList({Property("speed", kPropertyTypeInteger, 50, 0, 100),
                      Property("duration_ms", kPropertyTypeInteger, 0, 0, 60000)}),
        [this](const PropertyList &properties) -> ReturnValue
        {
            int speed = properties["speed"].value<int>();
            int duration_ms = properties["duration_ms"].value<int>();
            QueueAction(ACTION_BACKWARD, speed, duration_ms);
            return true;
        });

    // 3. 左转（差速）
    mcp_server.AddTool(
        "self.wheel.turn_left",
        "左转（差速转弯）。speed: 转弯速度(0-100); duration_ms: 持续时间(毫秒，0表示持续转弯)",
        PropertyList({Property("speed", kPropertyTypeInteger, 50, 0, 100),
                      Property("duration_ms", kPropertyTypeInteger, 0, 0, 60000)}),
        [this](const PropertyList &properties) -> ReturnValue
        {
            int speed = properties["speed"].value<int>();
            int duration_ms = properties["duration_ms"].value<int>();
            QueueAction(ACTION_TURN_LEFT, speed, duration_ms);
            return true;
        });

    // 4. 右转（差速）
    mcp_server.AddTool(
        "self.wheel.turn_right",
        "右转（差速转弯）。speed: 转弯速度(0-100); duration_ms: 持续时间(毫秒，0表示持续转弯)",
        PropertyList({Property("speed", kPropertyTypeInteger, 50, 0, 100),
                      Property("duration_ms", kPropertyTypeInteger, 0, 0, 60000)}),
        [this](const PropertyList &properties) -> ReturnValue
        {
            int speed = properties["speed"].value<int>();
            int duration_ms = properties["duration_ms"].value<int>();
            QueueAction(ACTION_TURN_RIGHT, speed, duration_ms);
            return true;
        });

    // 5. 原地左转
    mcp_server.AddTool(
        "self.wheel.spin_left",
        "原地左转（左轮后退，右轮前进）。speed: 转弯速度(0-100); duration_ms: 持续时间(毫秒)",
        PropertyList({Property("speed", kPropertyTypeInteger, 50, 0, 100),
                      Property("duration_ms", kPropertyTypeInteger, 0, 0, 60000)}),
        [this](const PropertyList &properties) -> ReturnValue
        {
            int speed = properties["speed"].value<int>();
            int duration_ms = properties["duration_ms"].value<int>();
            QueueAction(ACTION_SPIN_LEFT, speed, duration_ms);
            return true;
        });

    // 6. 原地右转
    mcp_server.AddTool(
        "self.wheel.spin_right",
        "原地右转（左轮前进，右轮后退）。speed: 转弯速度(0-100); duration_ms: 持续时间(毫秒)",
        PropertyList({Property("speed", kPropertyTypeInteger, 50, 0, 100),
                      Property("duration_ms", kPropertyTypeInteger, 0, 0, 60000)}),
        [this](const PropertyList &properties) -> ReturnValue
        {
            int speed = properties["speed"].value<int>();
            int duration_ms = properties["duration_ms"].value<int>();
            QueueAction(ACTION_SPIN_RIGHT, speed, duration_ms);
            return true;
        });

    // 7. 停止
    mcp_server.AddTool(
        "self.wheel.stop",
        "立即停止所有运动",
        PropertyList(),
        [this](const PropertyList &properties) -> ReturnValue
        {
            QueueAction(ACTION_STOP);
            return true;
        });

    // 8. 加速
    mcp_server.AddTool(
        "self.wheel.accelerate",
        "平滑加速。target_speed: 目标速度(0-100); duration_ms: 加速时间(毫秒)",
        PropertyList({Property("target_speed", kPropertyTypeInteger, 80, 0, 100),
                      Property("duration_ms", kPropertyTypeInteger, 2000, 100, 10000)}),
        [this](const PropertyList &properties) -> ReturnValue
        {
            int target_speed = properties["target_speed"].value<int>();
            int duration_ms = properties["duration_ms"].value<int>();
            QueueAction(ACTION_ACCELERATE, 0, duration_ms, 0, 0, target_speed);
            return true;
        });

    // 9. 减速
    mcp_server.AddTool(
        "self.wheel.decelerate",
        "平滑减速到停止。duration_ms: 减速时间(毫秒)",
        PropertyList({Property("duration_ms", kPropertyTypeInteger, 1000, 100, 10000)}),
        [this](const PropertyList &properties) -> ReturnValue
        {
            int duration_ms = properties["duration_ms"].value<int>();
            QueueAction(ACTION_DECELERATE, 0, duration_ms);
            return true;
        });

    // 10. 自定义左右轮速度
    mcp_server.AddTool(
        "self.wheel.set_wheel_speeds",
        "独立控制左右轮速度。left_speed: 左轮速度(-100到100); "
        "right_speed: 右轮速度(-100到100); duration_ms: 持续时间(毫秒，0表示持续)",
        PropertyList({Property("left_speed", kPropertyTypeInteger, 0, -100, 100),
                      Property("right_speed", kPropertyTypeInteger, 0, -100, 100),
                      Property("duration_ms", kPropertyTypeInteger, 0, 0, 60000)}),
        [this](const PropertyList &properties) -> ReturnValue
        {
            int left_speed = properties["left_speed"].value<int>();
            int right_speed = properties["right_speed"].value<int>();
            int duration_ms = properties["duration_ms"].value<int>();
            QueueAction(ACTION_CUSTOM_SPEED, 0, duration_ms, left_speed, right_speed, 0);
            return true;
        });

    // 11. 获取状态
    mcp_server.AddTool(
        "self.wheel.get_status",
        "获取机器人运动状态，返回 moving 或 idle，以及左右轮速度",
        PropertyList(),
        [this](const PropertyList &properties) -> ReturnValue
        {
            int left_speed = wheels_.getLeftSpeed();
            int right_speed = wheels_.getRightSpeed();
            std::string status = is_action_in_progress_ ? "moving" : "idle";

            std::string result = "{\"status\":\"" + status +
                                 "\",\"left_speed\":" + std::to_string(left_speed) +
                                 ",\"right_speed\":" + std::to_string(right_speed) + "}";
            return result;
        });

    // 12. 获取速度
    mcp_server.AddTool(
        "self.wheel.get_speeds",
        "获取当前左右轮速度",
        PropertyList(),
        [this](const PropertyList &properties) -> ReturnValue
        {
            int left_speed = wheels_.getLeftSpeed();
            int right_speed = wheels_.getRightSpeed();

            std::string result = "{\"left_speed\":" + std::to_string(left_speed) +
                                 ",\"right_speed\":" + std::to_string(right_speed) + "}";
            return result;
        });

    // 13. 跳舞 - 摇摆舞
    mcp_server.AddTool(
        "self.wheel.dance_shake",
        "跳摇摆舞：快速左右摇摆，充满节奏感",
        PropertyList(),
        [this](const PropertyList &properties) -> ReturnValue
        {
            QueueAction(ACTION_DANCE_SHAKE);
            return "开始跳摇摆舞 🎵";
        });

    // 14. 跳舞 - 旋转舞
    mcp_server.AddTool(
        "self.wheel.dance_spin",
        "跳旋转舞：360度原地旋转，速度先慢后快再慢",
        PropertyList(),
        [this](const PropertyList &properties) -> ReturnValue
        {
            QueueAction(ACTION_DANCE_SPIN);
            return "开始跳旋转舞 🌀";
        });

    // 15. 跳舞 - 波浪舞
    mcp_server.AddTool(
        "self.wheel.dance_wave",
        "跳波浪舞：前后移动时速度呈波浪变化",
        PropertyList(),
        [this](const PropertyList &properties) -> ReturnValue
        {
            QueueAction(ACTION_DANCE_WAVE);
            return "开始跳波浪舞 🌊";
        });

    // 16. 跳舞 - 之字舞
    mcp_server.AddTool(
        "self.wheel.dance_zigzag",
        "跳之字舞：走Z字形路线，充满动感",
        PropertyList(),
        [this](const PropertyList &properties) -> ReturnValue
        {
            QueueAction(ACTION_DANCE_ZIGZAG);
            return "开始跳之字舞 ⚡";
        });

    // 17. 跳舞 - 太空步
    mcp_server.AddTool(
        "self.wheel.dance_moonwalk",
        "跳太空步：模拟Michael Jackson的标志性动作",
        PropertyList(),
        [this](const PropertyList &properties) -> ReturnValue
        {
            QueueAction(ACTION_DANCE_MOONWALK);
            return "开始跳太空步 🌙";
        });

    // 18. 跳舞 - 随机舞蹈
    mcp_server.AddTool(
        "self.wheel.dance_random",
        "跳随机舞蹈：从5种舞蹈中随机选择一种。"
        "可选参数 dance_type: 1=摇摆舞, 2=旋转舞, 3=波浪舞, 4=之字舞, 5=太空步",
        PropertyList({Property("dance_type", kPropertyTypeInteger, 0, 0, 5)}),
        [this](const PropertyList &properties) -> ReturnValue
        {
            int dance_type = properties["dance_type"].value<int>();

            // 如果 dance_type 为 0 或未指定，则随机选择
            if (dance_type == 0)
            {
                dance_type = (esp_random() % 5) + 1;
                ESP_LOGI(TAG, "随机选择舞蹈类型: %d", dance_type);
            }

            std::string dance_name;
            int action_type;

            switch (dance_type)
            {
            case 1:
                action_type = ACTION_DANCE_SHAKE;
                dance_name = "摇摆舞 🎵";
                break;
            case 2:
                action_type = ACTION_DANCE_SPIN;
                dance_name = "旋转舞 🌀";
                break;
            case 3:
                action_type = ACTION_DANCE_WAVE;
                dance_name = "波浪舞 🌊";
                break;
            case 4:
                action_type = ACTION_DANCE_ZIGZAG;
                dance_name = "之字舞 ⚡";
                break;
            case 5:
                action_type = ACTION_DANCE_MOONWALK;
                dance_name = "太空步 🌙";
                break;
            default:
                return "错误：无效的舞蹈类型（应为1-5）";
            }

            QueueAction(action_type);
            return "开始跳" + dance_name;
        });

    // 19. 前进+方向控制（万向移动）
    mcp_server.AddTool(
        "self.wheel.move_forward_direction",
        "前进并支持左右方向控制（万向移动）。"
        "speed: 速度(0-100); "
        "direction: 方向(-100到100, -100=完全左转, 0=直线, 100=完全右转); "
        "duration_ms: 持续时间(毫秒，0表示持续运动)",
        PropertyList(std::vector<Property>{
            Property("speed", kPropertyTypeInteger, 50, 0, 100),
            Property("direction", kPropertyTypeInteger, 0, -100, 100),
            Property("duration_ms", kPropertyTypeInteger, 0, 0, 60000)}),
        [this](const PropertyList &properties) -> ReturnValue
        {
            int speed = properties["speed"].value<int>();
            int direction_int = properties["direction"].value<int>();
            float direction = direction_int / 100.0f; // 转换为 -1.0 到 1.0
            int duration_ms = properties["duration_ms"].value<int>();
            QueueAction(ACTION_FORWARD_DIRECTION, speed, duration_ms, 0, 0, 0, direction);
            return true;
        });

    // 20. 后退+方向控制（万向移动）
    mcp_server.AddTool(
        "self.wheel.move_backward_direction",
        "后退并支持左右方向控制（万向移动）。"
        "speed: 速度(0-100); "
        "direction: 方向(-100到100, -100=完全左转, 0=直线, 100=完全右转); "
        "duration_ms: 持续时间(毫秒，0表示持续运动)",
        PropertyList(std::vector<Property>{
            Property("speed", kPropertyTypeInteger, 50, 0, 100),
            Property("direction", kPropertyTypeInteger, 0, -100, 100),
            Property("duration_ms", kPropertyTypeInteger, 0, 0, 60000)}),
        [this](const PropertyList &properties) -> ReturnValue
        {
            int speed = properties["speed"].value<int>();
            int direction_int = properties["direction"].value<int>();
            float direction = direction_int / 100.0f; // 转换为 -1.0 到 1.0
            int duration_ms = properties["duration_ms"].value<int>();
            QueueAction(ACTION_BACKWARD_DIRECTION, speed, duration_ms, 0, 0, 0, direction);
            return true;
        });

    ESP_LOGI(TAG, "MCP工具注册完成 - 共20个工具（包括2个万向移动功能）");
}

// 全局控制器实例
static WheelRobotController *g_wheel_robot_controller = nullptr;

// 全局初始化函数
void InitializeWheelRobotController()
{
    if (g_wheel_robot_controller == nullptr)
    {
        g_wheel_robot_controller = new WheelRobotController();
        ESP_LOGI(TAG, "全局两轮机器人控制器已创建并初始化");
    }
}

// 获取全局控制器实例
WheelRobotController *GetWheelRobotController()
{
    return g_wheel_robot_controller;
}