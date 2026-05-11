# Power Budget — Desktop Clock

## Measurement Worksheet

| Condition | USB 5V Current (A) | LED Power (W) | Notes |
|-----------|---------------------|---------------|-------|
| All LEDs OFF (GCC=0, PWM=0) | ___ A | ___ W | Baseline (ESP32 only) |
| Low brightness (GCC=5, PWM=128) | ___ A | ___ W | |
| Medium brightness (GCC=20, PWM=128) | ___ A | ___ W | Default safe |
| High brightness (GCC=40, PWM=255) | ___ A | ___ W | Hard limit |
| Max (GCC=127, PWM=255) | ___ A | ___ W | DANGER — brief test only |

## Instructions

1. Connect USB power meter between PSU and device
2. Flash test firmware that sets GCC/PWM to each level
3. Record current at each level
4. Calculate safe GCC limit where current < 2.0A (USB spec)
5. Set `MAX_GCC_VALUE` to 90% of that limit

## IS31FL3729 Registers Reference

| Register | Address | Description |
|----------|---------|-------------|
| CONFIG | 0xA0 | Configuration (PD, SD, SS1, SS2, Sync) |
| GCC | 0xA1 | Global Current Control (0-127) |
| PWM base | 0x01 | Per-channel PWM (burst write 0x01-0x60 for 96 channels) |

## LED Matrix Parameters

| Parameter | Value |
|-----------|-------|
| Matrix size | 48 × 16 = 768 LEDs |
| LED type | White LED (Vf ~3.0-3.4V) |
| Max per-LED current | 20mA |
| Driver efficiency (est.) | ~85% |
| USB power limit | 5V / 2A = 10W |
| ESP32-S3 consumption | ~0.5W |
| Available LED power | ~9.5W |
| Theoretical max draw | ~50.7W (GCC=127, all PWM=255) |
| Default GCC limit | 20 (~2.4A total) |
| Hard GCC limit | 40 (~4.8A total) |

## Formula

```
I_total ≈ (GCC / 127) × 768 × 20mA × (3.3V / 5V) / efficiency
        = (GCC / 127) × 15.36A × 0.66 / 0.85
        = (GCC / 127) × ~11.9A @ 5V

GCC_limit = (I_usb_limit × 127) / 11.9A
          = (2.0A × 127) / 11.9A
          ≈ 21
```
