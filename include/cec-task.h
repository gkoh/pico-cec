#ifndef CEC_TASK_H
#define CEC_TASK_H

#include <stdint.h>
#include "cec-frame.h"

#define CEC_TASK_NAME "cec"

uint16_t cec_get_physical_address(void);
uint8_t cec_get_logical_address(void);
bool cec_send_msg(const cec_message_t *msg);
void cec_task(void *param);

#endif
