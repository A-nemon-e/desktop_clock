# 桌面时钟 — 基于 ESP32-S3 的 LED 矩阵时钟

一款基于 ESP32-S3 的独立桌面时钟，配备 48×16 白色 LED 矩阵显示屏、WiFi 连接能力以及全栈管理后端。

## 硬件

| 组件 | 详情 |
|---|---|
| MCU | ESP32-S3 N16R8（16MB Flash，8MB PSRAM） |
| 显示屏 | 48×16 白色 LED 矩阵（共 768 个 LED） |
| LED 驱动 | 6× IS31FL3729（每个 8×16） |
| I2C 多路复用器 | TCA9548A（8 通道，使用通道 0-5） |
| 传感器 | AHT21 温度 + 湿度 |
| 输入 | 轻触按键（单击/双击/长按） |
| 供电 | USB 5V |

**I2C 总线布局：**
```
ESP32-S3 I2C0
  └── TCA9548A (地址 0x70)
        ├── CH0: IS31FL3729 #0 (列  0-15)
        ├── CH1: IS31FL3729 #1 (列 16-31)
        ├── CH2: IS31FL3729 #2 (列 32-47)
        ├── CH3: IS31FL3729 #3 (列  0-15, 行偏移 8)
        ├── CH4: IS31FL3729 #4 (列 16-31, 行偏移 8)
        └── CH5: IS31FL3729 #5 (列 32-47, 行偏移 8)
```

## 功能特性

- **显示引擎**：30fps 渲染，7 种背景特效 + 5 种前景叠加
- **卡片系统**：最多 9 张可配置显示"卡片"，双击循环切换
- **WiFi**：STA 模式，凭据持久存储，连接反复失败时自动切换至 AP 模式
- **地理位置 + NTP**：IP 地理定位 → 时区 → NTP 同步，具备双 API 容错
- **MQTT**：实时配置推送，天气数据下发，OTA 通知
- **胖/瘦模式**：MQTT 连接失败时自动切换（连续 3 次失败 → 瘦模式 HTTP）
- **OTA**：OTA 固件升级，双槽位回滚
- **天气**：7 天天气预报及当前天气状况显示
- **亮度控制**：全局电流控制（0-127），长按按键循环调节
- **管理面板**：Vue3 + Element Plus 管理界面

## 显示特效

### 背景（7 种）
| 特效 | 说明 |
|---|---|
| Fire（火焰） | Doom 风格的火焰粒子模拟 |
| Code Rain（代码雨） | 黑客帝国风格的下落字符 |
| Water Ripple（水波纹） | 2D 波传播 |
| Game of Life（生命游戏） | 康威生命游戏 |
| Hourglass（沙漏） | 动画沙漏计时器 |
| Pong Clock（乒乓时钟） | 自动对战的 Pong 游戏显示时钟数字 |
| Weather（天气） | 太阳/云/雨/雪/风可视化 |

### 前景（5 种）
| 叠加层 | 示例 |
|---|---|
| 时钟 时分秒 | `12:34:56` |
| 时钟 时分 | `12:34` |
| 日期 + 星期 | `2026 May 11` / `Monday` |
| 紧凑日期 | `May 11 MON` |
| 温度 + 湿度 | `Hum: 65%` / `Temp: 23.5 C` |

### 字体（4 种）
`3x5` · `5x5` · `7x9` · `long`（时钟数字优化，约 2-3×11-14px）

## 项目结构

