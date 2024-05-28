#ifndef HDMI_CEC_H
#define HDMI_CEC_H

#if 0
#include "task.h"
#else
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#endif

#define CEC_TASK_NAME "cec"
#define CECPIN 11
#define ACTIVE \
  1  // Set to 1 to active;y reply to CEC messages
     // Set to 0 for a passive mode (when another active CEC sink is present)

typedef struct {
  uint8_t *data;
  size_t len;
} hdmi_message_t;

typedef enum {
  HDMI_FRAME_STATE_START_LOW,
  HDMI_FRAME_STATE_START_HIGH,
  HDMI_FRAME_STATE_DATA_LOW,
  HDMI_FRAME_STATE_DATA_HIGH,
  HDMI_FRAME_STATE_EOM_LOW,
  HDMI_FRAME_STATE_EOM_HIGH,
  HDMI_FRAME_STATE_ACK_LOW,
  HDMI_FRAME_STATE_ACK_HIGH,
  HDMI_FRAME_STATE_SEND_ACK,
  HDMI_FRAME_STATE_END,
  HDMI_FRAME_STATE_ABORT
} hdmi_frame_state_t;

typedef struct {
  hdmi_message_t *message;
  unsigned int bit;
  unsigned int byte;
  uint64_t start;
  bool first;
  bool eom;
  uint8_t address;
  hdmi_frame_state_t state;
} hdmi_frame_t;

//extern TaskHandle_t xCECTask;

void cec_task(void *data);

#endif
