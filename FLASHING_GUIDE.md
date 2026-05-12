# Desktop Clock ESP32-S3 Flashing and Hardware Verification Guide

Firmware version: 0.1.0 | Hardware revision: 0.1 | Target: ESP32-S3 (16MB Flash)

---

## 1. Prerequisites

### 1.1 ESP-IDF v5.5 Installation

Verify your ESP-IDF installation is present and matches the required version:

```bash
ls ~/.espressif/v5.5.4/esp-idf/
```

If the directory exists, source the export script and confirm:

```bash
. ~/.espressif/v5.5.4/esp-idf/export.sh
idf.py --version
```

The installation path `~/.espressif/v5.5.4/esp-idf/` is used throughout this guide. Adjust if your ESP-IDF is installed elsewhere.

### 1.2 USB Driver

| OS      | Driver Required                                     |
|---------|-----------------------------------------------------|
| Linux   | None (CDC-ACM kernel module, loaded automatically)  |
| macOS   | None (built-in CDC-ACM support)                     |
| Windows | [CP210x USB to UART Bridge VCP Drivers](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers) |

The ESP32-S3 on-board USB-to-UART bridge uses a CP2102 or CH343 chip. On Linux, verify the driver is loaded:

```bash
lsmod | grep cp210x
# or
lsmod | grep ch341
```

If nothing appears, the driver is built into the kernel (common on modern distributions).

### 1.3 Hardware Connection

1. Use a USB **data** cable (Type-C to Type-A or Type-C to Type-C). Charging-only cables will not work.
2. Connect the Type-C end to the ESP32-S3 board.
3. Connect the USB-A (or Type-C) end to your computer.

### 1.4 Verify Serial Port

After connecting, the board appears as a serial device.

**Linux:**

```bash
ls /dev/ttyUSB*
# Expected: /dev/ttyUSB0 (or /dev/ttyUSB1 if other devices present)

# Alternative check via kernel messages:
dmesg | grep -i "cp210x\|ch341\|ttyUSB" | tail -5
```

**macOS:**

```bash
ls /dev/cu.*
# Expected: /dev/cu.usbserial-XXXX or /dev/cu.SLAB_USBtoUART
```

**Windows:**

Check Device Manager under "Ports (COM & LPT)" for "Silicon Labs CP210x USB to UART Bridge (COMx)". Note the COM port number (e.g., COM3).

If no device appears:
- Verify the USB cable is a data cable (test with another device).
- Try a different USB port on your computer.
- On Windows, install the CP210x driver linked in section 1.2.

---

## 2. First Flash (Factory Firmware)

### 2.1 Set Target and Build

```bash
# Navigate to firmware directory
cd ~/260510_clock/firmware

# Source ESP-IDF environment
. ~/.espressif/v5.5.4/esp-idf/export.sh

# Set target chip (one-time, creates sdkconfig)
idf.py set-target esp32s3

# Build the firmware
idf.py build
```

Explanation:
- `set-target esp32s3` writes `CONFIG_IDF_TARGET_ESP32S3=y` into sdkconfig. The sdkconfig.defaults file already specifies this and other settings (PSRAM, 16MB flash, custom partition table, OTA, WiFi, MQTT, certificate bundle, SPIFFS), so most configuration is pre-applied.
- `idf.py build` compiles all components: main firmware (app_main.c, render_task.c) plus 17 component libraries (I2C mux, LED matrix drivers, display, WiFi manager, NTP, MQTT, cards, thin mode, OTA, etc.).

### 2.2 Flash and Monitor

```bash
# Replace /dev/ttyUSB0 with your actual port
idf.py -p /dev/ttyUSB0 flash monitor
```

This single command:
1. Erases and writes the bootloader, partition table, and factory app to flash.
2. Opens a serial monitor at 115200 baud (ESP-IDF default).

**Baud rate:** 115200 (set by ESP-IDF monitor, not configurable via this command).

**To exit the monitor:** Press `Ctrl+]` (Control + right bracket). On some keyboards, this is `Ctrl` + `]` (the key to the right of `P`).

If the flash step fails with a connection error:

1. Hold the **BOOT** button on the ESP32-S3 board.
2. While holding BOOT, press and release the **RST** (EN) button.
3. Release BOOT.
4. The chip enters download mode. Re-run the flash command.

Alternatively, on some boards, holding BOOT during the entire flash process (until you see "Writing at 0x...") achieves the same result.

