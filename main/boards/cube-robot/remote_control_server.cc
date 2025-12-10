/*
 * remote_control_server.cc
 * Otto HP Robot 遥控 Web 服务器实现
 */

#include "remote_control_server.h"
#include "remote_control_web_ui.h"
#include <esp_log.h>
#include <esp_wifi.h>
#include <mdns.h>
#include <cJSON.h>
#include <string.h>

#define TAG "RemoteControlServer"
#define MDNS_HOSTNAME "cube-robot-ctrl"
#define SERVER_PORT 80
#define MAX_REQUEST_SIZE 512

// 静态成员初始化
RemoteControlServer *RemoteControlServer::instance_ = nullptr;

// 私有构造函数
RemoteControlServer::RemoteControlServer()
    : server_(nullptr),
      wheel_controller_(nullptr),
      is_running_(false)
{
}

// 析构函数
RemoteControlServer::~RemoteControlServer()
{
    Stop();
}

// 获取单例实例
RemoteControlServer &RemoteControlServer::GetInstance()
{
    if (instance_ == nullptr)
    {
        instance_ = new RemoteControlServer();
    }
    return *instance_;
}

// 启动服务器
bool RemoteControlServer::Start()
{
    if (is_running_)
    {
        ESP_LOGW(TAG, "服务器已在运行");
        return true;
    }

    ESP_LOGI(TAG, "启动遥控 Web 服务器...");

    // 获取轮子控制器
    wheel_controller_ = GetWheelRobotController();
    if (wheel_controller_ == nullptr)
    {
        ESP_LOGE(TAG, "无法获取轮子控制器");
        return false;
    }

    // 配置 HTTP 服务器
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = SERVER_PORT;
    config.max_uri_handlers = 16; // 增加到16个,支持所有URI (当前13个)
    config.stack_size = 8192;
    config.task_priority = 5;

    // 启动 HTTP 服务器
    esp_err_t err = httpd_start(&server_, &config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "启动 HTTP 服务器失败: %s", esp_err_to_name(err));
        return false;
    }

    // 注册 URI 处理函数
    httpd_uri_t uri_root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = HandleRoot,
        .user_ctx = this};
    httpd_register_uri_handler(server_, &uri_root);

    httpd_uri_t uri_status = {
        .uri = "/api/status",
        .method = HTTP_GET,
        .handler = HandleStatus,
        .user_ctx = this};
    httpd_register_uri_handler(server_, &uri_status);

    // 先注册带方向的URI,避免被基础URI截获
    httpd_uri_t uri_move_forward_direction = {
        .uri = "/api/move/forward_direction",
        .method = HTTP_POST,
        .handler = HandleMoveForwardDirection,
        .user_ctx = this};
    httpd_register_uri_handler(server_, &uri_move_forward_direction);

    httpd_uri_t uri_move_backward_direction = {
        .uri = "/api/move/backward_direction",
        .method = HTTP_POST,
        .handler = HandleMoveBackwardDirection,
        .user_ctx = this};
    httpd_register_uri_handler(server_, &uri_move_backward_direction);

    // 再注册基础URI
    httpd_uri_t uri_move_forward = {
        .uri = "/api/move/forward",
        .method = HTTP_POST,
        .handler = HandleMoveForward,
        .user_ctx = this};
    httpd_register_uri_handler(server_, &uri_move_forward);

    httpd_uri_t uri_move_backward = {
        .uri = "/api/move/backward",
        .method = HTTP_POST,
        .handler = HandleMoveBackward,
        .user_ctx = this};
    httpd_register_uri_handler(server_, &uri_move_backward);

    httpd_uri_t uri_turn_left = {
        .uri = "/api/move/left",
        .method = HTTP_POST,
        .handler = HandleTurnLeft,
        .user_ctx = this};
    httpd_register_uri_handler(server_, &uri_turn_left);

    httpd_uri_t uri_turn_right = {
        .uri = "/api/move/right",
        .method = HTTP_POST,
        .handler = HandleTurnRight,
        .user_ctx = this};
    httpd_register_uri_handler(server_, &uri_turn_right);

    httpd_uri_t uri_spin_left = {
        .uri = "/api/move/spin_left",
        .method = HTTP_POST,
        .handler = HandleSpinLeft,
        .user_ctx = this};
    httpd_register_uri_handler(server_, &uri_spin_left);

    httpd_uri_t uri_spin_right = {
        .uri = "/api/move/spin_right",
        .method = HTTP_POST,
        .handler = HandleSpinRight,
        .user_ctx = this};
    httpd_register_uri_handler(server_, &uri_spin_right);

    httpd_uri_t uri_stop = {
        .uri = "/api/move/stop",
        .method = HTTP_POST,
        .handler = HandleStop,
        .user_ctx = this};
    httpd_register_uri_handler(server_, &uri_stop);

    httpd_uri_t uri_custom = {
        .uri = "/api/move/custom",
        .method = HTTP_POST,
        .handler = HandleCustomSpeed,
        .user_ctx = this};
    httpd_register_uri_handler(server_, &uri_custom);

    httpd_uri_t uri_dance = {
        .uri = "/api/dance",
        .method = HTTP_POST,
        .handler = HandleDance,
        .user_ctx = this};
    httpd_register_uri_handler(server_, &uri_dance);

    // 初始化并启动 mDNS 服务
    esp_err_t mdns_err = mdns_init();
    if (mdns_err == ESP_OK)
    {
        mdns_hostname_set(MDNS_HOSTNAME);
        mdns_instance_name_set("Cube Robot Remote Control");

        // 注册 HTTP 服务
        mdns_service_add(NULL, "_http", "_tcp", SERVER_PORT, NULL, 0);

        ESP_LOGI(TAG, "mDNS 服务启动成功，主机名: %s.local", MDNS_HOSTNAME);
    }
    else
    {
        ESP_LOGW(TAG, "mDNS 初始化失败: %s", esp_err_to_name(mdns_err));
    }

    is_running_ = true;
    ESP_LOGI(TAG, "遥控 Web 服务器启动成功，访问地址: %s", GetServerUrl());

    return true;
}

