#ifndef CEC_CONFIG_H
#define CEC_CONFIG_H

#include <stdint.h>

typedef struct {
  const char *name;
  uint8_t key;
} command_t;

typedef enum {
  CEC_CONFIG_DEFAULT_KODI = 0,
} cec_config_default_t;

/**
 * CEC configuration in-memory.
 */
typedef struct {
  /** DDC EDID delay in milliseconds. */
  uint32_t edid_delay_ms;

  /** CEC physical address. */
  uint16_t physical_address;

  /** User Control key mapping table. */
  command_t keymap[UINT8_MAX];
} cec_config_t;

extern const char *cec_user_control_name[UINT8_MAX];

void cec_config_set_keymap(cec_config_default_t type, cec_config_t *config);
void cec_config_set_default(cec_config_t *config);

void cec_config_complete(cec_config_t *config);

#endif
