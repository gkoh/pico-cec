#ifndef CEC_FRAME_H
#define CEC_FRAME_H

#include "FreeRTOS.h"
#include "task.h"

#ifndef CEC_PIN
#define CEC_PIN 3  // GPIO3 == D10 (Seeed Studio XIAO RP2040)
#endif

extern TaskHandle_t xCECTask;

typedef struct {
  uint8_t *data;
  uint8_t len;
} cec_message_t;

typedef enum {
  CEC_FRAME_STATE_START_LOW = 0,
  CEC_FRAME_STATE_START_HIGH = 1,
  CEC_FRAME_STATE_DATA_LOW = 2,
  CEC_FRAME_STATE_DATA_HIGH = 3,
  CEC_FRAME_STATE_EOM_LOW = 4,
  CEC_FRAME_STATE_EOM_HIGH = 5,
  CEC_FRAME_STATE_ACK_LOW = 6,
  CEC_FRAME_STATE_ACK_HIGH = 7,
  CEC_FRAME_STATE_ACK_WAIT = 8,
  CEC_FRAME_STATE_ACK_END = 9,
  CEC_FRAME_STATE_END = 10,
  CEC_FRAME_STATE_ABORT = 11
} cec_frame_state_t;

typedef struct cec_frame_t {
  cec_message_t *message;
  unsigned int bit;
  unsigned int byte;
  uint64_t start;
  bool first;
  bool eom;
  bool ack;
  uint8_t address;
  cec_frame_state_t state;
} cec_frame_t;

/* @todo need atomics for thread sync safety */
typedef struct {
  uint32_t rx_frames;
  uint32_t tx_frames;
  uint32_t rx_abort_frames;
  uint32_t tx_noack_frames;
} cec_frame_stats_t;

void cec_frame_init(void);
void cec_frame_get_stats(cec_frame_stats_t *stats);
bool cec_frame_send(uint8_t pldcnt, uint8_t *pld);
uint8_t cec_frame_recv(uint8_t *pld, uint8_t address);

#endif
