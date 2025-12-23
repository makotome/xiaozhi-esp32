/*
 * remote_control_web_ui.h
 * Cube Robot 方块机器人 遥控 Web 界面
 * 嵌入式 HTML/CSS/JavaScript
 */

#ifndef REMOTE_CONTROL_WEB_UI_H
#define REMOTE_CONTROL_WEB_UI_H

// 遥控界面 HTML (横屏模式 + 圆形摇杆)
const char REMOTE_CONTROL_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <meta name="screen-orientation" content="landscape">
    <meta name="mobile-web-app-capable" content="yes">
    <title>Cube Robot 方块机器人 遥控器</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
            -webkit-tap-highlight-color: transparent;
        }
        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Arial, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            height: 100vh;
            width: 100vw;
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 10px;
            color: white;
            overflow: hidden;
        }
        .container {
            width: 100%;
            height: 100%;
            display: flex;
            gap: 15px;
            align-items: center;
        }
        .header {
            position: absolute;
            top: 10px;
            left: 50%;
            transform: translateX(-50%);
            text-align: center;
            z-index: 100;
        }
        h1 {
            font-size: 20px;
            text-shadow: 2px 2px 4px rgba(0,0,0,0.3);
            margin-bottom: 5px;
        }
        .status-bar {
            background: rgba(255,255,255,0.2);
            border-radius: 10px;
            padding: 8px 15px;
            backdrop-filter: blur(10px);
            display: inline-block;
        }
        .status-indicator {
            display: inline-block;
            width: 8px;
            height: 8px;
            border-radius: 50%;
            background: #4ade80;
            margin-right: 6px;
            animation: pulse 2s infinite;
        }
        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.5; }
        }
        .left-panel {
            flex: 1;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            height: 100%;
        }
        .joystick-container {
            background: rgba(255,255,255,0.2);
            border-radius: 20px;
            padding: 20px;
            backdrop-filter: blur(10px);
            display: flex;
            align-items: center;
            justify-content: center;
        }
        .joystick {
            position: relative;
            width: 200px;
            height: 200px;
            background: rgba(255,255,255,0.2);
            border: 3px solid rgba(255,255,255,0.4);
            border-radius: 50%;
            touch-action: none;
            user-select: none;
        }
        .joystick-stick {
            position: absolute;
            width: 70px;
            height: 70px;
            background: linear-gradient(145deg, rgba(255,255,255,0.9), rgba(255,255,255,0.6));
            border: 3px solid rgba(255,255,255,0.8);
            border-radius: 50%;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%);
            cursor: grab;
            box-shadow: 0 4px 10px rgba(0,0,0,0.3);
            transition: all 0.1s ease-out;
        }
        .joystick-stick:active {
            cursor: grabbing;
            box-shadow: 0 2px 5px rgba(0,0,0,0.4);
        }
        .center-dot {
            position: absolute;
            width: 10px;
            height: 10px;
            background: rgba(0,0,0,0.3);
            border-radius: 50%;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%);
            pointer-events: none;
        }
        .right-panel {
            flex: 1;
            display: flex;
            flex-direction: column;
            gap: 10px;
            height: 100%;
            justify-content: center;
        }
        .control-group {
            background: rgba(255,255,255,0.2);
            border-radius: 15px;
            padding: 15px;
            backdrop-filter: blur(10px);
        }
        .group-title {
            font-size: 14px;
            font-weight: bold;
            margin-bottom: 10px;
            text-align: center;
        }
        .speed-control {
            display: flex;
            align-items: center;
            gap: 10px;
        }
        .speed-label {
            font-size: 14px;
            white-space: nowrap;
            min-width: 80px;
        }
        .slider {
            flex: 1;
            height: 6px;
            border-radius: 3px;
            background: rgba(255,255,255,0.3);
            outline: none;
            -webkit-appearance: none;
        }
        .slider::-webkit-slider-thumb {
            -webkit-appearance: none;
            appearance: none;
            width: 20px;
            height: 20px;
            border-radius: 50%;
            background: white;
            cursor: pointer;
            box-shadow: 0 2px 6px rgba(0,0,0,0.3);
        }
        .slider::-moz-range-thumb {
            width: 20px;
            height: 20px;
            border-radius: 50%;
            background: white;
            cursor: pointer;
            border: none;
            box-shadow: 0 2px 6px rgba(0,0,0,0.3);
        }
        .spin-controls {
            display: flex;
            gap: 8px;
        }
        .btn {
            background: rgba(255,255,255,0.3);
            border: 2px solid rgba(255,255,255,0.5);
            border-radius: 12px;
            color: white;
            font-weight: bold;
            cursor: pointer;
            transition: all 0.2s;
            user-select: none;
        }
        .btn:active {
            background: rgba(255,255,255,0.5);
            transform: scale(0.95);
        }
        .btn-spin {
            flex: 1;
            padding: 12px;
            font-size: 12px;
        }
        .dance-buttons {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 8px;
        }
        .btn-dance {
            padding: 12px 8px;
            font-size: 11px;
        }
        .btn-stop {
            width: 100px;
            height: 100px;
            background: linear-gradient(145deg, #ef4444, #dc2626);
            border: 4px solid rgba(255,255,255,0.8);
            border-radius: 50%;
            color: white;
            font-size: 18px;
            font-weight: bold;
            cursor: pointer;
            box-shadow: 0 4px 15px rgba(239, 68, 68, 0.5);
            transition: all 0.2s;
            user-select: none;
            margin-top: 20px;
        }
        .btn-stop:active {
            background: linear-gradient(145deg, #dc2626, #b91c1c);
            transform: scale(0.95);
            box-shadow: 0 2px 8px rgba(239, 68, 68, 0.6);
        }
        .footer {
            position: absolute;
            bottom: 5px;
            right: 10px;
            font-size: 10px;
            opacity: 0.6;
        }
    </style>
</head>
<body>
    <div class="header">
        <h1>🤖 Cube Robot 方块机器人</h1>
        <div class="status-bar">
            <span class="status-indicator"></span>
            <span id="status">遥控模式已连接</span>
        </div>
    </div>
    
    <div class="container">
        <!-- 左侧: 摇杆控制 -->
        <div class="left-panel">
            <div class="joystick-container">
                <div class="joystick" id="joystick">
                    <div class="center-dot"></div>
                    <div class="joystick-stick" id="stick"></div>
                </div>
            </div>
            <!-- 紧急停止按钮 -->
            <button class="btn-stop" onclick="emergencyStop()">⏹<br>停止</button>
        </div>
        
        <!-- 右侧: 其他控制 -->
        <div class="right-panel">
            <!-- 速度控制 -->
            <div class="control-group">
                <div class="group-title">⚡ 速度控制</div>
                <div class="speed-control">
                    <span class="speed-label">速度: <span id="speedValue">50</span>%</span>
                    <input type="range" min="0" max="100" value="50" class="slider" id="speedSlider" oninput="updateSpeed(this.value)">
                </div>
            </div>
            
            <!-- 原地旋转 -->
            <div class="control-group">
                <div class="group-title">🔄 原地旋转</div>
                <div class="spin-controls">
                    <button class="btn btn-spin" onclick="spin('left')">⟲ 左转</button>
                    <button class="btn btn-spin" onclick="spin('right')">⟳ 右转</button>
                </div>
            </div>
            
            <!-- 跳舞动作 -->
            <div class="control-group">
                <div class="group-title">🎵 跳舞动作</div>
                <div class="dance-buttons">
                    <button class="btn btn-dance" onclick="dance(1)">摇摆</button>
                    <button class="btn btn-dance" onclick="dance(2)">旋转</button>
                    <button class="btn btn-dance" onclick="dance(3)">波浪</button>
                    <button class="btn btn-dance" onclick="dance(4)">之字</button>
                    <button class="btn btn-dance" onclick="dance(5)">太空步</button>
                    <button class="btn btn-dance" onclick="dance(0)">随机</button>
                </div>
            </div>
        </div>
    </div>
    
    <div class="footer">Cube Robot 方块机器人 v1.0</div>
    
    <script>
        let currentSpeed = 50;
        let isMoving = false;
        let joystickActive = false;
        
        const joystick = document.getElementById('joystick');
        const stick = document.getElementById('stick');
        const maxDistance = 65; // 摇杆最大移动距离
        
        // 限流和状态管理
        let lastCommandTime = 0;
        let lastCommand = null;
        let commandThrottle = 200; // 限流: 200ms内只发送一次 (每秒最多5次)
        let pendingRequest = false; // 是否有请求正在处理中
        
        // 摇杆控制
        let currentX = 0;
        let currentY = 0;
        
        function handleJoystickStart(e) {
            e.preventDefault();
            joystickActive = true;
            handleJoystickMove(e);
        }
        
        function handleJoystickMove(e) {
            if (!joystickActive) return;
            
            const touch = e.touches ? e.touches[0] : e;
            const rect = joystick.getBoundingClientRect();
            const centerX = rect.left + rect.width / 2;
            const centerY = rect.top + rect.height / 2;
            
            let deltaX = touch.clientX - centerX;
            let deltaY = touch.clientY - centerY;
            
            // 计算距离和角度
            const distance = Math.sqrt(deltaX * deltaX + deltaY * deltaY);
            
            // 限制在圆形范围内
            if (distance > maxDistance) {
                const angle = Math.atan2(deltaY, deltaX);
                deltaX = Math.cos(angle) * maxDistance;
                deltaY = Math.sin(angle) * maxDistance;
            }
            
            // 更新摇杆位置
            stick.style.transform = `translate(calc(-50% + ${deltaX}px), calc(-50% + ${deltaY}px))`;
            
            currentX = deltaX;
            currentY = deltaY;
            
            // 发送控制命令
            sendJoystickCommand(deltaX, deltaY, distance);
        }
        
        function handleJoystickEnd(e) {
            e.preventDefault();
            joystickActive = false;
            
            // 摇杆回中
            stick.style.transform = 'translate(-50%, -50%)';
            currentX = 0;
            currentY = 0;
            
            // 停止机器人
            stop();
        }
        
        function sendJoystickCommand(x, y, distance) {
            // 调试日志
            console.log(`摇杆: x=${x.toFixed(1)}, y=${y.toFixed(1)}, distance=${distance.toFixed(1)}`);
            
            // 死区处理 (小于15%的移动忽略)
            if (distance < maxDistance * 0.15) {
                if (isMoving) {
                    stop();
                }
                return;
            }
            
            // 限流检查: 距离上次发送不足200ms则跳过
            const now = Date.now();
            if (now - lastCommandTime < commandThrottle) {
                return;
            }
            
            // 请求队列检查: 如果上一个请求还在处理中,跳过
            if (pendingRequest) {
                console.log('上一个请求未完成,跳过本次');
                return;
            }
            
            // 归一化
            const normalizedDistance = Math.min(distance / maxDistance, 1.0);
            
            // 计算速度
            let speed = Math.round(currentSpeed * normalizedDistance);
            
            // 计算方向 (-1.0 到 1.0)
            const directionFloat = Math.max(-1.0, Math.min(1.0, x / maxDistance));
            
            // 判断是前进还是后退 (y轴向上为负)
            // 优先判断前后移动,阈值增大到25像素
            let endpoint = '';
            let directionName = '';
            let commandType = '';
            let direction = 0;
            
            // Y轴判断调试
            console.log(`Y轴判断: y=${y}, 阈值检查: y<-25=${y < -25}, y>25=${y > 25}`);
            
            // Y轴阈值增大,更容易识别为前进/后退
            if (y < -25) { // 向上 = 前进 (阈值从-10改为-25)
                endpoint = 'move/forward_direction';
                commandType = 'forward';
                
                // X轴方向死区: 小于25%的偏移视为直线 (配合后端DIRECTION_FACTOR=0.4)
                // 25%前端死区 + 40%后端系数 = 实际10%转向,更容易直线
                if (Math.abs(directionFloat) < 0.25) {
                    direction = 0; // 直线前进
                    directionName = '前进';
                } else {
                    direction = Math.round(directionFloat * 100);
                    if (directionFloat > 0) {
                        directionName = `前进右转 (${Math.abs(direction)}%)`;
                    } else {
                        directionName = `前进左转 (${Math.abs(direction)}%)`;
                    }
                }
                
            } else if (y > 25) { // 向下 = 后退 (阈值从10改为25)
                endpoint = 'move/backward_direction';
                commandType = 'backward';
                
                console.log(`后退触发: y=${y}, endpoint=${endpoint}`);
                
                // X轴方向死区: 小于25%的偏移视为直线
                if (Math.abs(directionFloat) < 0.25) {
                    direction = 0; // 直线后退
                    directionName = '后退';
                } else {
                    direction = Math.round(directionFloat * 100);
                    if (directionFloat > 0) {
                        directionName = `后退右转 (${Math.abs(direction)}%)`;
                    } else {
                        directionName = `后退左转 (${Math.abs(direction)}%)`;
                    }
                }
                
            } else { // Y轴在-25到25之间: 主要是左右转向
                // 左右转向需要X轴偏移超过20像素
                if (x > 20) {
                    endpoint = 'move/right';
                    commandType = 'turn_right';
                    directionName = '右转';
                    direction = 0; // 原地转向不需要direction参数
                } else if (x < -20) {
                    endpoint = 'move/left';
                    commandType = 'turn_left';
                    directionName = '左转';
                    direction = 0;
                } else {
                    // Y和X都在死区内,停止
                    if (isMoving) {
                        stop();
                    }
                    return;
                }
            }
            
            if (endpoint) {
                const newCommand = {
                    endpoint: endpoint,
                    type: commandType,
                    speed: speed,
                    direction: direction
                };
                
                console.log(`准备发送命令: ${JSON.stringify(newCommand)}`);
                
                // 检查命令是否与上次相同(去重)
                // 方向差异阈值从5增加到10,减少微小变化导致的重复发送
                if (lastCommand && 
                    lastCommand.type === newCommand.type &&
                    lastCommand.speed === newCommand.speed &&
                    Math.abs(lastCommand.direction - newCommand.direction) < 10) {
                    // 命令相同,跳过发送
                    return;
                }
                
                // 如果命令类型改变(前进↔后退↔转向),先停止
                if (lastCommand && lastCommand.type !== newCommand.type) {
                    // 同步发送停止命令并等待
                    pendingRequest = true;
                    sendCommand('move/stop')
                        .then(() => {
                            // 停止命令完成后,发送新命令
                            return sendCommand(endpoint, {
                                speed: speed,
                                direction: direction,
                                duration_ms: 0
                            });
                        })
                        .then(() => {
                            pendingRequest = false;
                            isMoving = true;
                            lastCommand = newCommand;
                            lastCommandTime = now;
                            document.getElementById('status').textContent = `${directionName}中...`;
                        })
                        .catch(e => {
                            console.error('命令发送失败:', e);
                            pendingRequest = false;
                        });
                } else {
                    // 同类型命令,直接发送
                    pendingRequest = true;
                    const data = {
                        speed: speed,
                        direction: direction,
                        duration_ms: 0
                    };
                    
                    sendCommand(endpoint, data)
                        .then(() => {
                            pendingRequest = false;
                            isMoving = true;
                            lastCommand = newCommand;
                            lastCommandTime = now;
                            document.getElementById('status').textContent = `${directionName}中...`;
                        })
                        .catch(e => {
                            console.error('命令发送失败:', e);
                            pendingRequest = false;
                        });
                }
            }
        }
        
        // 添加摇杆事件监听
        stick.addEventListener('touchstart', handleJoystickStart, { passive: false });
        stick.addEventListener('touchmove', handleJoystickMove, { passive: false });
        stick.addEventListener('touchend', handleJoystickEnd, { passive: false });
        stick.addEventListener('mousedown', handleJoystickStart);
        document.addEventListener('mousemove', handleJoystickMove);
        document.addEventListener('mouseup', handleJoystickEnd);
        
        function updateSpeed(value) {
            currentSpeed = parseInt(value);
            document.getElementById('speedValue').textContent = currentSpeed;
        }
        
        async function sendCommand(endpoint, data = {}) {
            try {
                const response = await fetch(`/api/${endpoint}`, {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json'
                    },
                    body: JSON.stringify(data)
                });
                const result = await response.json();
                return result.success;
            } catch (error) {
                console.error('发送命令失败:', error);
                document.getElementById('status').textContent = '连接失败';
                return false;
            }
        }
        
        function stop() {
            if (!isMoving) return;
            isMoving = false;
            lastCommand = null;
            
            sendCommand('move/stop');
            document.getElementById('status').textContent = '遥控模式已连接';
        }
        
        function emergencyStop() {
            // 紧急停止: 强制停止,清除所有状态
            isMoving = false;
            lastCommand = null;
            pendingRequest = false;
            
            // 立即发送停止命令
            fetch('/api/move/stop', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({})
            }).then(() => {
                document.getElementById('status').textContent = '⏹ 已紧急停止';
                setTimeout(() => {
                    document.getElementById('status').textContent = '遥控模式已连接';
                }, 1500);
            }).catch(e => {
                console.error('紧急停止失败:', e);
            });
        }
        
        function spin(direction) {
            const data = {
                speed: currentSpeed,
                duration_ms: 500
            };
            
            sendCommand(`move/spin_${direction}`, data);
            document.getElementById('status').textContent = `原地${direction === 'left' ? '左' : '右'}转`;
            
            setTimeout(() => {
                if (!isMoving) {
                    document.getElementById('status').textContent = '遥控模式已连接';
                }
            }, 600);
        }
        
        function dance(type) {
            const danceNames = ['随机', '摇摆舞', '旋转舞', '波浪舞', '之字舞', '太空步'];
            const danceName = danceNames[type] || '跳舞';
            
            sendCommand('dance', { type: type });
            document.getElementById('status').textContent = `正在跳${danceName}...`;
            
            setTimeout(() => {
                if (!isMoving) {
                    document.getElementById('status').textContent = '遥控模式已连接';
                }
            }, 3000);
        }
        
        // 防止页面滚动
        document.addEventListener('touchmove', function(e) {
            if (e.target.classList.contains('slider')) {
                return; // 允许滑块滚动
            }
            e.preventDefault();
        }, { passive: false });
        
        // 强制横屏提示
        function checkOrientation() {
            if (window.innerHeight > window.innerWidth) {
                // 竖屏状态
                document.getElementById('status').textContent = '请将设备横屏使用';
            }
        }
        
        window.addEventListener('load', function() {
            checkOrientation();
            console.log('Cube Robot 方块机器人 遥控器已就绪 (横屏模式)');
        });
        
        window.addEventListener('resize', checkOrientation);
        window.addEventListener('orientationchange', checkOrientation);
    </script>
</body>
</html>
)rawliteral";

#endif  // REMOTE_CONTROL_WEB_UI_H
