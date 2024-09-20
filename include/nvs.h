#ifndef NVS_H
#define NVS_H

#include <stdbool.h>

#include "cec-config.h"

void nvs_load_config(cec_config_t *config);
bool nvs_save_config(const cec_config_t *config);

#endif
