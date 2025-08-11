#ifndef CEC_CONFIG_H
#define CEC_CONFIG_H

#include <stdint.h>

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

  /** Use CEC monitor mode. (bus analyser) */
  uint8_t monitor_mode;

  /** Save our currently allocated logical address for first attempt on restart */
  uint16_t allocated_laddr;

} cec_config_t;

void cec_config_set_default(cec_config_t *config);

#endif
