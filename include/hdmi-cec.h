#ifndef HDMI_CEC_H
#define HDMI_CEC_H

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>

#include "task.h"
#define CEC_TASK_NAME "cec"

#ifndef CEC_PIN
#define CEC_PIN 3  // GPIO3 == D10 (Seeed Studio XIAO RP2040)
#endif

typedef struct {
  uint8_t *data;
  uint8_t len;
} hdmi_message_t;

typedef enum {
  HDMI_FRAME_STATE_START_LOW = 0,
  HDMI_FRAME_STATE_START_HIGH = 1,
  HDMI_FRAME_STATE_DATA_LOW = 2,
  HDMI_FRAME_STATE_DATA_HIGH = 3,
  HDMI_FRAME_STATE_EOM_LOW = 4,
  HDMI_FRAME_STATE_EOM_HIGH = 5,
  HDMI_FRAME_STATE_ACK_LOW = 6,
  HDMI_FRAME_STATE_ACK_HIGH = 7,
  HDMI_FRAME_STATE_ACK_WAIT = 8,
  HDMI_FRAME_STATE_ACK_END = 9,
  HDMI_FRAME_STATE_END = 10,
  HDMI_FRAME_STATE_ABORT = 11
} hdmi_frame_state_t;

typedef struct {
  hdmi_message_t *message;
  unsigned int bit;
  unsigned int byte;
  uint64_t start;
  bool first;
  bool eom;
  bool ack;
  uint8_t address;
  hdmi_frame_state_t state;
} hdmi_frame_t;

/* @todo need atomics for thread sync safety */
typedef struct {
  uint32_t rx_frames;
  uint32_t tx_frames;
  uint32_t rx_abort_frames;
  uint32_t tx_noack_frames;
} hdmi_cec_stats_t;

extern TaskHandle_t xCECTask;

uint64_t cec_get_uptime_ms(void);
void cec_get_stats(hdmi_cec_stats_t *stats);
uint16_t cec_get_physical_address(void);
uint8_t cec_get_logical_address(void);
void cec_task(void *data);

#endif
