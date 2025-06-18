#ifndef HDMI_LINK_H
#define HDMI_LINK_H

/*
Minor refactoring of module hdmi-cec to encapsulate interrupt handling routines
in a single unit, and while at it separated out function log_cec_frame and its
supporting string table.

Choosen name for new module being hdmi-cec-link as its functionality somewhat
represents the link layer in the osi network model whilst still being clearly
related to hdmi-cec which it serves. Similar rationale with the log module.

The inherent modularity achieved as a result of this change will be apparent
via inspection of the respective header files.

This should also aid future porting to other devices, as it certainly helped me
with the effort towards supporting the esp family of targets. (using esp-idf)

As always with these things the pros'vs'cons are forever debatable, but this at
least reduces the size of the hdmi-cec module to a more manageable <400 lines.

hdmi-cec-link handles the cec packets (frames)
hdmi-cec handles the cec protocol (transactions)

Build tested for pi-pico and esp32, however not as yet passed a run-time check.

Further improvement may be possible.
*/

#include "FreeRTOS.h"
#include "task.h"

#ifndef CEC_PIN
//#define CEC_PIN 3  // GPIO3 == D10 (Seeed Studio XIAO RP2040)
#define CEC_PIN 4  // GPIO4 == D? (esp32)
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
