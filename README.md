# Desktop Clock — ESP32-S3 LED Matrix Clock

A standalone desktop clock built on ESP32-S3 with a 48×16 white LED matrix display, WiFi connectivity, and a full-stack management backend.

## Hardware

| Component | Details |
|---|---|
| MCU | ESP32-S3 N16R8 (16MB Flash, 8MB PSRAM) |
| Display | 48×16 white LED matrix (768 LEDs total) |
| LED Drivers | 6× IS31FL3729 (8×16 each) |
| I2C Mux | TCA9548A (8-channel, channels 0-5 used) |
| Sensor | AHT21 temperature + humidity |
| Input | Tactile button (click/double-click/long-press) |
| Power | USB 5V |

**I2C Bus Layout:**
```
ESP32-S3 I2C0
  └── TCA9548A (addr 0x70)
        ├── CH0: IS31FL3729 #0 (columns  0-15)
        ├── CH1: IS31FL3729 #1 (columns 16-31)
        ├── CH2: IS31FL3729 #2 (columns 32-47)
        ├── CH3: IS31FL3729 #3 (columns  0-15, row offset 8)
        ├── CH4: IS31FL3729 #4 (columns 16-31, row offset 8)
        └── CH5: IS31FL3729 #5 (columns 32-47, row offset 8)
```

## Features

- **Display Engine**: 30fps rendering with 7 background effects + 5 foreground overlays
- **Card System**: Up to 9 configurable display "cards" rotating on double-click
- **WiFi**: STA mode with persistent credential storage, AP fallback on repeated failures
- **Geo + NTP**: IP geolocation → timezone → NTP sync, with dual-API failover
- **MQTT**: Real-time configuration push, weather data delivery, OTA notifications
- **Fat/Thin Mode**: Auto-switch on MQTT connection failure (3 consecutive → thin HTTP)
- **OTA**: OTA firmware updates with dual-slot rollback
- **Weather**: 7-day forecast with current conditions display
- **Brightness Control**: Global Current Control (0-127), long-press button cycle
- **Admin Panel**: Vue3 + Element Plus management UI

## Display Effects

### Backgrounds (7)
| Effect | Description |
|---|---|
| Fire | Doom-style fire particle simulation |
| Code Rain | Matrix-style falling characters |
| Water Ripple | 2D wave propagation |
| Game of Life | Conway's cellular automaton |
| Hourglass | Animated sand timer |
| Pong Clock | Self-playing Pong with clock digits |
| Weather | Sun/cloud/rain/snow/wind visualization |

### Foregrounds (5)
| Overlay | Example |
|---|---|
| Clock HMS | `12:34:56` |
| Clock HM | `12:34` |
| Date + Weekday | `2026 May 11` / `Monday` |
| Compact Date | `May 11 MON` |
| Temp + Humidity | `Hum: 65%` / `Temp: 23.5 C` |

### Fonts (4)
`3x5` · `5x5` · `7x9` · `long` (clock-digit optimized, ~2-3×11-14px)

## Project Structure

