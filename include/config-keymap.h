#ifndef CONFIG_KEYMAP_H
#define CONFIG_KEYMAP_H

#include <stdint.h>

#include "cec-config.h"

typedef struct {
  const char *name;
  uint8_t key;
} command_t;

typedef enum {
  CONFIG_KEYMAP_CUSTOM = 0,
  CONFIG_KEYMAP_KODI = 1,
  CONFIG_KEYMAP_MISTER = 2,
} config_keymap_t;

/**
 * CEC with Keymap configuration in-memory.
 */
typedef struct {
  /** CEC configuration. */
  cec_config_t cec;

  /** Keymap configuration. */
  config_keymap_t keymap_type;

  /** User Control key mapping table. */
  command_t keymap[UINT8_MAX];
} config_t;

void config_keymap_set(config_t *config);
void config_keymap_set_default(config_t *config);
void config_keymap_complete(config_t *config);

#endif
