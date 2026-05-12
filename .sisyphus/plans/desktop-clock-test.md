# Desktop Clock — Comprehensive Test Plan

## TL;DR
> 为 25 个固件组件 + 6 个后端路由 + 6 个前端组件补齐测试。当前 6/25 固件组件有测试，后端部分覆盖，前端零测试。

---

## Phase 1: 固件单元测试（补 19 个缺失）

所有测试使用 Unity 框架。可全并行执行。

### Wave 1a — 硬件驱动测试 (HIGH, 3 tasks)

- [x] **T1.1** tca9548a 测试
  创建 `firmware/components/i2c_mux/test/test_tca9548a.c`
  - test_select_channel: 验证控制字节 = (1<<ch)
  - test_disable_all: 验证控制字节 = 0x00
  - test_invalid_channel_rejected: ch=8 → 返回错误

- [x] **T1.2** aht21 测试
  创建 `firmware/components/sensors/test/test_aht21.c`
  - test_init_sequence: mock I2C → 验证软复位(0xBA) → 校准(0xE1/0x08/0x00)
  - test_parse_humidity: 给定 raw bytes → humidity 在 0-100%
  - test_parse_temperature: 给定 raw bytes → temperature 在 -40~85°C
  - test_measurement_busy_rejected: status bit7=1 → 返回错误

- [x] **T1.3** button 测试
  创建 `firmware/components/button/test/test_button.c`
  - test_single_click_detected: press <300ms → BTN_EVENT_SINGLE_CLICK
  - test_double_click_detected: 2 presses <500ms apart → BTN_EVENT_DOUBLE_CLICK
  - test_long_press_detected: press >800ms → BTN_EVENT_LONG_PRESS_START
  - test_debounce: 快速 bounce → 只触发一次 press

### Wave 1b — 显示效果测试 (MEDIUM, 8 tasks)

- [x] **T1.4** display_task 时序测试
  创建 `firmware/components/display/test/test_display_task.c`
  - test_frame_period: 2次 swap 间隔 ≈ 33ms
  - test_mux_iteration: 验证 6 个通道都被访问
  - test_pwm_data_mapping: 验证 framebuffer pixel → pwm_data 映射

- [x] **T1.5** bg_fire 算法测试
  创建 `firmware/components/display/test/test_bg_fire.c`
  - test_propagation_upward: 底部热→顶部逐渐传热
  - test_decay: 验证火焰逐渐冷却
  - test_all_pixels_in_range: 所有值 [0,255]

- [x] **T1.6** bg_code_rain 测试
  - test_mask_blocks_render: mask 区域像素全为 0
  - test_drops_fall: 头部 row 逐帧增加
  - test_reset_on_overflow: 超出屏幕 → 回到顶部

- [x] **T1.7** bg_water 测试
  - test_ripple_propagates: 扰动点 → 周围像素高度变化
  - test_damping: 无新扰动时高度趋于 0

- [x] **T1.8** bg_life 测试
  - test_still_life_block: 2×2 方块 → 稳定不灭
  - test_blinker_oscillates: 3 格竖线 → 横竖交替
  - test_death_by_isolation: 单格 → 下一代死亡

- [x] **T1.9** bg_pong 测试
  - test_ball_moves: 球每帧位置变化
  - test_wall_bounce: 球碰上下壁反弹
  - test_paddle_bounce: 球碰球拍反弹
  - test_miss_reset: 球出界 → 位置重置

- [x] **T1.10** bg_weather 测试
  - test_sunny_icon_drawn: wc=SUNNY → 非空像素存在
  - test_all_conditions_handled: 每种 wc 都不崩溃

- [x] **T1.11** fg_render 格式测试
  创建 `firmware/components/display/test/test_fg_render.c`
  - test_clock_hms_format: 输出 "12:34:56"
  - test_clock_hm_format: 输出 "12:34"
  - test_date_full_format: 两行 "YYYY MM DD" + "Dayname"
  - test_date_compact_format: 一行 "MM DD DAY"
  - test_temp_humid_format: "Hum：XX%" / "Temp： XX.X C" (中文冒号+空格)

### Wave 1c — 网络栈测试 (MEDIUM, 5 tasks)

- [x] **T1.12** wifi_mgr 状态机测试
  创建 `firmware/components/wifi_mgr/test/test_wifi_mgr.c`
  - test_no_creds_goes_ap: NVS 空 → WIFI_STATE_AP_MODE
  - test_3_failures_max: 重试 3 次后不再重试
  - test_save_creds_writes_nvs: nvs_set_str 被调用

- [x] **T1.13** http_client 测试
  - test_get_json_parses: mock 200 + JSON → 正确解析
  - test_backup_on_primary_fail: 主 404 → 备 URL 被调用
  - test_timeout: 超时 → 返回错误

- [x] **T1.14** ntp_client 测试
  - test_primary_first: 先尝试 aliyun
  - test_backup_on_fail: aliyun 失败 → pool.ntp.org