// 停止服务器
void RemoteControlServer::Stop()
{
    if (!is_running_)
    {
        return;
    }

    ESP_LOGI(TAG, "停止遥控 Web 服务器...");

    // 停止 mDNS 服务
    mdns_free();

    if (server_ != nullptr)
    {
        httpd_stop(server_);
        server_ = nullptr;
    }

    is_running_ = false;
    ESP_LOGI(TAG, "遥控 Web 服务器已停止");
}

// 获取服务器 URL
const char *RemoteControlServer::GetServerUrl() const
{
    // 返回固定的 mDNS 域名
    return "http://" MDNS_HOSTNAME ".local";
}

// 解析 JSON 请求体
bool RemoteControlServer::ParseJsonBody(httpd_req_t *req, char *buffer, size_t buffer_size)
{
    int total_len = req->content_len;
    int cur_len = 0;
    int received = 0;

    if (total_len >= buffer_size)
    {
        ESP_LOGE(TAG, "请求体过大: %d bytes", total_len);
        return false;
    }

    while (cur_len < total_len)
    {
        received = httpd_req_recv(req, buffer + cur_len, total_len - cur_len);
        if (received <= 0)
        {
            ESP_LOGE(TAG, "接收请求体失败");
            return false;
        }
        cur_len += received;
    }
    buffer[cur_len] = '\0';

    return true;
}

// 发送 JSON 响应
esp_err_t RemoteControlServer::SendJsonResponse(httpd_req_t *req, const char *json)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json, strlen(json));
}

// 发送错误响应
esp_err_t RemoteControlServer::SendErrorResponse(httpd_req_t *req, const char *error_msg)
{
    char json[256];
    snprintf(json, sizeof(json), "{\"success\":false,\"error\":\"%s\"}", error_msg);
    return SendJsonResponse(req, json);
}

// 处理根路径 - 返回控制界面
esp_err_t RemoteControlServer::HandleRoot(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, REMOTE_CONTROL_HTML, strlen(REMOTE_CONTROL_HTML));
}