```
desktop-clock/
├── firmware/                    # ESP-IDF v5.5 firmware
│   ├── main/                    # Application entry point
│   │   ├── app_main.c           # Main task orchestration
│   │   ├── task_prio.h          # FreeRTOS priority definitions
│   │   ├── i2c_scan.c           # I2C bus scanner
│   │   └── gpio_test.c          # GPIO validation utility
│   ├── components/
│   │   ├── shared/              # Cross-cutting types + error codes
│   │   ├── config/              # Pinmap, power limit, NVS config sync
│   │   ├── log/                 # Header-only logging (3-letter tags)
│   │   ├── display/             # Display engine, fonts, effects
│   │   ├── i2c_mux/             # TCA9548A driver
│   │   ├── led_matrix/          # IS31FL3729 driver
│   │   ├── sensors/             # AHT21 driver
│   │   ├── button/              # Button debounce + gesture detect
│   │   ├── wifi_mgr/            # WiFi STA/AP manager
│   │   ├── http_client/         # HTTP client (geo, thin mode)
│   │   ├── ntp/                 # NTP time sync
│   │   ├── geo/                 # IP geolocation pipeline
│   │   ├── mqtt_client/         # MQTT with LWT + reconnect
│   │   ├── cards/               # Card rotation manager
│   │   ├── thin_mode/           # Thin mode HTTP handler
│   │   ├── ota/                 # OTA update handler
│   │   └── version/             # Build version stamp
│   ├── tests/
│   │   └── stability_monitor.py # 24h+ stability test framework
│   ├── partitions.csv           # Partition table (factory + 2 OTA slots)
│   └── sdkconfig.defaults       # ESP-IDF Kconfig defaults
│
├── backend/                     # FastAPI + PostgreSQL
│   ├── app/
│   │   ├── main.py              # FastAPI entry + middleware config
│   │   ├── config.py            # Pydantic settings
│   │   ├── database.py          # Async SQLAlchemy engine
│   │   ├── models/              # SQLAlchemy ORM models
│   │   ├── schemas/             # Pydantic request/response schemas
│   │   ├── routers/             # API route handlers
│   │   ├── services/            # Business logic (MQTT bridge, weather, OTA)
│   │   └── middleware/          # Auth middleware (session + X-Device-MAC)
│   ├── alembic/                 # Database migrations
│   ├── mosquitto/               # Mosquitto MQTT broker config
│   ├── docker-compose.yml       # Postgres + Mosquitto + Backend
│   ├── Dockerfile
│   └── requirements.txt
│
├── frontend/                    # Vue3 + Element Plus
│   ├── src/
│   │   ├── views/               # Page components
│   │   ├── components/          # UI components (card editor, etc.)
│   │   ├── api.ts               # Axios API client
│   │   ├── router.ts            # Vue Router config
│   │   └── types.ts             # TypeScript interfaces
│   ├── package.json
│   └── vite.config.ts
│
└── tests/                       # Backend integration tests
    ├── integration/
    │   └── test_e2e.py          # End-to-end API tests
    └── test_error_scenarios.py  # Error + constraint validation tests
```

## Quick Start

### Prerequisites
- Python 3.11+ · Node.js 20+ · Docker + Docker Compose
- ESP-IDF v5.5 (for firmware builds only)
- USB-to-serial adapter for flashing

### 1. Start Backend Services

```bash
cd backend
cp .env.example .env       # Edit SECRET_KEY and QWEATHER_API_KEY
docker compose up -d
```

This starts PostgreSQL 15, Mosquitto MQTT broker, and the FastAPI backend on port 8000.

### ESP-IDF Environment Setup

```bash
# 1. Install ESP-IDF v5.5
git clone -b v5.5 --recursive https://github.com/espressif/esp-idf.git ~/esp-idf-v5.5
cd ~/esp-idf-v5.5
./install.sh esp32s3                # Install toolchain for ESP32-S3
source export.sh                    # Activate environment (add to ~/.bashrc)

# 2. Verify installation
idf.py --version                    # Should show v5.5.x

# 3. Configure project
cd firmware
idf.py set-target esp32s3           # One-time target selection
idf.py menuconfig                    # Verify: PSRAM enabled, partition table = custom
idf.py build                         # Compile (expect ~30s on first build)
```

```bash
cd frontend
npm install
npm run dev                # Vite dev server on port 5173
```

### 3. Flash Firmware

```bash
cd firmware
idf.py set-target esp32s3
idf.py menuconfig          # Set serial port + PSRAM
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

On first boot, the device enters AP mode (`DesktopClock_XXXX`). Connect and configure WiFi credentials via the captive portal.

### 4. Access Admin Panel

1. Open `http://localhost:5173` in browser
2. Log in with default admin credentials (`admin` / `admin123`)
3. Register your device via MAC address
4. Create and edit display cards

## Backend API Endpoints

### Auth
| Method | Path | Description |
|---|---|---|
| POST | `/api/auth/register` | Register new user |
| POST | `/api/auth/login` | Login (session-based) |
| POST | `/api/auth/logout` | Logout |
| GET | `/api/auth/me` | Get current user |

