#include "cec-config.h"
#include "class/hid/hid.h"
#include "tusb.h"

/**
 * Official table of CEC User Control codes to human-readable names.
 *
 * From "High-Definition Multimedia Interface Specification Version 1.3, CEC
 * Table 23, User Control Codes, p. 62"
 *
 * @note Incomplete.
 */
const char *cec_user_control_name[UINT8_MAX] = {
    [0x00] = "Select",
    [0x01] = "Up",
    [0x02] = "Down",
    [0x03] = "Left",
    [0x04] = "Right",
    [0x05] = "Right-Up",
    [0x06] = "Right-Down",
    [0x07] = "Left-Up",
    [0x08] = "Left-Down",
    [0x0a] = "Options",
    [0x0d] = "Exit",
    [0x20] = "0",
    [0x21] = "1",
    [0x22] = "2",
    [0x23] = "3",
    [0x24] = "4",
    [0x25] = "5",
    [0x26] = "6",
    [0x27] = "7",
    [0x28] = "8",
    [0x29] = "9",
    [0x35] = "Display Information",
    [0x41] = "Volume Up",
    [0x42] = "Volume Down",
    [0x44] = "Play",
    [0x45] = "Stop",
    [0x46] = "Pause",
    [0x48] = "Rewind",
    [0x49] = "Fast Forward",
    [0x51] = "Sub Picture",
    [0x71] = "F1 (Blue)",
    [0x72] = "F2 (Red)",
    [0x73] = "F3 (Green)",
    [0x74] = "F4 (Yellow)",
    [0x75] = "F5",
    NULL,
};

/**
 * Default EDID probe delay in milliseconds.
 *
 * Number of milliseconds to delay the EDID probe. The DDC bus is shared and a
 * delay can avoid access conflicts.
 */
static const uint32_t default_edid_delay_ms = 5000;

/**
 * Default physical address.
 *
 * 0x0000 is typically reserved for the television and we never claim it.
 * Thus, use 0x0000 to indicate "auto-query over HDMI-DCD".
 */
static const uint16_t default_physical_addr = 0x0000;

/*
 * Default (Kodi) key mapping from HDMI user control to HID keyboard entry.
 */
static const uint8_t default_kodi_user_keymap[UINT8_MAX] = {
    [0x00] = HID_KEY_ENTER,
    [0x01] = HID_KEY_ARROW_UP,
    [0x02] = HID_KEY_ARROW_DOWN,
    [0x03] = HID_KEY_ARROW_LEFT,
    [0x04] = HID_KEY_ARROW_RIGHT,
    [0x0a] = HID_KEY_C,
    [0x0d] = HID_KEY_BACKSPACE,
    [0x20] = HID_KEY_0,
    [0x21] = HID_KEY_1,
    [0x22] = HID_KEY_2,
    [0x23] = HID_KEY_3,
    [0x24] = HID_KEY_4,
    [0x25] = HID_KEY_5,
    [0x26] = HID_KEY_6,
    [0x27] = HID_KEY_7,
    [0x28] = HID_KEY_8,
    [0x29] = HID_KEY_9,
    [0x35] = HID_KEY_I,
    [0x44] = HID_KEY_P,
    [0x45] = HID_KEY_X,
    [0x46] = HID_KEY_SPACE,
    [0x48] = HID_KEY_R,
    [0x49] = HID_KEY_F,
    [0x51] = HID_KEY_L,
    0x00,
};

void cec_config_set_default(cec_config_t *config) {
  if (config == NULL) {
    return;
  }
  config->edid_delay_ms = default_edid_delay_ms;
  config->physical_address = default_physical_addr;
}

void cec_config_set_keymap(cec_config_default_t type, cec_config_t *config) {
  if (config == NULL) {
    return;
  }

  const uint8_t *default_keymap = NULL;

  switch (type) {
    case CEC_CONFIG_DEFAULT_KODI:
      default_keymap = &default_kodi_user_keymap[0];
      break;
    default:
      return;
  }

  // set only the keys, keynames are finalised in cec_config_complete()
  for (unsigned int i = 0; i < UINT8_MAX; i++) {
    config->keymap[i].key = default_keymap[i];
  }
}

void cec_config_complete(cec_config_t *config) {
  for (uint8_t i = 0; i < UINT8_MAX; i++) {
    if (config->keymap[i].key != 0x00) {
      const char *name = cec_user_control_name[i];
      config->keymap[i].name = name;
    }
  }
}