- [x] **T1.15** mqtt_client 测试
  - test_lwt_set: 连接时 LWT topic 设置
  - test_3_fails_triggers_thin: fail_count≥3 → thin mode
  - test_reconnect_schedule: 断开后启动重连定时器

- [x] **T1.16** ota_handler 测试
  - test_sha256_verification: 正确 hash → 通过
  - test_sha256_mismatch: 错误 hash → 拒绝
  - test_rollback_check: PENDING_VERIFY → mark_valid

---

## Phase 2: 后端测试（让 20 个 skip → pass）

目标：不用 Docker 也能跑所有测试。

- [x] **T2.1** 替换 PostgreSQL 为 SQLite 内存测试
  修改 `backend/app/config.py` 添加测试模式：
  ```python
  # When TESTING=1, use sqlite+aiosqlite for tests
  ```
  安装 `aiosqlite` 依赖。所有 20 个 skip 的测试应在 SQLite 上通过。

- [x] **T2.2** Mock MQTT bridge
  测试中不连接真实 Mosquitto，使用 `unittest.mock.patch` 替换 `mqtt_bridge`

- [x] **T2.3** 补齐缺失的 API 测试
  - `test_admin_create_user`: POST /api/admin/users → 201
  - `test_admin_delete_user`: DELETE → 204
  - `test_admin_get_stats`: GET /api/admin/stats → {total_devices,online_devices}
  - `test_user_get_profile`: GET /api/user/profile → 200
  - `test_weather_query`: GET /api/weather/{mac} → 200
  - `test_weather_push`: 模拟 MQTT weather 消息

---

## Phase 3: 前端测试（从零搭建）

- [x] **T3.1** 安装 Vitest + Vue Test Utils
  ```bash
  cd frontend && npm install -D vitest @vue/test-utils jsdom
  ```
  创建 `frontend/vitest.config.ts` 和 `frontend/src/__tests__/` 目录

- [x] **T3.2** CardEditor 约束逻辑测试
  创建 `frontend/src/__tests__/CardEditor.spec.ts`
  - test_fire_rejects_5x5: 选火焰背景 → 字体选项仅显示 3x5
  - test_water_disables_foreground: 选水波纹 → 前景面板灰显
  - test_max_9_cards: 第 10 张卡片被拒绝
  - test_save_pushes_to_device: 保存按钮 → API 调用

- [x] **T3.3** 管理面板测试
  创建 `frontend/src/__tests__/AdminDevices.spec.ts`
  - test_device_list_renders: 表格渲染设备数据
  - test_filter_by_hw_rev: hw_rev 过滤生效
  - test_ota_button_triggers: OTA 按钮 → 确认弹窗

- [x] **T3.4** 登录表单测试
  创建 `frontend/src/__tests__/Login.spec.ts`
  - test_empty_fields_rejected: 空用户名 → 错误提示
  - test_successful_login_redirects: 正确凭证 → 跳转到 /devices

---

## Phase 4: 硬件集成测试

- [ ] **T4.1** 创建单文件硬件测试固件
  `firmware/main/hw_test_all.c` — 7 步顺序测试：
  1. LED 闪烁 → 肉眼确认 MCU 运行
  2. SDB 拉高/拉低 → I2C 扫描对比
  3. I2C 地址映射 → 8 设备全部可见
  4. LED 矩阵 → 全亮/棋盘/渐变图案
  5. AHT21 → 温湿度读数
  6. 字体渲染 → "88:88" 三种字体
  7. 按键 → 15 秒内检测按下事件

  每步输出 `TEST_N: PASS` 或 `TEST_N: FAIL <reason>` 到串口。

- [ ] **T4.2** 串口验证脚本
  `tests/hardware/hw_test_runner.py` — Python 脚本监控串口，自动统计 PASS/FAIL

---

## Phase 5: E2E 集成测试

- [x] **T5.1** 卡片配置端到端流程
  创建 `tests/e2e/test_card_flow.py`
  1. 前端 POST 卡片配置 → 后端保存 → MQTT 推送 → 固件解析 → ACK
  2. 用 mock 替代真实 MQTT/HTTP，验证 JSON 格式一致

- [x] **T5.2** 设备生命周期端到端
  1. 设备上电 → WiFi 连接 → IP 定位 → NTP → MQTT 注册
  2. 管理员绑定用户 → 推送配置 → 设备切换卡片
  3. OTA 触发 → 下载 → 验证 → 切换分区 → 重启

---

## 执行策略

```
Phase 1 固件测试 (16 tasks) → 全部并行
Phase 2 后端测试 (3 tasks) → 全部并行  
Phase 3 前端测试 (4 tasks) → 全部并行
Phase 4 硬件测试 (2 tasks) → 串行
Phase 5 E2E 测试 (2 tasks) → 最后执行

合计: 27 tasks, 最大并行: 23 tasks
```
