#ifndef NVS_H
#define NVS_H

#include <stdbool.h>

#include "cec-config.h"

/** Read configuration from NVS. */
bool nvs_read_config(cec_config_t *config);

/** Read and apply configuration from NVS. */
void nvs_load_config(cec_config_t *config);

/** Save configuration to NVS. */
bool nvs_save_config(const cec_config_t *config);

#endif