### 2.3 First Boot Sequence

On first boot, the firmware goes through these steps (visible on the serial monitor):

1. NVS flash initialization (auto-erase if corrupted).
2. MAC address read from eFuse.
3. I2C bus initialization (GPIO1=SCL, GPIO2=SDA, 400kHz).
4. SDB pin (GPIO8) driven LOW, then LED drivers initialized through TCA9548A mux (6 channels).
5. Display framebuffer allocated, display task created (priority 5).
6. Render task created (priority 4).
7. Card manager loads saved configuration from NVS.
8. WiFi manager starts. Since no credentials are saved, it enters **AP mode**.
9. The board creates a WiFi access point named `Clock-Config-XXXX` (where XXXX is derived from the MAC address).
10. The main loop waits for button events.

If WiFi credentials were previously saved and the stored network is reachable, the firmware will instead:
- Connect to WiFi in STA mode.
- Perform geo-location via IP.
- Sync time via NTP.
- Connect to MQTT broker at the configured backend URL.
- Begin displaying the clock face.

---

## 3. I2C Device Scan (Hardware Verification)

The I2C scan firmware tests signal integrity on the main I2C bus and through all 6 channels of the TCA9548A multiplexer.

### 3.1 Modify CMakeLists.txt

Edit `firmware/main/CMakeLists.txt`. Change the `SRCS` line to include only `i2c_scan.c`:

```cmake
idf_component_register(
    SRCS "i2c_scan.c"
    INCLUDE_DIRS "."
    REQUIRES config log shared display led_matrix i2c_mux sensors button
              wifi_mgr http_client ntp geo mqtt_client cards thin_mode ota version
)
```

The original line reads:
```cmake
    SRCS "app_main.c" "gpio_test.c" "i2c_scan.c" "render_task.c"
```

The `REQUIRES` line can remain unchanged. The i2c_scan.c firmware source does not use these component libraries, but leaving them causes no harm (UNUSED symbols are simply not linked).

### 3.2 Build and Flash

```bash
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

### 3.3 Expected Output

The scan runs 4 tests sequentially:

```
========================================
  I2C Bus Verification - Desktop Clock
========================================

--- Test 1: SDB=LOW (all LED drivers disabled) ---

=== Main Bus (SDB=LOW) ===
  Found device at 0x38 [AHT21]
  Found device at 0x70 [TCA9548A]
  Total: 2 device(s)

--- Test 2: SDB=HIGH (all LED drivers enabled) ---

=== Main Bus (SDB=HIGH) ===
  Found device at 0x38 [AHT21]
  Found device at 0x70 [TCA9548A]
  Total: 2 device(s)

--- Test 3: Through TCA9548A channels (ch0-ch5) ---

=== TCA9548A Channel 0 ===
  Found device at 0x34 [IS31FL3729]
  Total: 1 device(s)

=== TCA9548A Channel 1 ===
  Found device at 0x34 [IS31FL3729]
  Total: 1 device(s)

... (channels 2 through 5, each showing 1 device at 0x34)

--- Test 4: 400kHz I2C scan (first 2 channels) ---

=== Channel 0 @ 400kHz ===
  Found device at 0x34 [IS31FL3729]
  Total: 1 device(s)

=== Channel 1 @ 400kHz ===
  Found device at 0x34 [IS31FL3729]
  Total: 1 device(s)

========================================
  Verification Complete
