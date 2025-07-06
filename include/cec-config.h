#ifndef CEC_CONFIG_H
#define CEC_CONFIG_H

#include <stdint.h>

typedef struct {
  const char *name;
  uint8_t key;
} command_t;

typedef enum {
  CEC_CONFIG_KEYMAP_CUSTOM = 0,
  CEC_CONFIG_KEYMAP_KODI = 1,
  CEC_CONFIG_KEYMAP_MISTER = 2,
} cec_config_keymap_t;

typedef enum {
  CEC_CONFIG_DEVICE_TYPE_TV = 0,
  CEC_CONFIG_DEVICE_TYPE_RECORDING = 1,
  CEC_CONFIG_DEVICE_TYPE_RESERVED = 2,
  CEC_CONFIG_DEVICE_TYPE_TUNER = 3,
  CEC_CONFIG_DEVICE_TYPE_PLAYBACK = 4,
  CEC_CONFIG_DEVICE_TYPE_AUDIO_SYSTEM = 5,
} cec_config_device_type_t;

/**
 * CEC configuration in-memory.
 */
typedef struct {
  /** DDC EDID delay in milliseconds. */
  uint32_t edid_delay_ms;

  /** CEC physical address. */
  uint16_t physical_address;

  /** CEC logical address. */
  uint8_t logical_address;

  /** CEC device type. */
  uint8_t device_type;

  /** Keymap configuration. */
  cec_config_keymap_t keymap_type;

  /** User Control key mapping table. */
  command_t keymap[UINT8_MAX];
} cec_config_t;

void cec_config_set_keymap(cec_config_t *config);
void cec_config_set_default(cec_config_t *config);

void cec_config_complete(cec_config_t *config);

#endif