### User (Session Required)
| Method | Path | Description |
|---|---|---|
| GET | `/api/user/profile` | User profile |
| GET | `/api/user/devices` | List bound devices |
| POST | `/api/user/devices/bind` | Bind device to user |
| POST | `/api/user/devices/unbind` | Unbind device |
| GET | `/api/user/devices/{mac}/cards` | List device cards |
| POST | `/api/user/devices/{mac}/cards` | Create card |
| PUT | `/api/user/devices/{mac}/cards/{id}` | Update card |
| DELETE | `/api/user/devices/{mac}/cards/{id}` | Delete card |

### Device (X-Device-MAC Header Required)
| Method | Path | Description |
|---|---|---|
| POST | `/api/device/register` | Register/refresh device |
| GET | `/api/device/{mac}` | Get device info |
| PATCH | `/api/device/{mac}` | Update device fields |
| POST | `/api/device/status` | Device status report |
| GET | `/api/device/config_sync` | Sync local config |
| POST | `/api/device/ota_status` | Report OTA result |

### Admin (Session + is_admin Required)
| Method | Path | Description |
|---|---|---|
| GET | `/api/admin/users` | List all users |
| POST | `/api/admin/users` | Create user |
| DELETE | `/api/admin/users/{id}` | Delete user |
| GET | `/api/admin/devices` | List all devices |
| GET | `/api/admin/devices/{mac}` | Get device details |
| DELETE | `/api/admin/devices/{mac}` | Delete device + cards |
| POST | `/api/admin/bind` | Admin force-bind device |
| PUT | `/api/admin/backend-urls` | Set backend URLs |
| POST | `/api/admin/firmware/upload` | Upload firmware |

### Weather
| Method | Path | Description |
|---|---|---|
| POST | `/api/weather/query` | Query weather (device) |
| POST | `/api/weather/push` | Push weather to device |
| GET | `/api/weather/current/{mac}` | Get cached weather |

### Health
| Method | Path | Description |
|---|---|---|
| GET | `/api/health` | Health check |

## MQTT Topic Convention

| Topic | Direction | Description |
|---|---|---|
| `clock/{mac}/status` | Device → Server | Online/offline + device info |
| `clock/{mac}/config` | Server → Device | Card config push (versioned) |
| `clock/{mac}/config_report` | Device → Server | Local config report |
| `clock/{mac}/ota` | Server → Device | OTA update URL + version |
| `clock/{mac}/ota_status` | Device → Server | OTA apply result |
| `clock/{mac}/weather` | Server → Device | Weather data push |

## Card Configuration JSON

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

### Validation Rules
- Maximum 9 cards per device
- `water_ripple`, `game_of_life`, `hourglass` backgrounds reject foreground elements
- `fire` background restricts foreground fonts to `3x5`
- Valid font sizes: `3x5`, `5x5`, `7x9`, `long`
- Maximum 5 foreground slots per card

## Button Behavior

| Gesture | Action |
|---|---|
| Single click | Toggle display on/off |
| Double click | Next card (circular rotation) |
| Long press | Adjust brightness (cycle 0→127, reverse on release) |

## Testing

### Backend Integration Tests

```bash
cd backend
pip install httpx pytest pytest-asyncio
pytest ../tests/ -v
```

### Stability Monitor (24h+)

```bash
pip install paho-mqtt
python firmware/tests/stability_monitor.py \
  --broker localhost \
  --mac aa:bb:cc:dd:ee:ff \
  --duration 86400 \
  --interval 60
```

Monitors heap free, FPS, reconnect count, and connection state. Outputs CSV logs and JSON report to `firmware/tests/stability_logs/`.

### Firmware Unit Tests

```bash
cd firmware
idf.py build
# Unity tests run on-device via idf.py monitor
```

## Configuration

### Backend Environment Variables

