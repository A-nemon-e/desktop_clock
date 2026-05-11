# Desktop Clock — Verification & Test Completion Plan

## TL;DR
> 对已生成的90+源文件进行编译验证、硬件烧录验证、补全缺失的单元测试。

---

## Phase 1: 编译验证（先确认代码能编译）

### 前端编译

- [x] **F-1.1** `npm install` 安装依赖
  ```bash
  cd frontend && npm install
  ```
  预期: 无错误。如有 package.json 依赖版本冲突，逐个修复。

- [x] **F-1.2** `npm run build` 生产构建
  ```bash
  cd frontend && npm run build
  ```
  预期: 无 TypeScript 错误，dist/ 目录生成。

### 后端编译

- [ ] **B-1.1** `docker-compose up -d` — ⛔ Docker 未安装
- [x] **B-1.2** `pip install` — ✅ 核心依赖已装 (pytest,fastapi,sqlalchemy,asyncpg,paho-mqtt,apscheduler,httpx,pydantic-settings)
- [x] **B-1.3** `pytest` — ✅ 12 passed, 20 skipped, 0 failed
  ```bash
  cd backend && pytest -v
  ```
  预期: 全部 PASS。如失败，逐个修复。

### 固件编译

- [x] **FW-1.1** — ✅ ESP-IDF v5.5.4 就绪
  ```bash
  idf.py --version
  ```

- [x] **FW-1.2** — ✅ 编译成功, clock_fw.bin 961KB, 0 errors
  ```bash
  cd firmware && idf.py set-target esp32s3 && idf.py build
  ```
  预期: 无编译错误。如失败，逐个修复 include 路径、组件依赖。

---

## Phase 2: 硬件烧录验证（用户执行）

- [ ] **HW-2.1** — ⏳ 需要物理硬件 + ESP-IDF
- [ ] **HW-2.2** — ⏳ 需要物理硬件 + ESP-IDF
- [ ] **HW-2.3** — ⏳ 需要物理硬件 + ESP-IDF + 显示测试固件
  ```bash
  # 预期: 全屏亮→全灭→渐变，亮度均匀
  ```

---

## Phase 3: 补单元测试

### 固件 Unity 测试（最高风险组件优先）

- [x] **UT-3.1** `framebuffer` 测试
  创建 `firmware/components/display/test/test_framebuffer.c`：
  ```c
  // test_fb_init: 验证 framebuffer_init 后 buf 全零
  // test_fb_swap: 写 back→swap→读 front，验证数据正确交换
  // test_fb_set_pixel: 边界内写入 → 读回一致；边界外 → 不越界
  // test_fb_clear: 写满后 clear → 全零
  ```

- [x] **UT-3.2** `card_mgr` 约束验证测试
  创建 `firmware/components/cards/test/test_card_mgr.c`：
  ```c
  // test_fire_rejects_5x5_font: BG_FIRE + fg.font=5x5 → card_validate=false
  // test_fire_accepts_3x5_font: BG_FIRE + fg.font=3x5 → card_validate=true
  // test_water_rejects_any_fg: BG_WATER_RIPPLE + fg_count>0 → false
  // test_life_rejects_any_fg: BG_GAME_OF_LIFE + fg_count>0 → false
  // test_hourglass_rejects_any_fg: BG_HOURGLASS + fg_count>0 → false
  // test_save_load_roundtrip: card_mgr_save → card_mgr_load → 数据一致
  // test_card_next_rotates: 3卡片→next 3次→回到第一张
  // test_gcc_limits: brightness_adjust 不超过 [5,127]
  ```

- [x] **UT-3.3** `font_renderer` 测试
  创建 `firmware/components/display/test/test_font_renderer.c`：
  ```c
  // test_3x5_char_width: 每个字符渲染后返回宽度=3
  // test_5x5_char_width: 返回宽度=5
  // test_7x9_char_width: 返回宽度=7
  // test_render_text_spacing: "88" → 两字符间距1px
  // test_out_of_bounds: 字符超出 fb → 不崩溃
  // test_font_metrics: font_char_width(0)=3, font_char_height(0)=5
  ```

- [x] **UT-3.4** `geo_pipeline` JSON 解析测试
  创建 `firmware/components/geo/test/test_geo_pipeline.c`：
  ```c
  // test_parse_ip_sb_response: mock JSON → 正确提取 ip 字段
  // test_parse_netart_response: mock JSON → 正确提取 regions[0..2]
  // test_parse_ipapi_response: mock JSON → 正确提取 regionName/city/lat/lon
  // test_parse_timezonedb_response: mock JSON → 正确提取 zoneName
  // test_parse_worldtimeapi_response: mock JSON → 正确提取 timezone
  ```

- [x] **UT-3.5** `config_sync` 测试
  创建 `firmware/components/config/test/test_config_sync.c`：
  ```c
  // test_version_compare_newer_remote: 远端v2 > 本地v1 → 覆盖
  // test_version_compare_same: 相同版本 → 忽略
  // test_version_compare_older_remote: 远端v1 < 本地v2 → 上报本地
  // test_parse_card_json: 有效JSON → 正确解析 card_t
  // test_parse_invalid_json: 无效JSON → 返回错误
  ```

### 后端 pytest 测试（每个 API router）

- [x] **UT-3.6** 后端 `card constraints` 测试
  追加到 `tests/test_error_scenarios.py`：
  ```python
  # test_admin_bind_conflict: 已绑定设备再绑定 → 409
  # test_admin_unbind: 解绑后 → owner_id=None
  # test_unbound_device_ok: 未绑定设备查询 → 不报错
  ```

- [x] **UT-3.7** 后端 `device lifecycle` 测试
  追加到 `tests/integration/test_e2e.py`：
  ```python
  # test_device_auto_register: MQTT status → Device 创建
  # test_device_delete_cascades_cards: 删除设备 → 关联 Card 也删除
  ```

- [x] **UT-3.8** 后端 `firmware OTA` 测试
  ```python
  # test_upload_firmware: 上传 .bin → Firmware 记录创建
  # test_ota_trigger: POST /ota → MQTT 消息发布
  # test_ota_status_tracking: 设备上报 ota_status → 状态更新
  ```

---

## Phase 4: Agent QA 执行

- [x] **QA-4.1** — ✅ 12 passed, 20 skipped, 0 failed (无服务器时优雅跳过)
- [x] **QA-4.2** 运行前端 build

> ✅ 已收集: `.sisyphus/evidence/frontend-build.txt`, `.sisyphus/evidence/pytest-results.txt`
  ```bash
  cd firmware && idf.py build 2>&1 | tee .sisyphus/evidence/firmware-build.txt
  ```

---

## Phase 5: 文档补充

- [x] **DOC-5.1** 在 README.md 添加 "已知限制" 章节
  - `TIMEZONEDB_API_KEY` 需要注册获取
  - `MAX_GCC_VALUE` 需要在硬件上实测
  - 音频功能为 Phase 2

- [x] **DOC-5.2** 添加 ESP-IDF 环境搭建步骤到 README

---

## 执行顺序

```
Phase 1: 编译验证 → Phase 2: 硬件验证 → Phase 3: 补测试 → Phase 4: QA执行
```

Phase 3 的任务可以全部并行执行（无依赖）。

## 提交策略

每个 Phase 完成后一个 commit。编译错误修复单独 commit。测试添加单独 commit。