========================================
```

**Summary of expected devices:**

| Device       | I2C Address | Location             |
|--------------|-------------|----------------------|
| AHT21        | 0x38        | Main bus (direct)    |
| TCA9548A     | 0x70        | Main bus (direct)    |
| IS31FL3729   | 0x34        | Mux channel 0        |
| IS31FL3729   | 0x34        | Mux channel 1        |
| IS31FL3729   | 0x34        | Mux channel 2        |
| IS31FL3729   | 0x34        | Mux channel 3        |
| IS31FL3729   | 0x34        | Mux channel 4        |
| IS31FL3729   | 0x34        | Mux channel 5        |

Total: **8 devices**.

### 3.4 Troubleshooting I2C Scan

**Only 2 devices found (0x38 and 0x70, no channel devices):**
- Check that SDB (GPIO8) is pulled high. The scan sets SDB=HIGH before channel tests. Verify with a multimeter that GPIO8 measures ~3.3V.
- Check the physical connection between GPIO8 and the SDB pins of all 6 IS31FL3729 chips.

**No devices found at all:**
- Verify I2C pull-up resistors are present on SDA (GPIO2) and SCL (GPIO1). Each line should have a 2.2k to 4.7k pull-up to 3.3V.
- Check for shorts or cold solder joints on the I2C bus.
- Verify the ESP32-S3 is powered correctly (3.3V rail stable).

**Fewer than 6 channel devices:**
- Check the TCA9548A mux wiring. Each channel (SD0-SD5, pins 4-9) should connect to the SDA of one IS31FL3729. Each channel (SC0-SC5, pins 14-19) should connect to the SCL of one IS31FL3729.
- Verify the address pins of each IS31FL3729: AD0 and AD1 must both be tied to GND for address 0x34.

**400kHz scan fails but 100kHz passes:**
- Indicates marginal signal integrity. Check pull-up resistor values. For 400kHz, 2.2k pull-ups are recommended.
- Check for excessive bus capacitance (long wires, many devices).

### 3.5 Restore CMakeLists.txt

After verification, restore the original SRCS line:

```cmake
    SRCS "app_main.c" "gpio_test.c" "i2c_scan.c" "render_task.c"
```

---

## 4. GPIO Test (Pin Verification)

The GPIO test firmware cycles each output pin HIGH then LOW, allowing voltage verification with a multimeter.

### 4.1 Modify CMakeLists.txt

Change SRCS to include only `gpio_test.c`:

```cmake
idf_component_register(
    SRCS "gpio_test.c"
    ...
)
```

### 4.2 Build and Flash

```bash
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

### 4.3 Test Procedure

The firmware cycles through each pin in an infinite loop. Each pin is driven HIGH for 1 second, then LOW for 1 second.

**Serial output:**

```
I (xxx) gpio_test: === Test Cycle 1 ===
I (xxx) gpio_test: Button state: released
I (xxx) gpio_test: I2C_SCL (GPIO1): skipping (I2C peripheral)
I (xxx) gpio_test: I2C_SDA (GPIO2): skipping (I2C peripheral)
I (xxx) gpio_test: >>> SETTING SDB (IS31FL3729 Shutdown) (GPIO8) HIGH <<<
I (xxx) gpio_test: >>> SETTING SDB (IS31FL3729 Shutdown) (GPIO8) LOW <<<
I (xxx) gpio_test: >>> SETTING LED (Status LED) (GPIO4) HIGH <<<
I (xxx) gpio_test: >>> SETTING LED (Status LED) (GPIO4) LOW <<<
I (xxx) gpio_test: >>> SETTING I2S_WS (Audio Word Select) (GPIO15) HIGH <<<
I (xxx) gpio_test: >>> SETTING I2S_WS (Audio Word Select) (GPIO15) LOW <<<
I (xxx) gpio_test: >>> SETTING I2S_CLK (Audio Bit Clock) (GPIO16) HIGH <<<
I (xxx) gpio_test: >>> SETTING I2S_CLK (Audio Bit Clock) (GPIO16) LOW <<<
I (xxx) gpio_test: >>> SETTING I2S_MICDATA (Audio Data) (GPIO18) HIGH <<<
I (xxx) gpio_test: >>> SETTING I2S_MICDATA (Audio Data) (GPIO18) LOW <<<
```

**Verified pins:**

| GPIO | Signal          | Expected Behavior                                    |
|------|-----------------|------------------------------------------------------|
| 8    | SDB             | HIGH=~3.3V, LOW=~0V (enables/disables LED drivers)   |
| 4    | STATUS LED      | HIGH=LED on, LOW=LED off                             |
| 15   | I2S_WS          | HIGH=~3.3V, LOW=~0V                                  |
| 16   | I2S_CLK         | HIGH=~3.3V, LOW=~0V                                  |
| 18   | I2S_MICDATA     | HIGH=~3.3V, LOW=~0V                                  |
| 38   | BUTTON          | Input with pull-down; HIGH when pressed              |

**Skipped pins:**
- GPIO1 (I2C_SCL) and GPIO2 (I2C_SDA): These require the I2C peripheral to drive; they are not tested as GPIO outputs.
- GPIO43 (TXD0) and GPIO44 (RXD0): UART0 debug console; driven by the USB-to-UART bridge, not tested.

### 4.4 Verification Steps