| Variable | Default | Description |
|---|---|---|
| `DATABASE_URL` | `postgresql+asyncpg://...` | PostgreSQL connection string |
| `SECRET_KEY` | (required) | Session encryption key |
| `SESSION_EXPIRE_HOURS` | `24` | Session TTL |
| `QWEATHER_API_KEY` | (optional) | QWeather API key for weather |
| `MQTT_BROKER` | `localhost` | MQTT broker host |
| `MQTT_PORT` | `1883` | MQTT broker port |
| `ADMIN_DEFAULT_USERNAME` | `admin` | Default admin username |
| `ADMIN_DEFAULT_PASSWORD` | `admin123` | Default admin password |

### Firmware SDK Config

Key ESP-IDF settings (see `firmware/sdkconfig.defaults`):
- PSRAM enabled with `SPIRAM_MALLOC_ALWAYSINTERNAL=16384`
- Custom partition table with dual OTA slots
- WiFi STA + SoftAP
- OTA over HTTP enabled
- MQTT 3.1.1 protocol
- mbedTLS certificate bundle
- Bootloader rollback support

## OTA Update Flow

1. Admin uploads firmware via `/api/admin/firmware/upload`
2. Server publishes OTA URL to `clock/{mac}/ota`
3. Device downloads firmware to inactive OTA slot
4. Device verifies SHA-256 checksum
5. Device reports result to `/api/device/ota_status`
6. On next boot, bootloader validates new slot
7. If validation fails → automatic rollback to previous slot

## Known Limitations

- **ESP-IDF v5.5 required**: Firmware compilation requires the ESP-IDF v5.5 toolchain. Run `idf.py set-target esp32s3 && idf.py build` in the `firmware/` directory.
- **`TIMEZONEDB_API_KEY`**: The timezone lookup requires a free API key from [timezonedb.com](https://timezonedb.com). Replace `YOUR_KEY` in `firmware/components/geo/geo_pipeline.c:10`.
- **`QWEATHER_API_KEY`**: Weather display requires a free API key from [HeFeng/QWeather](https://dev.qweather.com). Set in `backend/.env` as `QWEATHER_API_KEY`.
- **`MAX_GCC_VALUE`**: The safe LED brightness limit needs hardware measurement. Run the power budget measurement (Wave 0.1) to set the correct value in `power_limit.h`.
- **Audio (Phase 2)**: Real-time audio reception is planned for a future phase. OTA hooks for audio task cleanup are in place.
- **Backend requires PostgreSQL + Mosquitto**: Use `docker-compose up -d` in `backend/` to start dependencies.
- **ESP-IDF build not verified**: The firmware has not been compiled — this environment lacks the ESP-IDF toolchain. Frontend build verified (`npm run build` passes with 0 errors).

## Test Coverage

| Component | Tests | File |
|-----------|-------|------|
| framebuffer | 4 | `firmware/components/display/test/test_framebuffer.c` |
| IS31FL3729 | 16 | `firmware/components/led_matrix/test/test_is31fl3729.c` |
| card_mgr | 10 | `firmware/components/cards/test/test_card_mgr.c` |
| font_renderer | 6 | `firmware/components/display/test/test_font_renderer.c` |
| geo_pipeline | 4 | `firmware/components/geo/test/test_geo_pipeline.c` |
| config_sync | 5 | `firmware/components/config/test/test_config_sync.c` |
| backend errors | 20 | `tests/test_error_scenarios.py` |
| backend E2E | 6 | `tests/integration/test_e2e.py` |
| backend OTA | 4 | `tests/test_firmware_ota.py` |
| **Total** | **75** | **9 test files** |

## Architecture Notes

- **Display-first**: The display refresh task runs at FreeRTOS priority 5 (highest). Network, I2C sensor, and MQTT operations never block the display.
- **No RTC battery**: Each power cycle performs full WiFi → IP geo → timezone → NTP sequence (~30s).
- **TCA9548A RESET**: Hardware pull-up — no GPIO control needed in firmware.
- **Power limit**: `MAX_GCC_VALUE` in `power_limit.h` caps LED brightness to prevent USB overcurrent.

## License

MIT