// 处理状态查询
esp_err_t RemoteControlServer::HandleStatus(httpd_req_t *req)
{
    RemoteControlServer *server = (RemoteControlServer *)req->user_ctx;

    auto &wheels = server->wheel_controller_->GetWheels();
    int left_speed = wheels.getLeftSpeed();
    int right_speed = wheels.getRightSpeed();

    char json[256];
    snprintf(json, sizeof(json),
             "{\"success\":true,\"mode\":\"remote_control\",\"left_speed\":%d,\"right_speed\":%d}",
             left_speed, right_speed);

    return SendJsonResponse(req, json);
}

// 处理前进
esp_err_t RemoteControlServer::HandleMoveForward(httpd_req_t *req)
{
    RemoteControlServer *server = (RemoteControlServer *)req->user_ctx;
    char buffer[MAX_REQUEST_SIZE];

    if (!ParseJsonBody(req, buffer, sizeof(buffer)))
    {
        return SendErrorResponse(req, "解析请求失败");
    }

    cJSON *root = cJSON_Parse(buffer);
    if (root == nullptr)
    {
        return SendErrorResponse(req, "无效的JSON");
    }

    int speed = 50;   // 默认速度
    int duration = 0; // 默认持续运动

    cJSON *speed_item = cJSON_GetObjectItem(root, "speed");
    if (speed_item != nullptr && cJSON_IsNumber(speed_item))
    {
        speed = speed_item->valueint;
    }

    cJSON *duration_item = cJSON_GetObjectItem(root, "duration_ms");
    if (duration_item != nullptr && cJSON_IsNumber(duration_item))
    {
        duration = duration_item->valueint;
    }

    cJSON_Delete(root);

    // 调用控制器
    server->wheel_controller_->GetWheels().moveForward(speed);
    if (duration > 0)
    {
        vTaskDelay(pdMS_TO_TICKS(duration));
        server->wheel_controller_->GetWheels().stopAll();
    }

    ESP_LOGI(TAG, "前进: 速度=%d, 持续=%dms", speed, duration);

    return SendJsonResponse(req, "{\"success\":true}");
}

// 处理后退
esp_err_t RemoteControlServer::HandleMoveBackward(httpd_req_t *req)
{
    RemoteControlServer *server = (RemoteControlServer *)req->user_ctx;
    char buffer[MAX_REQUEST_SIZE];

    if (!ParseJsonBody(req, buffer, sizeof(buffer)))
    {
        return SendErrorResponse(req, "解析请求失败");
    }

    cJSON *root = cJSON_Parse(buffer);
    if (root == nullptr)
    {
        return SendErrorResponse(req, "无效的JSON");
    }

    int speed = 50;
    int duration = 0;

    cJSON *speed_item = cJSON_GetObjectItem(root, "speed");
    if (speed_item != nullptr && cJSON_IsNumber(speed_item))
    {
        speed = speed_item->valueint;
    }

    cJSON *duration_item = cJSON_GetObjectItem(root, "duration_ms");
    if (duration_item != nullptr && cJSON_IsNumber(duration_item))
    {
        duration = duration_item->valueint;
    }

    cJSON_Delete(root);

    server->wheel_controller_->GetWheels().moveBackward(speed);
    if (duration > 0)
    {
        vTaskDelay(pdMS_TO_TICKS(duration));
        server->wheel_controller_->GetWheels().stopAll();
    }

    ESP_LOGI(TAG, "后退: 速度=%d, 持续=%dms", speed, duration);

    return SendJsonResponse(req, "{\"success\":true}");
}

