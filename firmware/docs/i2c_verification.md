# I2C Verification Worksheet

## Test Setup

| Parameter | Value |
|-----------|-------|
| SDA | GPIO 2 |
| SCL | GPIO 1 |
| SDB | GPIO 8 |
| TCA9548A Address | 0x70 (A0=A1=A2=GND) |
| AHT21 Address | 0x38 |
| IS31FL3729 Address | 0x34 (AD0=AD1=GND) |
| Mux Channels Used | 0-5 (6 channels) |

## Expected Device Map

| Bus | Address | Device | Expected | Found? |
|-----|---------|--------|----------|--------|
| Main | 0x38 | AHT21 | ✓ | ☐ |
| Main | 0x70 | TCA9548A | ✓ | ☐ |
| Mux Ch0 | 0x34 | IS31FL3729 #1 | ✓ | ☐ |
| Mux Ch1 | 0x34 | IS31FL3729 #2 | ✓ | ☐ |
| Mux Ch2 | 0x34 | IS31FL3729 #3 | ✓ | ☐ |
| Mux Ch3 | 0x34 | IS31FL3729 #4 | ✓ | ☐ |
| Mux Ch4 | 0x34 | IS31FL3729 #5 | ✓ | ☐ |
| Mux Ch5 | 0x34 | IS31FL3729 #6 | ✓ | ☐ |

## Test Sequence

### 1. SDB Control Verification

| Step | Action | Expected Result | Actual | Pass? |
|------|--------|-----------------|--------|-------|
| 1.1 | SDB=LOW, scan main bus | 0x38 + 0x70 only | | ☐ |
| 1.2 | SDB=HIGH, scan main bus | 0x38 + 0x70 only (mux isolates 0x34) | | ☐ |

### 2. Mux Channel Scan

| Step | Action | Expected Result | Actual | Pass? |
|------|--------|-----------------|--------|-------|
| 2.1 | Mux Ch0 scan @ 100kHz | 0x34 found | | ☐ |
| 2.2 | Mux Ch1 scan @ 100kHz | 0x34 found | | ☐ |
| 2.3 | Mux Ch2 scan @ 100kHz | 0x34 found | | ☐ |
| 2.4 | Mux Ch3 scan @ 100kHz | 0x34 found | | ☐ |
| 2.5 | Mux Ch4 scan @ 100kHz | 0x34 found | | ☐ |
| 2.6 | Mux Ch5 scan @ 100kHz | 0x34 found | | ☐ |

### 3. High-Speed Scan (400kHz)

| Step | Action | Expected Result | Actual | Pass? |
|------|--------|-----------------|--------|-------|
| 3.1 | Ch0 scan @ 400kHz | 0x34 found, no errors | | ☐ |
| 3.2 | Ch1 scan @ 400kHz | 0x34 found, no errors | | ☐ |

## Signal Integrity Checks

| Test | 100kHz | 400kHz | Notes |
|------|--------|--------|-------|
| All 8 devices found? | ☐ | ☐ | |
| SDB correctly isolates? | ☐ | ☐ | |
| Waveform clean? | ☐ | ☐ | Check rise time, no ringing on scope |
| Communication errors? | ☐ | ☐ | ZERO errors expected |
| Pull-up resistors adequate? | ☐ | ☐ | 4.7kΩ typical, verify edges |

## Selected I2C Frequency

**Final choice:** ______ kHz

**Reason:** ___________________________

## Results Summary

| Test | Pass/Fail | Notes |
|------|-----------|-------|
| SDB isolation | | |
| Mux channel scan | | |
| 400kHz stability | | |

**Verified by:** _________________ **Date:** _________________
