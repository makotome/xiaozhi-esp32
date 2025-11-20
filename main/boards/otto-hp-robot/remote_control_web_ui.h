/*
 * remote_control_web_ui.h
 * Otto HP Robot 遥控 Web 界面
 * 嵌入式 HTML/CSS/JavaScript
 */

#ifndef REMOTE_CONTROL_WEB_UI_H
#define REMOTE_CONTROL_WEB_UI_H

// 遥控界面 HTML (压缩版)
const char REMOTE_CONTROL_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>Otto HP Robot 遥控器</title>
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
            min-height: 100vh;
            display: flex;
            flex-direction: column;
            align-items: center;
            padding: 20px;
            color: white;
        }
        .container {
            max-width: 500px;
            width: 100%;
        }
        h1 {
            text-align: center;
            margin-bottom: 10px;
            font-size: 28px;
            text-shadow: 2px 2px 4px rgba(0,0,0,0.3);
        }
        .status-bar {
            background: rgba(255,255,255,0.2);
            border-radius: 15px;
            padding: 15px;
            margin-bottom: 20px;
            backdrop-filter: blur(10px);
            text-align: center;
        }
        .status-indicator {
            display: inline-block;
            width: 12px;
            height: 12px;
            border-radius: 50%;
            background: #4ade80;
            margin-right: 8px;
            animation: pulse 2s infinite;
        }
        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.5; }
        }
        .control-pad {
            background: rgba(255,255,255,0.2);
            border-radius: 20px;
            padding: 30px;
            margin-bottom: 20px;
            backdrop-filter: blur(10px);
        }
        .dpad {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 10px;
            max-width: 300px;
            margin: 0 auto 20px;
        }
        .btn {
            background: rgba(255,255,255,0.3);
            border: 2px solid rgba(255,255,255,0.5);
            border-radius: 15px;
            color: white;
            font-size: 24px;
            font-weight: bold;
            padding: 20px;
            cursor: pointer;
            transition: all 0.2s;
            user-select: none;
        }
        .btn:active {
            background: rgba(255,255,255,0.5);
            transform: scale(0.95);
        }
        .btn:disabled {
            opacity: 0.3;
            cursor: not-allowed;
        }
        .btn-forward { grid-column: 2; }
        .btn-left { grid-column: 1; grid-row: 2; }
        .btn-stop { 
            grid-column: 2; 
            grid-row: 2;
            background: rgba(239, 68, 68, 0.5);
            border-color: rgba(239, 68, 68, 0.7);
        }
        .btn-right { grid-column: 3; grid-row: 2; }
        .btn-backward { grid-column: 2; grid-row: 3; }
        .spin-controls {
            display: flex;
            gap: 10px;
            margin-top: 15px;
        }
        .btn-spin {
            flex: 1;
            padding: 15px;
            font-size: 14px;
        }
        .speed-control {
            margin: 20px 0;
        }
        .speed-label {
            text-align: center;
            margin-bottom: 10px;
            font-size: 16px;
        }
        .slider {
            width: 100%;
            height: 8px;
            border-radius: 5px;
            background: rgba(255,255,255,0.3);
            outline: none;
            -webkit-appearance: none;
        }
        .slider::-webkit-slider-thumb {
            -webkit-appearance: none;
            appearance: none;
            width: 24px;
            height: 24px;
            border-radius: 50%;
            background: white;
            cursor: pointer;
            box-shadow: 0 2px 6px rgba(0,0,0,0.3);
        }
        .slider::-moz-range-thumb {
            width: 24px;
            height: 24px;
            border-radius: 50%;
            background: white;
            cursor: pointer;
            border: none;
            box-shadow: 0 2px 6px rgba(0,0,0,0.3);
        }
        .dance-controls {
            background: rgba(255,255,255,0.2);
            border-radius: 20px;
            padding: 20px;
            backdrop-filter: blur(10px);
        }
        .dance-title {
            text-align: center;
            margin-bottom: 15px;
            font-size: 18px;
            font-weight: bold;
        }
        .dance-buttons {
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 10px;
        }
        .btn-dance {
            padding: 15px;
            font-size: 14px;
        }
        .footer {
            margin-top: 20px;
            text-align: center;
            font-size: 12px;
            opacity: 0.8;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🤖 Otto HP Robot</h1>
        
        <div class="status-bar">
            <span class="status-indicator"></span>
            <span id="status">遥控模式已连接</span>
        </div>
        
        <div class="control-pad">
            <div class="dpad">
                <button class="btn btn-forward" ontouchstart="move('forward')" ontouchend="stop()" onmousedown="move('forward')" onmouseup="stop()">▲</button>
                <button class="btn btn-left" ontouchstart="move('left')" ontouchend="stop()" onmousedown="move('left')" onmouseup="stop()">◄</button>
                <button class="btn btn-stop" ontouchstart="stop()" onclick="stop()">⬛</button>
                <button class="btn btn-right" ontouchstart="move('right')" ontouchend="stop()" onmousedown="move('right')" onmouseup="stop()">►</button>
                <button class="btn btn-backward" ontouchstart="move('backward')" ontouchend="stop()" onmousedown="move('backward')" onmouseup="stop()">▼</button>
            </div>
            
            <div class="spin-controls">
                <button class="btn btn-spin" onclick="spin('left')">⟲ 原地左转</button>
                <button class="btn btn-spin" onclick="spin('right')">⟳ 原地右转</button>
            </div>
            
            <div class="speed-control">
                <div class="speed-label">速度: <span id="speedValue">50</span>%</div>
                <input type="range" min="0" max="100" value="50" class="slider" id="speedSlider" oninput="updateSpeed(this.value)">
            </div>
        </div>
        
        <div class="dance-controls">
            <div class="dance-title">🎵 跳舞动作</div>
            <div class="dance-buttons">
                <button class="btn btn-dance" onclick="dance(1)">摇摆舞</button>
                <button class="btn btn-dance" onclick="dance(2)">旋转舞</button>
                <button class="btn btn-dance" onclick="dance(3)">波浪舞</button>
                <button class="btn btn-dance" onclick="dance(4)">之字舞</button>
                <button class="btn btn-dance" onclick="dance(5)">太空步</button>
                <button class="btn btn-dance" onclick="dance(0)">随机舞</button>
            </div>
        </div>
        
        <div class="footer">
            Otto HP Robot v1.0 | 遥控模式
        </div>
    </div>
    
    <script>
        let currentSpeed = 50;
        let isMoving = false;
        let moveInterval = null;
        
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
        
        function move(direction) {
            if (isMoving) return;
            isMoving = true;
            
            const data = {
                speed: currentSpeed,
                duration_ms: 0  // 持续运动
            };
            
            sendCommand(`move/${direction}`, data);
            document.getElementById('status').textContent = `${getDirectionName(direction)}中...`;
        }
        
        function stop() {
            if (!isMoving) return;
            isMoving = false;
            
            sendCommand('move/stop');
            document.getElementById('status').textContent = '遥控模式已连接';
        }
        
        function spin(direction) {
            const data = {
                speed: currentSpeed,
                duration_ms: 500
            };
            
            sendCommand(`move/spin_${direction}`, data);
            document.getElementById('status').textContent = `原地${direction === 'left' ? '左' : '右'}转`;
            
            setTimeout(() => {
                document.getElementById('status').textContent = '遥控模式已连接';
            }, 600);
        }
        
        function dance(type) {
            const danceNames = ['随机', '摇摆舞', '旋转舞', '波浪舞', '之字舞', '太空步'];
            const danceName = danceNames[type] || '跳舞';
            
            sendCommand('dance', { type: type });
            document.getElementById('status').textContent = `正在跳${danceName}...`;
            
            setTimeout(() => {
                document.getElementById('status').textContent = '遥控模式已连接';
            }, 3000);
        }
        
        function getDirectionName(direction) {
            const names = {
                'forward': '前进',
                'backward': '后退',
                'left': '左转',
                'right': '右转'
            };
            return names[direction] || direction;
        }
        
        // 防止页面滚动
        document.addEventListener('touchmove', function(e) {
            e.preventDefault();
        }, { passive: false });
        
        // 页面加载完成后的初始化
        window.addEventListener('load', function() {
            console.log('Otto HP Robot 遥控器已就绪');
        });
    </script>
</body>
</html>
)rawliteral";

#endif // REMOTE_CONTROL_WEB_UI_H
