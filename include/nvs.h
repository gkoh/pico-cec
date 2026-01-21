#ifndef NVS_H
#define NVS_H

#include <stdbool.h>

#include "config-keymap.h"

/** Read configuration from NVS. */
bool nvs_read_config(config_t *config);

/** Read and apply configuration from NVS. */
void nvs_load_config(config_t *config);

/** Save configuration to NVS. */
bool nvs_save_config(const config_t *config);

#endif