```
desktop-clock/
├── firmware/                    # ESP-IDF v5.5 固件
│   ├── main/                    # 应用入口点
│   │   ├── app_main.c           # 主任务编排
│   │   ├── task_prio.h          # FreeRTOS 优先级定义
│   │   ├── i2c_scan.c           # I2C 总线扫描器
│   │   └── gpio_test.c          # GPIO 验证工具
│   ├── components/
│   │   ├── shared/              # 跨组件类型 + 错误码
│   │   ├── config/              # 引脚映射、功率限制、NVS 配置同步
│   │   ├── log/                 # 仅头文件日志（3 字母标签）
│   │   ├── display/             # 显示引擎、字体、特效
│   │   ├── i2c_mux/             # TCA9548A 驱动
│   │   ├── led_matrix/          # IS31FL3729 驱动
│   │   ├── sensors/             # AHT21 驱动
│   │   ├── button/              # 按键消抖 + 手势检测
│   │   ├── wifi_mgr/            # WiFi STA/AP 管理器
│   │   ├── http_client/         # HTTP 客户端（地理位置、瘦模式）
│   │   ├── ntp/                 # NTP 时间同步
│   │   ├── geo/                 # IP 地理定位流水线
│   │   ├── mqtt_client/         # 带 LWT + 重连的 MQTT
│   │   ├── cards/               # 卡片轮转管理器
│   │   ├── thin_mode/           # 瘦模式 HTTP 处理器
│   │   ├── ota/                 # OTA 升级处理器
│   │   └── version/             # 构建版本戳
│   ├── tests/
│   │   └── stability_monitor.py # 24 小时以上稳定性测试框架
│   ├── partitions.csv           # 分区表（factory + 2 个 OTA 槽位）
│   └── sdkconfig.defaults       # ESP-IDF Kconfig 默认配置
│
├── backend/                     # FastAPI + PostgreSQL
│   ├── app/
│   │   ├── main.py              # FastAPI 入口 + 中间件配置
│   │   ├── config.py            # Pydantic 设置
│   │   ├── database.py          # 异步 SQLAlchemy 引擎
│   │   ├── models/              # SQLAlchemy ORM 模型
│   │   ├── schemas/             # Pydantic 请求/响应模式
│   │   ├── routers/             # API 路由处理器
│   │   ├── services/            # 业务逻辑（MQTT 桥接、天气、OTA）
│   │   └── middleware/          # 认证中间件（session + X-Device-MAC）
│   ├── alembic/                 # 数据库迁移
│   ├── mosquitto/               # Mosquitto MQTT Broker 配置
│   ├── docker-compose.yml       # Postgres + Mosquitto + 后端
│   ├── Dockerfile
│   └── requirements.txt
│
├── frontend/                    # Vue3 + Element Plus
│   ├── src/
│   │   ├── views/               # 页面组件
│   │   ├── components/          # UI 组件（卡片编辑器等）
│   │   ├── api.ts               # Axios API 客户端
│   │   ├── router.ts            # Vue Router 配置
│   │   └── types.ts             # TypeScript 接口
│   ├── package.json
│   └── vite.config.ts
│
└── tests/                       # 后端集成测试
    ├── integration/
    │   └── test_e2e.py          # 端到端 API 测试
    └── test_error_scenarios.py  # 错误 + 约束验证测试
```

## 快速开始

### 前置条件
- Python 3.11+ · Node.js 20+ · Docker + Docker Compose
- ESP-IDF v5.5（仅构建固件时需要）
- USB 转串口适配器（用于烧录）

### 1. 启动后端服务

```bash
cd backend
cp .env.example .env       # 编辑 SECRET_KEY 和 QWEATHER_API_KEY
docker compose up -d
```

这会启动 PostgreSQL 15、Mosquitto MQTT Broker 以及端口 8000 上的 FastAPI 后端。

### ESP-IDF 环境设置

```bash
# 1. 安装 ESP-IDF v5.5
git clone -b v5.5 --recursive https://github.com/espressif/esp-idf.git ~/esp-idf-v5.5
cd ~/esp-idf-v5.5
./install.sh esp32s3                # 安装 ESP32-S3 工具链
source export.sh                    # 激活环境（可添加到 ~/.bashrc）

# 2. 验证安装
idf.py --version                    # 应显示 v5.5.x

# 3. 配置项目
cd firmware
idf.py set-target esp32s3           # 一次性目标选择
idf.py menuconfig                    # 确认：PSRAM 已启用，分区表 = custom
idf.py build                         # 编译（首次构建约需 30 秒）
```

```bash
cd frontend
npm install
npm run dev                # Vite 开发服务器，端口 5173
```

### 3. 烧录固件

```bash
cd firmware
idf.py set-target esp32s3
idf.py menuconfig          # 设置串口 + PSRAM
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

首次启动时，设备进入 AP 模式（`DesktopClock_XXXX`）。连接后通过强制门户配置 WiFi 凭据。

### 4. 访问管理面板

1. 在浏览器中打开 `http://localhost:5173`
2. 使用默认管理员凭据登录（`admin` / `admin123`）
3. 通过 MAC 地址注册你的设备
4. 创建和编辑显示卡片

## 后端 API 接口

### 认证
| 方法 | 路径 | 说明 |
|---|---|---|
| POST | `/api/auth/register` | 注册新用户 |
| POST | `/api/auth/login` | 登录（基于 session） |
| POST | `/api/auth/logout` | 登出 |
| GET | `/api/auth/me` | 获取当前用户 |