1. Place a multimeter in DC voltage mode.
2. Connect the black probe to GND.
3. For each pin listed above, touch the red probe to the pin while watching the serial output.
4. Confirm the voltage matches the expected HIGH (~3.3V) and LOW (~0V) states.
5. For the button (GPIO38): press the physical button and observe the serial output change from "released" to "PRESSED".

### 4.5 Restore CMakeLists.txt

Restore the original SRCS line after testing.

---

## 5. Normal Firmware Flash

### 5.1 Restore Standard Configuration

Ensure `firmware/main/CMakeLists.txt` contains the original SRCS line:

```cmake
    SRCS "app_main.c" "gpio_test.c" "i2c_scan.c" "render_task.c"
```

### 5.2 Build and Flash

```bash
cd ~/260510_clock/firmware
. ~/.espressif/v5.5.4/esp-idf/export.sh
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

### 5.3 First-Time Setup (AP Mode)

On first boot with no saved WiFi credentials:

1. The serial monitor shows `WiFi manager started` followed by AP creation.
2. On your phone or computer, scan for WiFi networks. Find `Clock-Config-XXXX`.
3. Connect to this access point. No password is required.
4. Open a browser and navigate to `http://192.168.4.1`.
5. The configuration page allows you to:
   - Select your home WiFi network (SSID).
   - Enter the WiFi password.
   - Configure the MQTT backend URL (default: `mqtt://192.168.1.100:1883`).
6. Submit the configuration.
7. The ESP32-S3 reboots and attempts to connect to your WiFi.

### 5.4 Subsequent Boots

After successful WiFi configuration:

1. The firmware reads saved credentials from NVS.
2. WiFi connects in STA mode.
3. IP-based geolocation determines the timezone.
4. NTP synchronizes the system clock.
5. The display shows the current time.
6. MQTT connects to the configured backend for remote control.

**Note:** The WiFi connection supports 2.4GHz networks only. 5GHz networks are not supported by the ESP32-S3.

---

## 6. Hardware Integration Test

A comprehensive hardware test firmware (`hw_test_all.c`) must be created that sequentially tests all subsystems.

### 6.1 Create the Test File

Create `firmware/main/hw_test_all.c` with the following structure:

```c
/**
 * Hardware Integration Test - Desktop Clock
 * Tests all subsystems in sequence, requiring user confirmation at each step.
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "pinmap.h"

static const char *TAG = "hw_test";

void app_main(void)
{
    printf("\n========================================\n");
    printf("  Hardware Integration Test\n");
    printf("========================================\n\n");

    // Test 1: Status LED
    printf("[1/7] Status LED (GPIO4)\n");
    printf("      The on-board LED should blink 3 times.\n");
    printf("      Confirm: LED blinks? (y/n, then press Enter)\n");
    // ... blink LED 3 times ...

    // Test 2: I2C Bus Scan
    printf("[2/7] I2C Bus Scan\n");
    printf("      Scanning main bus + 6 mux channels.\n");
    printf("      Expected: 8 devices (1xAHT21 + 1xTCA9548A + 6xIS31FL3729)\n");
    // ... run I2C scan from i2c_scan.c logic ...

    // Test 3: Button
    printf("[3/7] Button (GPIO38)\n");
    printf("      Press the button now.\n");
    printf("      Confirm: button press detected? (y/n, then press Enter)\n");
    // ... wait for button press ...

    // Test 4: LED Matrix All-On
    printf("[4/7] LED Matrix - All LEDs ON\n");
    printf("      All LED matrices should light up at full brightness.\n");
    printf("      Confirm: all matrices lit? (y/n, then press Enter)\n");
    // ... initialize LED drivers, set all LEDs on ...

    // Test 5: LED Matrix Ramp
    printf("[5/7] LED Matrix - Brightness Ramp\n");
    printf("      LEDs should smoothly ramp from 0 to 100%% and back.\n");
    printf("      Confirm: smooth ramp visible? (y/n, then press Enter)\n");

    // Test 6: LED Matrix Test Pattern
    printf("[6/7] LED Matrix - Test Pattern\n");
    printf("      Each matrix should display a unique test pattern.\n");
    printf("      Confirm: patterns visible on all matrices? (y/n, then press Enter)\n");

    // Test 7: AHT21 Sensor
    printf("[7/7] AHT21 Temperature/Humidity Sensor\n");
    printf("      Reading sensor values...\n");
    printf("      Temperature: XX.X C  Humidity: XX.X %%\n");
    printf("      Confirm: values are reasonable? (y/n, then press Enter)\n");

    printf("\n========================================\n");
    printf("  ALL TESTS COMPLETE\n");
    printf("========================================\n");
}
```

