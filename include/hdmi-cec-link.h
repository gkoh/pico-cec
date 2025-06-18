#ifndef HDMI_LINK_H
#define HDMI_LINK_H

#include "FreeRTOS.h"
#include "task.h"

#ifndef CEC_PIN
#define CEC_PIN 3  // GPIO3 == D10 (Seeed Studio XIAO RP2040)
#endif

extern TaskHandle_t xCECTask;

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

typedef struct hdmi_frame_t {
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

void hdmi_link_init(void);
void cec_get_stats(hdmi_cec_stats_t *stats);
bool send_frame(uint8_t pldcnt, uint8_t *pld);
uint8_t recv_frame(uint8_t *pld, uint8_t address);

#endif