### 用户（需要 Session）
| 方法 | 路径 | 说明 |
|---|---|---|
| GET | `/api/user/profile` | 用户资料 |
| GET | `/api/user/devices` | 列出已绑定设备 |
| POST | `/api/user/devices/bind` | 绑定设备到用户 |
| POST | `/api/user/devices/unbind` | 解绑设备 |
| GET | `/api/user/devices/{mac}/cards` | 列出设备卡片 |
| POST | `/api/user/devices/{mac}/cards` | 创建卡片 |
| PUT | `/api/user/devices/{mac}/cards/{id}` | 更新卡片 |
| DELETE | `/api/user/devices/{mac}/cards/{id}` | 删除卡片 |

### 设备（需要 X-Device-MAC 请求头）
| 方法 | 路径 | 说明 |
|---|---|---|
| POST | `/api/device/register` | 注册/刷新设备 |
| GET | `/api/device/{mac}` | 获取设备信息 |
| PATCH | `/api/device/{mac}` | 更新设备字段 |
| POST | `/api/device/status` | 设备状态上报 |
| GET | `/api/device/config_sync` | 同步本地配置 |
| POST | `/api/device/ota_status` | 上报 OTA 结果 |

### 管理员（需要 Session + is_admin）
| 方法 | 路径 | 说明 |
|---|---|---|
| GET | `/api/admin/users` | 列出所有用户 |
| POST | `/api/admin/users` | 创建用户 |
| DELETE | `/api/admin/users/{id}` | 删除用户 |
| GET | `/api/admin/devices` | 列出所有设备 |
| GET | `/api/admin/devices/{mac}` | 获取设备详情 |
| DELETE | `/api/admin/devices/{mac}` | 删除设备及卡片 |
| POST | `/api/admin/bind` | 管理员强制绑定设备 |
| PUT | `/api/admin/backend-urls` | 设置后端 URL |
| POST | `/api/admin/firmware/upload` | 上传固件 |

### 天气
| 方法 | 路径 | 说明 |
|---|---|---|
| POST | `/api/weather/query` | 查询天气（设备） |
| POST | `/api/weather/push` | 推送天气到设备 |
| GET | `/api/weather/current/{mac}` | 获取缓存的天气数据 |

### 健康检查
| 方法 | 路径 | 说明 |
|---|---|---|
| GET | `/api/health` | 健康检查 |

## MQTT 主题约定

| 主题 | 方向 | 说明 |
|---|---|---|
| `clock/{mac}/status` | 设备 → 服务器 | 在线/离线 + 设备信息 |
| `clock/{mac}/config` | 服务器 → 设备 | 卡片配置推送（带版本号） |
| `clock/{mac}/config_report` | 设备 → 服务器 | 本地配置上报 |
| `clock/{mac}/ota` | 服务器 → 设备 | OTA 升级 URL + 版本号 |
| `clock/{mac}/ota_status` | 设备 → 服务器 | OTA 执行结果 |
| `clock/{mac}/weather` | 服务器 → 设备 | 天气数据推送 |

## 卡片配置 JSON

```json
{
  "cards": [
    {
      "id": "uuid",
      "device_mac": "aa:bb:cc:dd:ee:ff",
      "position": 0,
      "bg_type": "code_rain",
      "fg_config": {
        "fgs": [
          {"type": 0, "font": "5x5", "dur": 5000},
          {"type": 4, "font": "3x5", "dur": 3000}
        ]
      },
      "duration": 30,
      "rel_brightness": 80
    }
  ],
  "version": 1715432123
}
```

### 验证规则
- 每设备最多 9 张卡片
- `water_ripple`、`game_of_life`、`hourglass` 背景禁止添加前景元素
- `fire` 背景限制前景字体只能使用 `3x5`
- 有效字体大小：`3x5`、`5x5`、`7x9`、`long`
- 每张卡片最多 5 个前景槽位

## 按键行为

| 手势 | 操作 |
|---|---|
| 单击 | 切换显示开/关 |
| 双击 | 切换到下一张卡片（循环轮转） |
| 长按 | 调节亮度（循环 0→127，松手时反向） |

## 测试

### 后端集成测试

```bash
cd backend
pip install httpx pytest pytest-asyncio
pytest ../tests/ -v
```

### 稳定性监控（24 小时以上）

```bash
pip install paho-mqtt
python firmware/tests/stability_monitor.py \
  --broker localhost \
  --mac aa:bb:cc:dd:ee:ff \
  --duration 86400 \
  --interval 60
```

监控空闲堆内存、FPS、重连次数及连接状态。输出 CSV 日志和 JSON 报告到 `firmware/tests/stability_logs/`。

### 固件单元测试

```bash
cd firmware
idf.py build
# Unity 测试通过 idf.py monitor 在设备上运行
```

