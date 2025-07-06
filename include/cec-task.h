#ifndef CEC_TASK_H
#define CEC_TASK_H

#include <stdint.h>

#define CEC_TASK_NAME "cec"

uint16_t cec_get_physical_address(void);
uint8_t cec_get_logical_address(void);
void cec_task(void *param);

#endif
