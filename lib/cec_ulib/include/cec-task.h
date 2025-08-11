#ifndef CEC_TASK_H
#define CEC_TASK_H

#include <stdint.h>

#define CEC_TASK_NAME "cec"

bool cec_task_get_status(void);
uint16_t cec_get_physical_address(void);
uint8_t cec_get_logical_address(void);
void cec_task(void *param);

#endif
