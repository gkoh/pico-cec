#ifndef CEC_TASK_H
#define CEC_TASK_H

#include <stdint.h>

#define CEC_TASK_NAME "cec"

uint16_t cec_get_physical_address(void);
uint8_t cec_get_logical_address(void);
bool cec_send_msg(uint8_t address, uint8_t opcode);
void cec_task(void *param);

#endif