// 处理左转
esp_err_t RemoteControlServer::HandleTurnLeft(httpd_req_t *req)
{
    RemoteControlServer *server = (RemoteControlServer *)req->user_ctx;
    char buffer[MAX_REQUEST_SIZE];

    if (!ParseJsonBody(req, buffer, sizeof(buffer)))
    {
        return SendErrorResponse(req, "解析请求失败");
    }

    cJSON *root = cJSON_Parse(buffer);
    if (root == nullptr)
    {
        return SendErrorResponse(req, "无效的JSON");
    }

    int speed = 50;
    int duration = 0;

    cJSON *speed_item = cJSON_GetObjectItem(root, "speed");
    if (speed_item != nullptr && cJSON_IsNumber(speed_item))
    {
        speed = speed_item->valueint;
    }

    cJSON *duration_item = cJSON_GetObjectItem(root, "duration_ms");
    if (duration_item != nullptr && cJSON_IsNumber(duration_item))
    {
        duration = duration_item->valueint;
    }

    cJSON_Delete(root);

    server->wheel_controller_->GetWheels().turnLeft(speed);
    if (duration > 0)
    {
        vTaskDelay(pdMS_TO_TICKS(duration));
        server->wheel_controller_->GetWheels().stopAll();
    }

    ESP_LOGI(TAG, "左转: 速度=%d, 持续=%dms", speed, duration);

    return SendJsonResponse(req, "{\"success\":true}");
}

// 处理右转
esp_err_t RemoteControlServer::HandleTurnRight(httpd_req_t *req)
{
    RemoteControlServer *server = (RemoteControlServer *)req->user_ctx;
    char buffer[MAX_REQUEST_SIZE];

    if (!ParseJsonBody(req, buffer, sizeof(buffer)))
    {
        return SendErrorResponse(req, "解析请求失败");
    }

    cJSON *root = cJSON_Parse(buffer);
    if (root == nullptr)
    {
        return SendErrorResponse(req, "无效的JSON");
    }

    int speed = 50;
    int duration = 0;

    cJSON *speed_item = cJSON_GetObjectItem(root, "speed");
    if (speed_item != nullptr && cJSON_IsNumber(speed_item))
    {
        speed = speed_item->valueint;
    }

    cJSON *duration_item = cJSON_GetObjectItem(root, "duration_ms");
    if (duration_item != nullptr && cJSON_IsNumber(duration_item))
    {
        duration = duration_item->valueint;
    }

    cJSON_Delete(root);

    server->wheel_controller_->GetWheels().turnRight(speed);
    if (duration > 0)
    {
        vTaskDelay(pdMS_TO_TICKS(duration));
        server->wheel_controller_->GetWheels().stopAll();
    }

    ESP_LOGI(TAG, "右转: 速度=%d, 持续=%dms", speed, duration);

    return SendJsonResponse(req, "{\"success\":true}");
}

// 处理原地左转
esp_err_t RemoteControlServer::HandleSpinLeft(httpd_req_t *req)
{
    RemoteControlServer *server = (RemoteControlServer *)req->user_ctx;
    char buffer[MAX_REQUEST_SIZE];

    if (!ParseJsonBody(req, buffer, sizeof(buffer)))
    {
        return SendErrorResponse(req, "解析请求失败");
    }

    cJSON *root = cJSON_Parse(buffer);
    int speed = 50;
    int duration = 500; // 默认半秒

    if (root != nullptr)
    {
        cJSON *speed_item = cJSON_GetObjectItem(root, "speed");
        if (speed_item != nullptr && cJSON_IsNumber(speed_item))
        {
            speed = speed_item->valueint;
        }

        cJSON *duration_item = cJSON_GetObjectItem(root, "duration_ms");
        if (duration_item != nullptr && cJSON_IsNumber(duration_item))
        {
            duration = duration_item->valueint;
        }

        cJSON_Delete(root);
    }

    server->wheel_controller_->GetWheels().turnLeft(speed);
    vTaskDelay(pdMS_TO_TICKS(duration));
    server->wheel_controller_->GetWheels().stopAll();

    ESP_LOGI(TAG, "原地左转: 速度=%d, 持续=%dms", speed, duration);

    return SendJsonResponse(req, "{\"success\":true}");
}

