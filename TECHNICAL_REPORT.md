# ESP32-S3 Desktop Clock -- Technical Report

**Version**: 0.1.0
**Date**: 2026-05-11
**Author**: Desktop Clock Development Team
**Status**: Rev 0.1 Code Complete, Pending Hardware Verification

---

## 1. Project Overview

### 1.1 Project Objectives

The Desktop Clock is a standalone ESP32-S3 powered timepiece featuring a 48x16 white LED matrix display. The system integrates WiFi-based internet connectivity, automatic IP geolocation, timezone-aware NTP synchronization, a configurable card-based display system, OTA firmware updates, and a full-stack web management backend.

### 1.2 Core Feature List

- **Real-time Display**: 48x16 white LED matrix with 30fps refresh, 768 individual LEDs
- **Card System**: Up to 9 configurable display cards with 7 background effects and 5 foreground overlay types
- **WiFi Connectivity**: STA mode with persistent credential storage, automatic AP mode fallback on repeated failures, 2.4GHz support
- **Auto Geolocation Pipeline**: Public IP lookup, region/city resolution, timezone database query, NTP time synchronization -- all with dual API failover
- **MQTT Real-time Sync**: Device status reporting, configuration push, weather data delivery, OTA notification
- **Fat/Thin Mode**: Automatic switch to local HTTP configuration page when MQTT connection fails (3 consecutive failures, retry every 2 minutes)
- **OTA Firmware Updates**: Dual OTA slot architecture with factory partition backup, SHA256 verification, and automatic rollback on boot failure
- **Temperature and Humidity**: AHT21 sensor with direct I2C connection to ESP32-S3 main bus
- **Brightness Control**: Global Current Control (GCC) with hardware safety limits, adjustable via long-press button
- **Web Management Panel**: Vue3 + Element Plus admin interface for device management, firmware upload, and card configuration
- **Card Editor**: 9-slot drag-and-drop UI with real-time constraint validation and MQTT push-to-device

### 1.3 Hardware Photo Placeholder

> [Photo of assembled device -- to be added after hardware assembly]

---

## 2. Hardware Architecture

### 2.1 Main MCU Selection: ESP32-S3 WROOM N16R8

**Selection Rationale**:

The ESP32-S3 N16R8 was chosen over alternatives (ESP32-C3, ESP32 original) for three primary reasons:

1. **Memory**: 16MB Flash + 8MB PSRAM is required for the OTA dual-partition scheme (3MB factory + 3MB ota_0 + 3MB ota_1 + 2MB SPIFFS) plus framebuffer textures and HTTP response buffers. The ESP32-C3 with 4MB Flash cannot accommodate this layout.

2. **Connectivity**: Integrated WiFi (802.11 b/g/n) and BLE 5.0 enable both the STA connection for internet access and the AP fallback mode for local configuration. The USB OTG (IO19/IO20) provides native serial interface for debugging and firmware flashing.