## 配置

### 后端环境变量

| 变量 | 默认值 | 说明 |
|---|---|---|
| `DATABASE_URL` | `postgresql+asyncpg://...` | PostgreSQL 连接字符串 |
| `SECRET_KEY` | （必填） | Session 加密密钥 |
| `SESSION_EXPIRE_HOURS` | `24` | Session 有效期 |
| `QWEATHER_API_KEY` | （可选） | 和风天气 API 密钥 |
| `MQTT_BROKER` | `localhost` | MQTT Broker 主机 |
| `MQTT_PORT` | `1883` | MQTT Broker 端口 |
| `ADMIN_DEFAULT_USERNAME` | `admin` | 默认管理员用户名 |
| `ADMIN_DEFAULT_PASSWORD` | `admin123` | 默认管理员密码 |

### 固件 SDK 配置

关键 ESP-IDF 设置（详见 `firmware/sdkconfig.defaults`）：
- PSRAM 已启用，`SPIRAM_MALLOC_ALWAYSINTERNAL=16384`
- 自定义分区表，双 OTA 槽位
- WiFi STA + SoftAP
- OTA over HTTP 已启用
- MQTT 3.1.1 协议
- mbedTLS 证书包
- Bootloader 回滚支持

## OTA 升级流程

1. 管理员通过 `/api/admin/firmware/upload` 上传固件
2. 服务器将 OTA URL 发布到 `clock/{mac}/ota`
3. 设备将固件下载到非活跃 OTA 分区
4. 设备验证 SHA-256 校验和
5. 设备将结果上报至 `/api/device/ota_status`
6. 下次启动时，bootloader 验证新槽位
7. 若验证失败 → 自动回滚至上一槽位

## 已知限制

- **需要 ESP-IDF v5.5**：固件编译要求 ESP-IDF v5.5 工具链。在 `firmware/` 目录下执行 `idf.py set-target esp32s3 && idf.py build`。
- **`TIMEZONEDB_API_KEY`**：时区查询需要从 [timezonedb.com](https://timezonedb.com) 获取免费 API 密钥。替换 `firmware/components/geo/geo_pipeline.c:10` 中的 `YOUR_KEY`。
- **`QWEATHER_API_KEY`**：天气显示需要从 [和风天气](https://dev.qweather.com) 获取免费 API 密钥。在 `backend/.env` 中配置 `QWEATHER_API_KEY`。
- **`MAX_GCC_VALUE`**：安全的 LED 亮度限制需要硬件实测。运行功耗预算测量（Wave 0.1）以设置 `power_limit.h` 中的正确值。
- **音频（Phase 2）**：实时音频接收计划在后续阶段实现。音频任务清理的 OTA 钩子已就位。
- **后端需要 PostgreSQL + Mosquitto**：在 `backend/` 目录下使用 `docker-compose up -d` 启动依赖服务。
- **ESP-IDF 构建未验证**：固件尚未编译 — 当前环境缺少 ESP-IDF 工具链。前端构建已验证（`npm run build` 通过，0 错误）。

## 测试覆盖

| 组件 | 测试数 | 文件 |
|-----------|-------|------|
| framebuffer | 4 | `firmware/components/display/test/test_framebuffer.c` |
| IS31FL3729 | 16 | `firmware/components/led_matrix/test/test_is31fl3729.c` |
| card_mgr | 10 | `firmware/components/cards/test/test_card_mgr.c` |
| font_renderer | 6 | `firmware/components/display/test/test_font_renderer.c` |
| geo_pipeline | 4 | `firmware/components/geo/test/test_geo_pipeline.c` |
| config_sync | 5 | `firmware/components/config/test/test_config_sync.c` |
| 后端错误场景 | 20 | `tests/test_error_scenarios.py` |
| 后端 E2E | 6 | `tests/integration/test_e2e.py` |
| 后端 OTA | 4 | `tests/test_firmware_ota.py` |
| **总计** | **75** | **9 个测试文件** |

## 架构说明

- **显示优先**：显示刷新任务运行在 FreeRTOS 优先级 5（最高）。网络、I2C 传感器和 MQTT 操作永不阻塞显示。
- **无 RTC 电池**：每次上电均执行完整的 WiFi → IP 地理定位 → 时区 → NTP 流程（约 30 秒）。
- **TCA9548A RESET**：硬件上拉 — 固件中无需 GPIO 控制。
- **功率限制**：`power_limit.h` 中的 `MAX_GCC_VALUE` 限制 LED 亮度以防止 USB 过流。

## 许可证

MIT
