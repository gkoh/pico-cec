#ifndef BLINK_H

#include "FreeRTOS.h"
#include "task.h"

typedef enum {
  BLINK_STATE_BLUE_2HZ,
  BLINK_STATE_GREEN_2HZ,
  BLINK_STATE_RED_2HZ,
  BLINK_STATE_GREEN_ON,
  BLINK_STATE_OFF,
} blink_state_t;

extern TaskHandle_t xBlinkTask;

void blink_init(void);
void blink_task(void *param);
void blink_set(blink_state_t state);
void blink_set_blink(blink_state_t state);

#endif