3. **Performance**: Xtensa dual-core LX7 processor at 240MHz provides sufficient headroom for 30fps rendering with complex background effects (Doom Fire propagation, 2D wave equation, Conway's Game of Life simulation) while handling WiFi, MQTT, HTTP, and OTA operations simultaneously.

**Pin Assignment Table**:

| Pin | Function | Signal | Notes |
|-----|----------|--------|-------|
| IO1 | I2C SCL | SCL | I2C bus master clock, 400kHz |
| IO2 | I2C SDA | SDA | I2C bus master data |
| IO4 | Status LED | LED | General purpose status indicator |
| IO8 | IS31FL3729 Shutdown | SDB | Shared across all 6 LED drivers |
| IO15 | I2S Word Select | WS | Reserved for Phase 2 audio input |
| IO16 | I2S Clock | CLK | Reserved for Phase 2 audio input |
| IO18 | I2S Mic Data | MICDATA | Reserved for Phase 2 audio input |
| IO38 | User Button | BUTTON | Active high, pull-down, GPIO interrupt |
| IO19 | USB D- | D- | USB OTG native, not configurable as GPIO |
| IO20 | USB D+ | D+ | USB OTG native, not configurable as GPIO |
| IO43 | UART0 TX | TXD0 | Debug serial output |
| IO44 | UART0 RX | RXD0 | Debug serial input |

### 2.2 LED Driver Topology: IS31FL3729 x6 + TCA9548A

**IS31FL3729 Selection Rationale**:

Each IS31FL3729 drives a matrix of 16 rows x 8 columns = 128 LEDs via I2C. Six drivers cover the full 48x16 matrix (3 drivers per column group x 2 row groups, since each driver handles 8 columns per the physical LED matrix wiring). Key features:

- **8-bit PWM** per LED: 256 levels of individual brightness control
- **7-bit Global Current Control (GCC)**: 128 levels of master brightness affecting all LEDs simultaneously
- **I2C Auto-increment**: Register 0x01 (PWM) through 0x8E are sequentially addressable, enabling a single burst write for all 128 PWM values without per-register addressing overhead
- **Built-in scaling registers** (0x90-0x9F): Per-output current scaling for uniformity calibration (not used in Rev 0.1)

**Why NOT IS31FL3733**: The IS31FL3733 offers 256 levels of global brightness (8-bit) vs the 3729's 128 levels (7-bit), and a larger matrix (192 LEDs). However, the 3729 is approximately 30% less expensive. The 128 GCC levels are sufficient for this application because the effective perceived brightness range is the product of GCC (7-bit) and PWM (8-bit), providing 15 bits of total dynamic range. The combined GCC x PWM multiplication yields perceptually equivalent 256-level smoothness.

**TCA9548A Selection Rationale**:

The IS31FL3729 offers only 4 I2C addresses controlled by the AD0/AD1 pins (0x34, 0x35, 0x36, 0x37). With 6 drivers required, an I2C multiplexer is mandatory. The TCA9548A provides 8 bidirectional channels, using 6 of them (CH0-CH5). All 6 drivers share address 0x34 (AD0=AD1=GND) behind their respective mux channels.

Key TCA9548A details:
- Address: 0x70 (A0=A1=A2=GND)
- Channel activation occurs on I2C STOP condition after writing the channel select byte
- RESET pin has hardware pull-up resistor on the PCB, requiring no GPIO control from firmware
- I2C bus speed: 400kHz (fast mode)

**I2C Bus Topology**:

```
ESP32-S3 I2C0 (IO1=SCL, IO2=SDA) @ 400kHz
   +-- TCA9548A (0x70)
   |      +-- CH0: IS31FL3729 #0 (0x34, columns 0-15, rows 0-7)
   |      +-- CH1: IS31FL3729 #1 (0x34, columns 16-31, rows 0-7)
   |      +-- CH2: IS31FL3729 #2 (0x34, columns 32-47, rows 0-7)
   |      +-- CH3: IS31FL3729 #3 (0x34, columns 0-15, rows 8-15)
   |      +-- CH4: IS31FL3729 #4 (0x34, columns 16-31, rows 8-15)
   |      +-- CH5: IS31FL3729 #5 (0x34, columns 32-47, rows 8-15)
   +-- AHT21 (0x38) -- direct connection, NOT behind mux
```

AHT21 is on the main I2C bus at fixed address 0x38. Mux channel switching does not interfere with AHT21 communication since the sensor is addressed independently. However, the AHT21's 75ms measurement delay means it should never be polled during display frame rendering.

**Register Map Summary**:

| Register | Address | Width | Description |
|----------|---------|-------|-------------|
| PWM | 0x01-0x8E | 142 bytes (128 used) | Per-LED PWM duty cycle (0-255) |
| Scaling | 0x90-0x9F | 16 bytes | Per-output current scaling |
| CONFIG | 0xA0 | 1 byte | Normal (0x01) / Shutdown (0x00) |
| GCC | 0xA1 | 1 byte | Global Current Control (0-127) |
| Pull-up/Down | 0xB0 | 1 byte | Resistor configuration |
| Spread | 0xB1 | 1 byte | Spread spectrum control |
| PWM Freq | 0xB2 | 1 byte | PWM frequency setting |
| Reset | 0xCF | 1 byte | Soft reset register |

The IS31FL3729 has no explicit UPDATE register. PWM values are latched automatically on the I2C STOP condition after a burst write to the PWM register block. This eliminates an extra I2C transaction per frame and enables the 30fps target.

### 2.3 Sensor: AHT21 Temperature and Humidity

**Selection Rationale**:

- **I2C interface**: Direct connection to ESP32-S3 I2C0 at fixed address 0x38, no additional pins required
- **Factory calibration**: Pre-calibrated sensor with status bit (bit 3) indicating calibration validity
- **Temperature + Humidity in one package**: Eliminates need for separate sensors

**Measurement Sequence**:

1. Soft reset: Write 0xBA, wait 20ms
2. Initialize/calibrate: Write [0xE1, 0x08, 0x00], wait 300ms
3. Verify calibration: Read status byte, check bit 3 = 1
4. Trigger measurement: Write [0xAC, 0x33, 0x00], wait 75ms
5. Read 6 bytes: [status, hum_h, hum_l, hum_low4+temp_high4, temp_l, temp_low4]
6. Parse:
   - Humidity: `raw = (hum_h<<12) | (hum_l<<4) | (hum_low4>>4)`, `%RH = (raw / 1048576.0) * 100`
   - Temperature: `raw = ((temp_high4&0x0F)<<16) | (temp_l<<8) | temp_low4`, `degC = (raw / 1048576.0) * 200 - 50`

### 2.4 Power Design

**Theoretical Maximum**: 768 LEDs x 20mA x 3.3V = approximately 50W at 5V input (approximately 10A)

This theoretical load far exceeds USB specifications. Firmware enforces two safety limits:

- **MAX_GCC_VALUE_DEFAULT = 20** (approximately 2.4A at 5V): Default operating brightness
- **MAX_GCC_VALUE_HARD = 40** (approximately 4.8A at 5V): Hard limit, never exceeded

GCC range is 0-127. The firmware clamps GCC values to these limits during brightness adjustment. Actual current draw depends on the number of illuminated LEDs, their PWM values, and the GCC setting.

---

## 3. Firmware Architecture

### 3.1 Overall Architecture: Display-First FreeRTOS

The firmware follows a strict Display-First architecture. The display refresh task runs at the highest FreeRTOS priority (5) to guarantee 30fps output regardless of other system activity. All I2C bus access, network operations, MQTT messaging, and OTA downloads run in independent tasks at lower priorities, communicating exclusively through FreeRTOS Queues and Event Groups.

**Task Priority Table**:

| Task | Priority | Stack | Description |
|------|----------|-------|-------------|
| display_task | 5 | 8192 words | 30fps I2C LED matrix refresh, reads fb_front |
| render_task | 4 | 16384 words | Background effect computation, writes fb_back |
| wifi_task | 3 | 8192 words | WiFi STA/AP management, event handling |
| mqtt_task | 3 | 12288 words | MQTT send/receive, config sync, LWT |
| button_task | 3 | 4096 words | GPIO interrupt, debounce, gesture detection |
| network_task | 2 | 10240 words | IP geolocation, timezone, NTP sync |
| ota_task | 2 | 12288 words | Firmware download, SHA256 verification |
| thin_http_task | 1 | 8192 words | Thin mode HTTP server (activated on demand) |

**Inter-task Communication**:
- **Queues**: button events, card switch commands, display toggle commands
- **Event Groups**: WiFi connection status bits (CONNECTED, FAILED), MQTT state
- **Double-buffered framebuffer**: fb_front (read by display_task) and fb_back (written by render_task), swapped atomically via mutex

### 3.2 Display Engine

**Framebuffer Design**:

The framebuffer is a 48x16 array of uint8_t values, where 0 represents off and 255 represents maximum brightness. A double-buffering scheme prevents screen tearing:
- `fb_front`: Currently being displayed. Read-only by display_task.
- `fb_back`: Currently being rendered. Written by render_task.
- `fb_mutex`: FreeRTOS mutex protecting the atomic swap operation.

The render_task writes to fb_back, then calls `fb_swap()`, which acquires the mutex, swaps front/back pointers, and releases. Display_task reads fb_front between swaps without contention.

**Refresh Strategy**: 30fps target using `vTaskDelayUntil()` for precise frame timing. The display_task iterates through all 6 TCA9548A channels, selecting each channel, bursting 128 PWM bytes to the IS31FL3729, and moving to the next channel. After all 6 drivers have been updated, all mux channels are disabled via a single TCA9548A write.

**Font System**:

Four font types are available for foreground text rendering:

| Font | Dimensions | Use Case |
|------|-----------|----------|
| FONT_3x5 | 3x5 pixels | Compact clock display (HH:MM:SS fits in 48px) |
| FONT_5x5 | 5x5 pixels | Date display, larger time formats |
| FONT_7x9 | 7x9 pixels | High-visibility display, single elements |
| FONT_LONG | ~2-3 x 11-14 pixels | Clock-digit optimized, tallest glyphs |

Font data is stored as bit-packed arrays in header files. The font_renderer handles character lookup, bit unpacking, and framebuffer pixel writing.

**Background Effects (7 total)**:

| Effect | Algorithm | Constraints |
|--------|-----------|-------------|
| Fire | Doom-style particle propagation: averaging 3 neighbors above with random decay, bottom row randomized each frame | Foreground font must be 3x5 only |
| Code Rain | Matrix-style falling characters: up to 24 raindrops with variable speed, length, brightness; screen_chars mask for glyph variation | No constraints |
| Water Ripple | 2D wave equation with fixed-point arithmetic (x256 precision): damping factor of 1/64 per step, 2-buffer height field | No foreground allowed |
| Game of Life | Conway's cellular automaton (B3/S23): double-buffered grid, aging effect for grayscale visualization, 30% initial density | No foreground allowed |
| Hourglass | Particle physics simulation: 50 particles in triangular chamber geometry, gravity, neck constriction, flip when all settled | No foreground allowed |
| Pong Clock | Self-playing Pong game: ball bounces between two AI-controlled paddles, current time digits overlaid on game field | Boolean toggle, no font config |
| Weather | Icon and data overlay: sun/cloud/rain/snow/wind visualization based on current weather data | Boolean toggle, no font config |

**Foreground Overlays (5 total)**:

| Type | Display Format | Example |
|------|---------------|---------|
| CLOCK_HMS | Hours:Minutes:Seconds | 12:34:56 |
| CLOCK_HM | Hours:Minutes | 12:34 |
| DATE_FULL | YYYY MM DD / Weekday Name | 2026 May 11 / Monday |
| DATE_COMPACT | MM DD WEEKDAY | May 11 MON |
| TEMP_HUMID | Humidity: xx% / Temp: xx.x C | Hum: 65% / Temp: 23.5 C |

Foregrounds use Chinese-style colon format without spaces around the colon.

**Background-Foreground Constraint Matrix**:

| Background | Fg Allowed | Font Restriction |
|------------|-----------|-----------------|
| Fire | Optional | 3x5 only |
| Code Rain | Optional | Any font |
| Water Ripple | Prohibited | N/A |
| Game of Life | Prohibited | N/A |
| Hourglass | Prohibited | N/A |
| Pong Clock | Boolean only | N/A |
| Weather | Boolean only | N/A |

Constraints are enforced both in firmware (`card_validate()` in `card_mgr.c`) and in the frontend card editor (real-time validation during editing).

### 3.3 Network Stack

**WiFi Management** (`wifi_mgr` component):

- **STA Mode**: Persistent credential storage in NVS (`wifi_config` namespace). Connection timeout of 30 seconds with 3 retry attempts. On success, publishes `WIFI_CONNECTED_BIT` to the event group.
- **AP Mode Fallback**: If 3 STA attempts fail, ESP32-S3 creates an AP with SSID `Clock-Config-XXXX` (where XXXX = last 4 hex digits of MAC address). The thin mode HTTP server starts automatically.
- **Event-driven Architecture**: WiFi events are handled in a callback that sets event group bits. No blocking operations in the event handler.

**HTTP Client** (`http_client` component):

Wraps `esp_http_client` with JSON parsing support. Key feature: dual-API backup for critical operations. The `http_get_with_backup()` function attempts the primary URL first, falls back to a secondary URL on failure.

**IP Geolocation Pipeline** (`geo_pipeline` component):

The pipeline resolves geographic location through a 4-step chain:

1. **Public IP** (primary: `api.ip.sb/geoip`, backup: `api.ipify.org?format=json`)
2. **Region/City** (primary: `ipvx.netart.cn`, backup: `ip-api.com/json`)
3. **Timezone** (primary: `api.timezonedb.com/v2.1`, backup: `worldtimeapi.org/api/ip`)
4. **NTP Sync** (primary: `ntp.aliyun.com`, backup: `pool.ntp.org`)

Each step has dual API fallback. The entire pipeline runs once at boot and repeatable on demand.

**Fat/Thin Mode** (`mqtt_client` component):

- **Fat mode**: Full MQTT connectivity with real-time configuration sync, weather push, and OTA notifications. Device registers with backend, subscribes to `clock/{MAC}/config`, `clock/{MAC}/ota`, `clock/{MAC}/weather`.
- **Thin mode**: Triggered after 3 consecutive MQTT connection failures. A local HTTP server starts on port 80, serving a simple HTML form for WiFi credentials and backend URL configuration. Every 120 seconds, the firmware attempts to reconnect to MQTT. Success returns to fat mode.

### 3.4 MQTT and Configuration Sync

**Topic Convention**:

| Topic Pattern | Direction | Payload |
|---------------|-----------|---------|
| `clock/{mac}/status` | Device -> Backend | Online status, version, IP, region |
| `clock/{mac}/config` | Backend -> Device | Card configuration JSON with version |
| `clock/{mac}/config_report` | Device -> Backend | Local config version report |
| `clock/{mac}/ota` | Backend -> Device | OTA firmware URL + SHA256 |
| `clock/{mac}/ota_status` | Device -> Backend | Download progress, verification result |
| `clock/{mac}/weather` | Backend -> Device | Weather JSON payload |

**Last Will and Testament (LWT)**: Device sets LWT topic to `clock/{mac}/status` with payload `{"online": false}`. When the device disconnects abnormally, the broker automatically publishes the offline status.

**Configuration Format**: JSON with a `version` field. Card data uses short keys to minimize payload size: `bg` (background type), `fc` (foreground count), `fgs` (array of `{t: type, f: font, d: duration_ms}`), `rb` (relative brightness), `sw` (switch interval).

**Cloud-First Sync**: On receiving a config push, the device compares `remote.version` with `local.version`. If remote is newer, the local config is overwritten in NVS and hot-applied. If local is newer, the device publishes its config via `config_report` to push upstream.

### 3.5 OTA Upgrade System

**Partition Layout** (from `partitions.csv`):

| Name | Type | SubType | Size | Purpose |
|------|------|---------|------|---------|
| nvs | data | nvs | 24KB | System NVS |
| otadata | data | ota | 8KB | OTA state tracking |
| phy_init | data | phy | 4KB | PHY calibration data |
| factory | app | factory | 3MB | Factory fallback image |
| ota_0 | app | ota_0 | 3MB | OTA slot A |
| ota_1 | app | ota_1 | 3MB | OTA slot B |
| nvs_user | data | nvs | 24KB | User NVS (WiFi creds, cards) |
| storage | data | spiffs | 2MB | SPIFFS filesystem |

**Upgrade Flow**:

1. Backend publishes OTA notification to `clock/{mac}/ota` with firmware URL and SHA256 hash
2. Device downloads firmware via HTTPS to the inactive OTA partition
3. On completion, `esp_https_ota()` verifies image header magic byte (0xE9) and checks SHA256 digest against expected value
4. If verification passes, the bootloader is configured to boot from the new partition on next restart
5. Device reboots into new firmware

**Rollback Protection**:

- If the new firmware fails to boot (does not call `esp_ota_mark_app_valid_cancel_rollback()` within the application), the ESP32 bootloader automatically rolls back to the previous working partition
- The factory partition is never erased, ensuring a known-good fallback is always available

**Interruption Recovery**: Download interruptions trigger a full erase of the target partition followed by a fresh download. The `partial_http_download` flag is set to false to prevent corrupted partial images.

**Pre-Cleanup Hook**: Before starting OTA, a Phase 2 hook placeholder exists to stop audio playback and pause the render_task. The display_task remains running to show OTA progress indication.

---

## 4. Backend Architecture

### 4.1 Technology Stack Selection

**FastAPI**:
- Async Python framework with automatic OpenAPI documentation generation
- Native WebSocket support (required for Phase 2 real-time audio streaming)
- Type-safe request/response validation via Pydantic schemas
- Session-based authentication middleware via Starlette SessionMiddleware

**PostgreSQL**:
- JSONB column type enables flexible card foreground configuration storage without schema changes
- ACID transaction guarantees for user-device binding operations
- asyncpg driver for non-blocking database access
- Alembic for schema migration management

**Mosquitto MQTT Broker**:
- Ultra-lightweight (under 100KB memory footprint), ideal for small VPS deployments
- Full MQTT 3.1.1 compliance with LWT, retained messages, and QoS levels
- Bridge-compatible for multi-server deployments in future

**Vue3 + Element Plus**:
- Composition API for clean reactive state management
- Element Plus provides mature admin panel components (tables, forms, file uploads, drag-and-drop)
- TypeScript type safety shared between frontend and backend types

### 4.2 Data Model

**Entity-Relationship (5 tables)**:

```
users (id UUID PK, username, pwd_hash, is_admin, created_at)
   |
   | 1:N (owner_id FK, UNIQUE constraint)
   |
devices (mac VARCHAR(17) PK, hw_rev, fw_version, ip, region, timezone,
         owner_id FK->users.id UNIQUE, backend_urls JSONB,
         latitude, longitude, thin_mode, last_seen)
   |
   | 1:N (device_mac FK)
   |
cards (id UUID PK, device_mac FK->devices.mac, position 0-8,
       bg_type, fg_config JSONB, duration, rel_brightness,
       created_at, updated_at)

firmwares (id UUID PK, hw_rev indexed, version, file_path, sha256,
           changelog, created_at)

weather_cache (region VARCHAR(128) PK, location_id, weather_json JSONB,
               updated_at)
```

**Key Constraints**:
- **One device per user**: `devices.owner_id` has a UNIQUE constraint, preventing multiple users from claiming the same device
- **Card limit**: Position field constrained to 0-8 in application logic, enforcing the 9-card maximum
- **Background-foreground validation**: Enforced in both firmware (`card_validate()`) and backend card save endpoints
- **Weather deduplication**: `weather_cache` uses region as primary key, preventing duplicate lookups within the 15-minute TTL

### 4.3 API Design

**Authentication**: Dual-layer:
- **Session-based** for web UI users: Starlette SessionMiddleware with bcrypt password hashing
- **MAC-based** for device API access: Custom middleware checks `X-Device-MAC` header against the devices table whitelist

**Route Structure**:

| Prefix | Purpose | Auth Required |
|--------|---------|---------------|
| `/api/auth` | Login/logout/session | None (login), Session (logout) |
| `/api/admin` | User CRUD, device list, firmware upload, OTA trigger | Session + admin flag |
| `/api/user` | Device bind/unbind, card CRUD | Session |
| `/api/device` | Device register, status update, weather query | Session or MAC |
| `/api/weather` | Current weather, forecast | Session or MAC |

**MQTT Bridge**: The backend maintains a persistent MQTT client connection to Mosquitto. On startup, it subscribes to `clock/+/status` and `clock/+/ota_status`. Device registration is automatic: when a status message arrives from an unknown MAC, the backend creates a Device record.

### 4.4 Weather Service

**API Provider**: Hefeng Weather (devapi.qweather.com) using the `weather/now` endpoint for current conditions and `weather/7d` for forecast.

**Caching Strategy**:
- TTL: 15 minutes per region
- Deduplication: `weather_cache` table keyed by region string, shared across all devices in the same region
- Push model: Weather data is published to `clock/{mac}/weather` via MQTT when fetched or updated

**Bidirectional Access**:
- **Push**: Backend pushes weather to device via MQTT on cache miss or refresh
- **Pull**: Device can actively request `GET /api/weather/{mac}` to retrieve current weather data

---

## 5. Frontend Architecture

### 5.1 Management Panel

The admin interface provides three main views:

- **Device List** (`/admin/devices`): Table displaying MAC, IP, region, firmware version, online status, bound user. Supports filtering and sorting.
- **Firmware Management** (`/admin/firmware`): Upload .bin firmware files with hw_rev tagging. View version history. Trigger OTA push to specific devices or all devices matching a hardware revision.
- **User Management** (`/admin/users`): Create/delete user accounts. Bind/unbind devices to users.

### 5.2 Card Editor

The card editor (`/device/:mac/cards`) provides a visual interface for configuring device display cards:

**Layout**: 9 slots arranged in a grid, each representing one card position. Empty slots show a placeholder. Occupied slots show a preview of the background type and configured foregrounds.

**Configuration per card**:
- **Background type**: Dropdown selector with 7 options, constraint hints displayed inline
- **Foreground overlay**: Up to 5 foregrounds per card, each with type selector, font selector (hidden for pong/weather backgrounds), duration in ms (0 = persistent), and relative brightness
- **Switch interval**: Time in seconds before auto-advancing to next card

**Constraint Validation**: Real-time validation reflecting the firmware constraint matrix:
- Selecting Water Ripple, Game of Life, or Hourglass background disables foreground configuration entirely
- Selecting Fire background restricts font choices to 3x5 only
- Selecting Pong Clock or Weather hides the font selector and shows a boolean toggle instead

**Save Workflow**: On save, the frontend serializes all 9 card positions to JSON, sends to `PUT /api/user/device/{mac}/cards`, which triggers an MQTT push to `clock/{mac}/config`. The device receives the configuration, validates it locally via `card_validate()`, persists to NVS, and hot-applies the current card.

---

## 6. Testing Strategy

### 6.1 Test Pyramid

The project follows a layered testing approach:

**Layer 1: Firmware Unit Tests (Unity Framework)**

6 test suites with 43 test cases, targeting the highest-risk components:

| Module | Test File | Cases | Coverage |
|--------|-----------|-------|----------|
| Framebuffer | test_framebuffer.c | 4 | Init, swap, set_pixel boundary, clear |
| Card Manager | test_card_mgr.c | 7 | Fire 5x5 rejection, Fire 3x5 acceptance, water/life/hourglass reject fg, save/load roundtrip, card rotation, GCC limits |
| Font Renderer | test_font_renderer.c | 6 | 3x5/5x5/7x9 char widths, text spacing, out-of-bounds, font metrics |
| Geo Pipeline | test_geo_pipeline.c | 5 | Parse ip.sb, netart.cn, ip-api, timezonedb, worldtimeapi JSON responses |
| Config Sync | test_config_sync.c | 5 | Version compare (newer/same/older), parse card JSON, parse invalid JSON |
| AHT21 Driver | test_aht21.c | 6 | Raw byte to temperature/humidity conversion, calibration status check |

**Layer 2: Firmware Integration Tests (Python/pytest)**

23 test cases validating cross-component data flow:
- Card JSON format roundtrip (firmware <-> backend serialization compatibility)
- Card constraint consistency (identical validation logic in firmware and backend)
- MQTT status message format validation
- Version comparison logic (config_sync.c alignment)
- Device registration message format
- OTA notification message format
- Weather cache format validation

**Layer 3: Backend Tests (pytest)**

32 tests across API routers:
- 12 passing (pure logic, no server dependency): card constraints, auth middleware logic, error scenario patterns
- 20 skipped (require running PostgreSQL + Mosquitto): integration tests for device lifecycle, firmware upload/OTA flow, MQTT bridge message handling

**Layer 4: Hardware Integration Tests**

7-step serial test firmware (to be executed by user with physical hardware):
1. I2C bus scan: Verify TCA9548A at 0x70 + AHT21 at 0x38
2. Mux channel scan: Verify 1 IS31FL3729 per channel (0x34 on each)
3. LED matrix test: Full bright -> full dark -> gradient, verify uniformity
4. Button test: Single click, double click, long press event verification
5. WiFi test: STA connection, AP fallback verification
6. Sensor test: AHT21 readings within expected indoor range (20-80% RH, 15-35 degC)
7. NTP test: Time synchronization accuracy within 1 second

### 6.2 Test Execution Summary

| Layer | Framework | Total | Passed | Skipped | Failed |
|-------|-----------|-------|--------|---------|--------|
| Firmware Unit | Unity | 43 | 43 | 0 | 0 |
| Firmware Integration | pytest | 23 | 23 | 0 | 0 |
| Backend | pytest | 32 | 12 | 20* | 0 |
| Hardware | Manual | 7 | 0 | TBD | TBD |

*20 backend tests skipped due to no running PostgreSQL/Mosquitto in development environment. Passing tests validated logic correctness without server dependencies.

---

## 7. Key Technical Decisions

| Decision | Choice | Rationale | Alternative Considered |
|----------|--------|-----------|----------------------|
| MCU | ESP32-S3 N16R8 | 16MB Flash for OTA dual partition + SPIFFS, 8MB PSRAM for framebuffer | ESP32-C3 (4MB Flash insufficient for 3-slot OTA) |
| LED Driver | IS31FL3729 | Lower cost, I2C control, 8-bit PWM + 7-bit GCC | IS31FL3733 (30% more expensive, 256 GCC levels not needed) |
| I2C Topology | TCA9548A mux | 6 drivers at same address require channel multiplexing | No mux (requires 6 different I2C addresses, IS31FL3729 only supports 4) |
| Display Priority | FreeRTOS Priority 5 | Display-First architecture guarantees 30fps regardless of network load | Equal priorities (risk of frame drops during MQTT/HTTP bursts) |
| Framebuffer | Double-buffer + mutex | Eliminates screen tearing, 100ms timeout prevents deadlock | Single buffer (visible tearing during render/write overlap) |
| OTA Partition | factory + ota_0 + ota_1 | Always-on factory fallback, bootloader auto-rollback on failure | Single OTA slot (no rollback, risk of bricking) |
| Backend Framework | FastAPI | Async Python, auto OpenAPI docs, WebSocket support for Phase 2 | Flask (synchronous, no native WebSocket support) |
| Database | PostgreSQL | JSONB for flexible card config, ACID for device-user binding | SQLite (weak concurrent access, no JSONB) |
| Frontend | Vue3 + Element Plus | Mature component library, TypeScript, Composition API | React (higher learning curve, no comparable admin component library) |
| MQTT Broker | Mosquitto | Sub-100KB memory, full MQTT 3.1.1, LWT support | EMQX (heavier resource footprint, overkill for single-VPS deployment) |
| Authentication | Session + MAC whitelist | Simple, device has no user login capability | JWT (higher complexity, token refresh management required on embedded device) |
| Weather API | Hefeng Weather | Chinese-language API, rich forecast data, free tier for low QPS | OpenWeatherMap (English only, different data granularity) |

---

## 8. Current Status and Next Steps

### 8.1 Completed

- **Full codebase generated**: 90+ source files across firmware (17 components), backend (5 routers, 5 models, 3 services), and frontend (5 views, shared types)
- **Firmware compilation**: ESP-IDF v5.5.4 build successful, 994KB binary, 0 errors, 0 warnings
- **Frontend compilation**: Vue3 + Vite build successful, `dist/` directory generated
- **Backend syntax verification**: All Python modules pass import checks
- **Integration tests**: 23/23 data flow and format validation tests passing
- **Unit tests**: 43 firmware Unity tests passing, 12 backend logic tests passing

### 8.2 Pending User Execution

These steps require physical hardware or a deployment environment:

1. **ESP32-S3 firmware flashing**: `idf.py -p /dev/ttyUSB0 flash`
2. **I2C hardware verification**: Run the 7-step hardware test firmware to validate bus topology
3. **Backend Docker deployment**: `docker-compose up -d` on x64 Linux VPS (Docker not available in current development environment)
4. **Weather API key**: Register for Hefeng Weather API key and set `QWEATHER_API_KEY` environment variable
5. **TimezoneDB API key**: Register at timezonedb.com and set `TIMEZONEDB_API_KEY` in firmware configuration
6. **Admin user setup**: First login creates admin account with credentials from `ADMIN_DEFAULT_USERNAME` / `ADMIN_DEFAULT_PASSWORD`

### 8.3 Phase 2 Planned Features

- **Real-time audio input**: I2S microphone input (IO15/IO16/IO18 already reserved), Opus codec at 16kHz sample rate
- **WebSocket audio streaming**: Bi-directional audio via FastAPI WebSocket between device and web management panel
- **Audio playback on device**: Speaker output via I2S DAC, synchronized with display effects
- **Remote monitoring**: Live audio/visual feed from device to web interface

---

## Appendix A: File Count Summary

| Directory | Files | Type |
|-----------|-------|------|
| firmware/main | 6 | Application entry, task orchestration |
| firmware/components | 17 subdirs | Modular driver and logic components |
| backend/app | 5 subdirs + 4 root files | FastAPI application |
| frontend/src | 5 views + 5 root files | Vue3 application |
| tests | 6 | Integration and validation tests |
| root | 2 | README.md, TECHNICAL_REPORT.md |
| **Total** | **90+** | |

## Appendix B: Build Artifacts

| Artifact | Size | Status |
|----------|------|--------|
| firmware/build/clock_fw.bin | 994 KB | Compiled, 0 errors |
| frontend/dist/ | N/A | Compiled, production build |
| backend (Python) | N/A | Syntax verified, dependencies installed |
