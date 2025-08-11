#include "portable.h"
DECLARE_TAG()

#include "cec-user.h"
#include "config-keymap.h"

/**
 * Default (Kodi) key mapping from CEC user control to HID keyboard entry.
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

void config_keymap_set_default(config_t *config) {
  if (config == NULL) {
    return;
  }
  cec_config_set_default(&config->cec);
#if KEYMAP_DEFAULT_KODI
  config->keymap_type = CEC_CONFIG_KEYMAP_KODI;
#elif KEYMAP_DEFAULT_MISTER
  config->keymap_type = CEC_CONFIG_KEYMAP_MISTER;
#else
#error "Unknown default keymap."
#endif
}

void config_keymap_set(config_t *config) {
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

  // set only the keys, keynames are finalised in config_keymap_complete()
  for (unsigned int i = 0; i < UINT8_MAX; i++) {
    config->keymap[i].key = default_keymap[i];
  }
}

void config_keymap_complete(config_t *config) {
  for (uint8_t i = 0; i < UINT8_MAX; i++) {
    if (config->keymap[i].key != 0x00) {
      const char *name = cec_user_control_name[i];
      config->keymap[i].name = name;
    }
  }
}
