#ifndef CEC_TASK_H
#define CEC_TASK_H

#include <stdint.h>

#include "cec-config.h"
#include "cec-log.h"

bool cec_get_status(void);
uint8_t cec_get_logical_address(void);
uint16_t cec_get_physical_address(void);

bool cec_read(uint8_t *user_control, int timeout_ticks);
bool cec_init(cec_config_t config, log_callback_t log_callback);
bool cec_gpio(unsigned int gpio);

#endif