**Note:** This is a skeleton. The full implementation requires integrating the LED driver initialization, I2C scan logic, AHT21 read, and button detection from the existing component libraries. This file does not yet exist in the repository.

### 6.2 Build and Flash

```bash
# Edit CMakeLists.txt: SRCS "hw_test_all.c"
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

### 6.3 Run Through Tests

Follow each prompt. After each test step, visually confirm the result and respond (via serial input if implemented, or by observation if the test is output-only).

---

## 7. Troubleshooting

### 7.1 No Serial Output

- **Verify baud rate:** The ESP-IDF monitor uses 115200 by default. Ensure your terminal is set to 115200 if using a separate serial program.
- **Check USB cable:** The cable must be a data cable, not a charge-only cable. Test by checking if `dmesg` (Linux) or Device Manager (Windows) shows a new device when connected.
- **Check port:** Verify you are using the correct port. On Linux, `ls /dev/ttyUSB*` before and after connecting. The new entry is your device.
- **Verify device is powered:** The on-board power LED should be lit. If not, check the USB connection and try a different port.
- **Try manual serial terminal:** Use `screen`, `minicom`, or `picocom` directly:
  ```bash
  screen /dev/ttyUSB0 115200
  # Exit screen: Ctrl+A, then K, then Y
  ```

### 7.2 I2C Scan Shows No Devices

- **SDB pin:** Measure GPIO8 voltage. It must be ~3.3V during channel scanning. If the pin is stuck LOW, the IS31FL3729 drivers remain in shutdown and do not respond on I2C.
- **Pull-up resistors:** Verify 2.2k to 4.7k pull-up resistors are installed on SDA (GPIO2) and SCL (GPIO1) to 3.3V. Without pull-ups, the I2C bus cannot function.
- **Power:** Verify the 3.3V rail is stable. I2C devices require clean power.
- **Solder joints:** Inspect all I2C device solder joints under magnification. A single cold joint on SDA or SCL can disable the entire bus.

### 7.3 LED Matrix Not Lighting

- **GCC (Global Current Control):** The IS31FL3729 power-on default may set GCC to a very low value, making LEDs appear off. The initialization sequence in `is31fl3729_init()` sets GCC to a visible level.
- **SDB pin:** The SDB pin (GPIO8) must be HIGH to enable the LED drivers. If LOW, all drivers are in shutdown mode and all LED outputs are high-impedance.
- **3729 initialization:** Verify that `is31fl3729_init()` is called for each of the 6 channels during startup. The app_main.c init loop iterates channels 0 through 5.
- **TCA9548A mux:** If the mux is not initializing correctly, no channel selection occurs and the IS31FL3729 drivers cannot be addressed. Verify TCA9548A appears at 0x70 during I2C scan.

### 7.4 WiFi Connection Failure

- **2.4GHz only:** The ESP32-S3 supports 2.4GHz WiFi only. 5GHz networks are not visible to the device.
- **SSID contains special characters:** Some special characters or spaces in SSIDs may cause connection issues. Test with a simple ASCII SSID first.
- **Signal strength:** If the device is inside a metal enclosure or far from the access point, signal may be insufficient. Test close to the router first.
- **Saved credentials corruption:** If stored WiFi credentials become corrupted, erase NVS:
  ```bash
  idf.py -p /dev/ttyUSB0 erase-flash
  idf.py -p /dev/ttyUSB0 flash monitor
  ```
  This clears all saved settings, forcing a fresh AP mode setup on next boot.

### 7.5 Flash Failure (Download Mode)

If `idf.py flash` fails with a connection error or timeout:

1. **Manual bootloader entry:**
   - Hold the **BOOT** button (labeled "BOOT" or "IO0" on most ESP32-S3 boards).
   - While holding BOOT, press and release the **RST** (or "EN") button.
   - Release the BOOT button after 1-2 seconds.
   - The chip now listens for a firmware download.
   - Re-run `idf.py -p /dev/ttyUSB0 flash`.
2. **Alternative:** Hold BOOT throughout the entire flash process. Release only after you see progress output ("Writing at 0x...").
3. **Check USB cable:** Replace with a known-good data cable. Some cables are power-only.
4. **Check port permissions (Linux):** Ensure your user is in the `dialout` group:
   ```bash
   sudo usermod -a -G dialout $USER
   # Log out and back in for the change to take effect
   ```

### 7.6 OTA Rollback

If an OTA update fails, the bootloader's rollback feature reverts to the previous working firmware. The serial output during boot shows `ota_check_rollback()` if a rollback occurred. This is normal and indicates the system is self-recovering.

---

## 8. Pin Reference

All GPIO assignments from `firmware/components/config/include/pinmap.h`:

### I2C Bus

| Signal | GPIO | Notes                                    |
|--------|------|------------------------------------------|
| SCL    | 1    | I2C clock, 2.2k-4.7k pull-up to 3.3V    |
| SDA    | 2    | I2C data, 2.2k-4.7k pull-up to 3.3V     |

### LED Matrix Control

| Signal | GPIO | Notes                                              |
|--------|------|----------------------------------------------------|
| SDB    | 8    | Shutdown for all 6 IS31FL3729 drivers (active HIGH) |

### Status Indicator

| Signal      | GPIO | Notes                        |
|-------------|------|------------------------------|
| STATUS LED  | 4    | On-board indicator LED       |

### User Input

| Signal | GPIO | Notes                                |
|--------|------|--------------------------------------|
| BUTTON | 38   | Active HIGH, external pull-down      |

### I2S Audio (Phase 2)

| Signal      | GPIO | Notes          |
|-------------|------|----------------|
| I2S_WS      | 15   | Word select    |
| I2S_CLK     | 16   | Bit clock      |
| I2S_MICDATA | 18   | Microphone data|

### USB (Native)

| Signal | GPIO | Notes                                      |
|--------|------|--------------------------------------------|
| D-     | 19   | USB OTG data minus (not used as GPIO)       |
| D+     | 20   | USB OTG data plus (not used as GPIO)        |

### UART0 (Debug Console)

| Signal | GPIO | Notes                                      |
|--------|------|--------------------------------------------|
| TXD0   | 43   | UART transmit (connected to USB bridge TX)  |
| RXD0   | 44   | UART receive (connected to USB bridge RX)   |

### Default Firmware Parameters

| Parameter              | Value                         | Source            |
|------------------------|-------------------------------|-------------------|
| Firmware version       | 0.1.0                         | app_main.c:31     |
| Hardware revision      | 0.1                           | app_main.c:32     |
| I2C frequency          | 400 kHz                       | app_main.c:30     |
| I2C port               | I2C_NUM_0                     | app_main.c:29     |
| Flash size             | 16 MB                         | sdkconfig.defaults:35-36 |
| Factory partition      | 3 MB (0x300000)               | partitions.csv:5  |
| OTA partitions (x2)    | 3 MB each                     | partitions.csv:6-7 |
| SPIFFS storage         | 2 MB (0x200000)               | partitions.csv:9  |
| FreeRTOS tick rate     | 1000 Hz                       | sdkconfig.defaults:34 |
| Default MQTT broker    | mqtt://192.168.1.100:1883     | app_main.c:33     |
| WiFi AP SSID pattern   | Clock-Config-XXXX             | wifi_mgr component|

### Partition Layout

```
Offset     Size       Name        Type     SubType
0x9000     0x6000     nvs         data     nvs
0xf000     0x2000     otadata     data     ota
0x11000    0x1000     phy_init    data     phy
0x20000    0x300000   factory     app      factory
(auto)     0x300000   ota_0       app      ota_0
(auto)     0x300000   ota_1       app      ota_1
(auto)     0x6000     nvs_user    data     nvs
(auto)     0x200000   storage     data     spiffs
```

---

## Quick Command Reference

```bash
# Enter project directory
cd ~/260510_clock/firmware

# Source ESP-IDF
. ~/.espressif/v5.5.4/esp-idf/export.sh

# Set target (one-time)
idf.py set-target esp32s3

# Build
idf.py build

# Flash and monitor
idf.py -p /dev/ttyUSB0 flash monitor

# Monitor only (no flash)
idf.py -p /dev/ttyUSB0 monitor

# Erase entire flash
idf.py -p /dev/ttyUSB0 erase-flash

# Clean build artifacts
idf.py fullclean

# Flash with manual download mode (hold BOOT, press RST, release BOOT)
idf.py -p /dev/ttyUSB0 flash

# Exit serial monitor
# Press: Ctrl + ]
```
