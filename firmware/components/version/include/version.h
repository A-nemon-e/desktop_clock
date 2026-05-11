#pragma once

/**
 * Firmware Version & Hardware Revision Reporter
 *
 * Firmware version is injected at build time via -DFW_VERSION="x.y.z".
 * Hardware revision is auto-detected at runtime via I2C probe for TCA9548A.
 */

/**
 * Get the firmware version string.
 *
 * @return Compile-time version (e.g., "1.0.0")
 */
const char* get_fw_version(void);

/**
 * Auto-detect hardware revision.
 *
 * Probes I2C address 0x70 (TCA9548A mux):
 *   - If TCA9548A detected → "0.1"
 *   - Otherwise            → "0.2"
 *
 * @return Hardware revision string (e.g., "0.1")
 */
const char* get_hw_revision(void);
