#include <stddef.h>

#include "cec-config.h"

/**
 * Default EDID probe delay in milliseconds.
 *
 * Number of milliseconds to delay the EDID probe. The DDC bus is shared and a
 * delay can avoid access conflicts.
 */
static const uint32_t default_edid_delay_ms = 5000;

/**
 * Default monitor mode (on/off).
 */
static const uint8_t default_monitor_mode = 0;

/**
 * Default physical address.
 *
 * 0x0000 is typically reserved for the television and we never claim it.
 * Thus, use 0x0000 to indicate "auto-query over HDMI-DCD".
 */
static const uint16_t default_physical_addr = 0x0000;

/**
 * Default logical address.
 *
 * Valid values are 0x00 through to 0x0f.
 * 0x00 is the TV, 0x0f is unregistered, both are treated as 'auto-allocate'.
 * Anything else is treated as 'hardcoded'.
 */
static const uint8_t default_logical_addr = 0x0f;

/**
 * Default device type.
 *
 * One of "playback" or "recording" enumeration.
 */
static const uint8_t default_device_type = CEC_CONFIG_DEVICE_TYPE_PLAYBACK;

static const uint8_t default_allocated_laddr = 0x00;

void cec_config_set_default(cec_config_t *config) {
  if (config == NULL) {
    return;
  }
  config->edid_delay_ms = default_edid_delay_ms;
  config->monitor_mode = default_monitor_mode;
  config->physical_address = default_physical_addr;
  config->logical_address = default_logical_addr;
  config->device_type = default_device_type;
  config->allocated_laddr = default_allocated_laddr;
}