// 处理原地右转
esp_err_t RemoteControlServer::HandleSpinRight(httpd_req_t *req)
{
    RemoteControlServer *server = (RemoteControlServer *)req->user_ctx;
    char buffer[MAX_REQUEST_SIZE];

    if (!ParseJsonBody(req, buffer, sizeof(buffer)))
    {
        return SendErrorResponse(req, "解析请求失败");
    }

    cJSON *root = cJSON_Parse(buffer);
    int speed = 50;
    int duration = 500;

    if (root != nullptr)
    {
        cJSON *speed_item = cJSON_GetObjectItem(root, "speed");
        if (speed_item != nullptr && cJSON_IsNumber(speed_item))
        {
            speed = speed_item->valueint;
        }

        cJSON *duration_item = cJSON_GetObjectItem(root, "duration_ms");
        if (duration_item != nullptr && cJSON_IsNumber(duration_item))
        {
            duration = duration_item->valueint;
        }

        cJSON_Delete(root);
    }

    server->wheel_controller_->GetWheels().turnRight(speed);
    vTaskDelay(pdMS_TO_TICKS(duration));
    server->wheel_controller_->GetWheels().stopAll();

    ESP_LOGI(TAG, "原地右转: 速度=%d, 持续=%dms", speed, duration);

    return SendJsonResponse(req, "{\"success\":true}");
}

// 处理停止
esp_err_t RemoteControlServer::HandleStop(httpd_req_t *req)
{
    RemoteControlServer *server = (RemoteControlServer *)req->user_ctx;

    server->wheel_controller_->GetWheels().stopAll();

    ESP_LOGI(TAG, "停止");

    return SendJsonResponse(req, "{\"success\":true}");
}

// 处理自定义速度
esp_err_t RemoteControlServer::HandleCustomSpeed(httpd_req_t *req)
{
    RemoteControlServer *server = (RemoteControlServer *)req->user_ctx;
    char buffer[MAX_REQUEST_SIZE];

    if (!ParseJsonBody(req, buffer, sizeof(buffer)))
    {
        return SendErrorResponse(req, "解析请求失败");
    }

    cJSON *root = cJSON_Parse(buffer);
    if (root == nullptr)
    {
        return SendErrorResponse(req, "无效的JSON");
    }

    int left_speed = 0;
    int right_speed = 0;
    int duration = 0;

    cJSON *left_item = cJSON_GetObjectItem(root, "left_speed");
    if (left_item != nullptr && cJSON_IsNumber(left_item))
    {
        left_speed = left_item->valueint;
    }

    cJSON *right_item = cJSON_GetObjectItem(root, "right_speed");
    if (right_item != nullptr && cJSON_IsNumber(right_item))
    {
        right_speed = right_item->valueint;
    }

    cJSON *duration_item = cJSON_GetObjectItem(root, "duration_ms");
    if (duration_item != nullptr && cJSON_IsNumber(duration_item))
    {
        duration = duration_item->valueint;
    }

    cJSON_Delete(root);

    server->wheel_controller_->GetWheels().setWheelSpeeds(left_speed, right_speed);
    if (duration > 0)
    {
        vTaskDelay(pdMS_TO_TICKS(duration));
        server->wheel_controller_->GetWheels().stopAll();
    }

    ESP_LOGI(TAG, "自定义速度: 左=%d, 右=%d, 持续=%dms", left_speed, right_speed, duration);

    return SendJsonResponse(req, "{\"success\":true}");
}

// 处理跳舞
esp_err_t RemoteControlServer::HandleDance(httpd_req_t *req)
{
    RemoteControlServer *server = (RemoteControlServer *)req->user_ctx;
    char buffer[MAX_REQUEST_SIZE];

    if (!ParseJsonBody(req, buffer, sizeof(buffer)))
    {
        return SendErrorResponse(req, "解析请求失败");
    }

    cJSON *root = cJSON_Parse(buffer);
    if (root == nullptr)
    {
        return SendErrorResponse(req, "无效的JSON");
    }

    int dance_type = 1; // 默认摇摆舞

    cJSON *type_item = cJSON_GetObjectItem(root, "type");
    if (type_item != nullptr && cJSON_IsNumber(type_item))
    {
        dance_type = type_item->valueint;
    }

    cJSON_Delete(root);

    // 调用对应的舞蹈
    auto &wheels = server->wheel_controller_->GetWheels();
    switch (dance_type)
    {
    case 1:
        wheels.danceShake();
        break;
    case 2:
        wheels.danceSpin();
        break;
    case 3:
        wheels.danceWave();
        break;
    case 4:
        wheels.danceZigzag();
        break;
    case 5:
        wheels.danceMoonwalk();
        break;
    default:
        return SendErrorResponse(req, "无效的舞蹈类型");
    }

    ESP_LOGI(TAG, "跳舞: 类型=%d", dance_type);

    return SendJsonResponse(req, "{\"success\":true}");
}

