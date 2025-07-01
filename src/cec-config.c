#include "class/hid/hid.h"
#include "tusb.h"

#include "cec-config.h"

/**
 * Official table of CEC User Control codes to human-readable names.
 *
 * From "High-Definition Multimedia Interface Specification Version 1.3, CEC
 * Table 23, User Control Codes, p. 62"
 *
 * @note Incomplete, add as required.
 */
const char *cec_user_control_name[UINT8_MAX] = {
    [CEC_USER_SELECT] = "Select",
    [CEC_USER_UP] = "Up",
    [CEC_USER_DOWN] = "Down",
    [CEC_USER_LEFT] = "Left",
    [CEC_USER_RIGHT] = "Right",
    [CEC_USER_RIGHT_UP] = "Right-Up",
    [CEC_USER_RIGHT_DOWN] = "Right-Down",
    [CEC_USER_LEFT_UP] = "Left-Up",
    [CEC_USER_LEFT_DOWN] = "Left-Down",
    [CEC_USER_OPTIONS] = "Options",
    [CEC_USER_EXIT] = "Exit",
    [CEC_USER_0] = "0",
    [CEC_USER_1] = "1",
    [CEC_USER_2] = "2",
    [CEC_USER_3] = "3",
    [CEC_USER_4] = "4",
    [CEC_USER_5] = "5",
    [CEC_USER_6] = "6",
    [CEC_USER_7] = "7",
    [CEC_USER_8] = "8",
    [CEC_USER_9] = "9",
    [CEC_USER_DISPLAY_INFO] = "Display Information",
    [CEC_USER_VOLUME_UP] = "Volume Up",
    [CEC_USER_VOLUME_DOWN] = "Volume Down",
    [CEC_USER_PLAY] = "Play",
    [CEC_USER_STOP] = "Stop",
    [CEC_USER_PAUSE] = "Pause",
    [CEC_USER_REWIND] = "Rewind",
    [CEC_USER_FAST_FWD] = "Fast Forward",
    [CEC_USER_SUB_PICTURE] = "Sub Picture",
    [CEC_USER_F1_BLUE] = "F1 (Blue)",
    [CEC_USER_F2_RED] = "F2 (Red)",
    [CEC_USER_F3_GREEN] = "F3 (Green)",
    [CEC_USER_F4_YELLOW] = "F4 (Yellow)",
    [CEC_USER_F5] = "F5",
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

/**
 * Default (Kodi) key mapping from HDMI user control to HID keyboard entry.
 */
static const uint8_t default_kodi_user_keymap[UINT8_MAX] = {
    [CEC_USER_SELECT] = HID_KEY_ENTER,
    [CEC_USER_UP] = HID_KEY_ARROW_UP,
    [CEC_USER_DOWN] = HID_KEY_ARROW_DOWN,
    [CEC_USER_LEFT] = HID_KEY_ARROW_LEFT,
    [CEC_USER_RIGHT] = HID_KEY_ARROW_RIGHT,
    [CEC_USER_OPTIONS] = HID_KEY_C,
    [CEC_USER_EXIT] = HID_KEY_BACKSPACE,
    [CEC_USER_0] = HID_KEY_0,
    [CEC_USER_1] = HID_KEY_1,
    [CEC_USER_2] = HID_KEY_2,
    [CEC_USER_3] = HID_KEY_3,
    [CEC_USER_4] = HID_KEY_4,
    [CEC_USER_5] = HID_KEY_5,
    [CEC_USER_6] = HID_KEY_6,
    [CEC_USER_7] = HID_KEY_7,
    [CEC_USER_8] = HID_KEY_8,
    [CEC_USER_9] = HID_KEY_9,
    [CEC_USER_DISPLAY_INFO] = HID_KEY_I,
    [CEC_USER_PLAY] = HID_KEY_P,
    [CEC_USER_STOP] = HID_KEY_X,
    [CEC_USER_PAUSE] = HID_KEY_SPACE,
    [CEC_USER_REWIND] = HID_KEY_R,
    [CEC_USER_FAST_FWD] = HID_KEY_F,
    [CEC_USER_SUB_PICTURE] = HID_KEY_L,
    0x00,
};

/**
 * Key mapping for MiSTer integration, from LaserBearIndustries.
 */
static const uint8_t default_mister_user_keymap[UINT8_MAX] = {
    [CEC_USER_SELECT] = HID_KEY_ENTER,
    [CEC_USER_UP] = HID_KEY_ARROW_UP,
    [CEC_USER_DOWN] = HID_KEY_ARROW_DOWN,
    [CEC_USER_LEFT] = HID_KEY_ARROW_LEFT,
    [CEC_USER_RIGHT] = HID_KEY_ARROW_RIGHT,
    [CEC_USER_OPTIONS] = HID_KEY_F12,
    [CEC_USER_EXIT] = HID_KEY_F12,
    [CEC_USER_0] = HID_KEY_0,
    [CEC_USER_1] = HID_KEY_1,
    [CEC_USER_2] = HID_KEY_2,
    [CEC_USER_3] = HID_KEY_3,
    [CEC_USER_4] = HID_KEY_4,
    [CEC_USER_5] = HID_KEY_5,
    [CEC_USER_6] = HID_KEY_6,
    [CEC_USER_7] = HID_KEY_7,
    [CEC_USER_8] = HID_KEY_8,
    [CEC_USER_9] = HID_KEY_9,
    [CEC_USER_DISPLAY_INFO] = HID_KEY_I,
    [CEC_USER_PLAY] = HID_KEY_F12,
    [CEC_USER_STOP] = HID_KEY_F12,
    [CEC_USER_PAUSE] = HID_KEY_F12,
    [CEC_USER_REWIND] = HID_KEY_F12,
    [CEC_USER_FAST_FWD] = HID_KEY_F12,
    [CEC_USER_SUB_PICTURE] = HID_KEY_L,
    0x00};

void cec_config_set_default(cec_config_t *config) {
  if (config == NULL) {
    return;
  }
  config->edid_delay_ms = default_edid_delay_ms;
  config->physical_address = default_physical_addr;
  config->logical_address = default_logical_addr;
  config->device_type = default_device_type;
#if KEYMAP_DEFAULT_KODI
  config->keymap_type = CEC_CONFIG_KEYMAP_KODI;
#elif KEYMAP_DEFAULT_MISTER
  config->keymap_type = CEC_CONFIG_KEYMAP_MISTER;
#else
#error "Unknown default keymap."
#endif
}

void cec_config_set_keymap(cec_config_t *config) {
  if (config == NULL) {
    return;
  }

  const uint8_t *default_keymap = NULL;

  switch (config->keymap_type) {
    case CEC_CONFIG_KEYMAP_CUSTOM:
      break;
    case CEC_CONFIG_KEYMAP_KODI:
      default_keymap = &default_kodi_user_keymap[0];
      break;
    case CEC_CONFIG_KEYMAP_MISTER:
      default_keymap = &default_mister_user_keymap[0];
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
