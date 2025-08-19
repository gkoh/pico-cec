#ifndef CEC_CONFIG_H
#define CEC_CONFIG_H

#include <stdint.h>

// Vanilla FreeRTOS specifies stack sizes in number of words whilst
// XTENSA FreeRTOS uses number of bytes
#ifndef STACK_WORDSIZE
#define STACK_WORDSIZE 1
// Needs better name, currently inverted meaning - STACK_BYTES? STACK_WORDBYTES??
// Whatever, it needs to equal one for pico, and four with the esp32 port
// Currently defined in ./main/CMakeLists.txt for the esp32 port
#endif

#define PICO_CEC_VENDOR_ID 0x0010FA

// NOTE: moved from config.h
#define CEC_TASK_NAME "cec-driver"
#define LOG_TASK_NAME "cec-logger"
#define CEC_PRIORITY (configMAX_PRIORITIES - 2)
#define LOG_PRIORITY (configMAX_PRIORITIES - 5)
#define CEC_STACK_SIZE (1024 * STACK_WORDSIZE)
#define LOG_STACK_SIZE (1024 * STACK_WORDSIZE)

// TODO: perhaps move this to the only place it is used -> console.c
//       or keep it here as documentation for to possible values of device_type configuration
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

  /** CEC logical address, 0 through 15 */
  uint8_t logical_address;

  /** CEC device type. */
  uint8_t device_type;

  /** Use CEC monitor mode. (bus analyser) */
  uint8_t monitor_mode;

  /** Save our dynamically allocated logical address for first attempt on restart */
  uint16_t allocated_laddr;

  /** Device vendor id for broadcasting to the cec bus */
  uint16_t vendor_id;

  /** Device gpio pin to which CEC bus is connected */
  unsigned int gpio_pin;

} cec_config_t;

void cec_config_set_default(cec_config_t *config);

#endif
