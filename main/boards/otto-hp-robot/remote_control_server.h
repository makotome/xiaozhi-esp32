/*
 * remote_control_server.h
 * Otto HP Robot 遥控 Web 服务器
 * 提供 HTTP REST API 用于远程控制机器人
 */

#ifndef REMOTE_CONTROL_SERVER_H
#define REMOTE_CONTROL_SERVER_H

#include <esp_http_server.h>
#include "wheel_robot_controller.h"

// 遥控服务器类
class RemoteControlServer
{
private:
    static RemoteControlServer *instance_;
    httpd_handle_t server_;
    WheelRobotController *wheel_controller_;
    bool is_running_;

    // 私有构造函数 (单例模式)
    RemoteControlServer();

    // HTTP 处理函数
    static esp_err_t HandleRoot(httpd_req_t *req);
    static esp_err_t HandleStatus(httpd_req_t *req);
    static esp_err_t HandleMoveForward(httpd_req_t *req);
    static esp_err_t HandleMoveBackward(httpd_req_t *req);
    static esp_err_t HandleTurnLeft(httpd_req_t *req);
    static esp_err_t HandleTurnRight(httpd_req_t *req);
    static esp_err_t HandleSpinLeft(httpd_req_t *req);
    static esp_err_t HandleSpinRight(httpd_req_t *req);
    static esp_err_t HandleStop(httpd_req_t *req);
    static esp_err_t HandleCustomSpeed(httpd_req_t *req);
    static esp_err_t HandleDance(httpd_req_t *req);
    static esp_err_t HandleMoveForwardDirection(httpd_req_t *req);  // 万向前进
    static esp_err_t HandleMoveBackwardDirection(httpd_req_t *req); // 万向后退

    // 辅助函数
    static esp_err_t SendJsonResponse(httpd_req_t *req, const char *json);
    static esp_err_t SendErrorResponse(httpd_req_t *req, const char *error_msg);
    static bool ParseJsonBody(httpd_req_t *req, char *buffer, size_t buffer_size);

public:
    // 禁止拷贝和赋值
    RemoteControlServer(const RemoteControlServer &) = delete;
    RemoteControlServer &operator=(const RemoteControlServer &) = delete;

    // 获取单例实例
    static RemoteControlServer &GetInstance();

    // 析构函数
    ~RemoteControlServer();

    // 启动服务器
    bool Start();

    // 停止服务器
    void Stop();

    // 检查服务器是否运行
    bool IsRunning() const { return is_running_; }

    // 获取服务器 URL
    const char *GetServerUrl() const;
};

#endif // REMOTE_CONTROL_SERVER_H