// 处理万向前进
esp_err_t RemoteControlServer::HandleMoveForwardDirection(httpd_req_t *req)
{
    RemoteControlServer *server = (RemoteControlServer *)req->user_ctx;
    char buffer[MAX_REQUEST_SIZE];

    if (!ParseJsonBody(req, buffer, sizeof(buffer)))
    {
        return SendErrorResponse(req, "解析请求失败");
    }

    cJSON *root = cJSON_Parse(buffer);
    if (root == nullptr)
    {
        return SendErrorResponse(req, "无效的JSON");
    }

    int speed = 50;
    float direction = 0.0f;
    int duration = 0;

    cJSON *speed_item = cJSON_GetObjectItem(root, "speed");
    if (speed_item != nullptr && cJSON_IsNumber(speed_item))
    {
        speed = speed_item->valueint;
    }

    cJSON *direction_item = cJSON_GetObjectItem(root, "direction");
    if (direction_item != nullptr && cJSON_IsNumber(direction_item))
    {
        // 前端发送的是整数-100到100,需要转换为浮点数-1.0到1.0
        int direction_int = direction_item->valueint;
        direction = static_cast<float>(direction_int) / 100.0f;
    }

    cJSON *duration_item = cJSON_GetObjectItem(root, "duration_ms");
    if (duration_item != nullptr && cJSON_IsNumber(duration_item))
    {
        duration = duration_item->valueint;
    }

    cJSON_Delete(root);

    // 调用万向前进 (立即执行,不等待duration)
    server->wheel_controller_->GetWheels().moveForwardWithDirection(speed, direction);

    ESP_LOGI(TAG, "万向前进: 速度=%d, 方向整数=%d, 方向浮点=%.2f", speed, static_cast<int>(direction * 100), direction);

    return SendJsonResponse(req, "{\"success\":true}");
}

// 处理万向后退
esp_err_t RemoteControlServer::HandleMoveBackwardDirection(httpd_req_t *req)
{
    RemoteControlServer *server = (RemoteControlServer *)req->user_ctx;
    char buffer[MAX_REQUEST_SIZE];

    if (!ParseJsonBody(req, buffer, sizeof(buffer)))
    {
        return SendErrorResponse(req, "解析请求失败");
    }

    cJSON *root = cJSON_Parse(buffer);
    if (root == nullptr)
    {
        return SendErrorResponse(req, "无效的JSON");
    }

    int speed = 50;
    float direction = 0.0f;
    int duration = 0;

    cJSON *speed_item = cJSON_GetObjectItem(root, "speed");
    if (speed_item != nullptr && cJSON_IsNumber(speed_item))
    {
        speed = speed_item->valueint;
    }

    cJSON *direction_item = cJSON_GetObjectItem(root, "direction");
    if (direction_item != nullptr && cJSON_IsNumber(direction_item))
    {
        // 前端发送的是整数-100到100,需要转换为浮点数-1.0到1.0
        int direction_int = direction_item->valueint;
        direction = static_cast<float>(direction_int) / 100.0f;
    }

    cJSON *duration_item = cJSON_GetObjectItem(root, "duration_ms");
    if (duration_item != nullptr && cJSON_IsNumber(duration_item))
    {
        duration = duration_item->valueint;
    }

    cJSON_Delete(root);

    // 调用万向后退 (立即执行,不等待duration)
    server->wheel_controller_->GetWheels().moveBackwardWithDirection(speed, direction);

    ESP_LOGI(TAG, "万向后退: 速度=%d, 方向整数=%d, 方向浮点=%.2f", speed, static_cast<int>(direction * 100), direction);

    return SendJsonResponse(req, "{\"success\":true}");
}
