#include <stdio.h>
#include <string.h>

#include "cec-hal.h"
DECLARE_TAG()

#include "cec-frame.h"
#include "cec-util.h"

#define MAX_EDGE_TIME_CAPTURES 512
static uint32_t pulse_times[MAX_EDGE_TIME_CAPTURES];
static uint32_t prev_edge_time;
static int edge_count = 0;
static bool edge_rising = false;

static void frame_rx_capture_isr(uint32_t edge_time) {
  uint32_t pulse_time = edge_time - prev_edge_time;
  prev_edge_time = edge_time;
  if (edge_count == MAX_EDGE_TIME_CAPTURES)
    edge_count = 0;

  if (edge_rising) {
    pulse_times[edge_count++] = pulse_time;
    cec_hal_rx_irq_low();
    edge_rising = false;
  } else {
    cec_hal_rx_irq_high();
    edge_rising = true;
  }
}

#define TOLERANCE 0

static char dump_buffer[32];
void cec_frame_dump(write_str_ptr_t write_str) {
  int bits = 0;
  uint16_t byte = 0;
  for (int i = 0; i < MAX_EDGE_TIME_CAPTURES; i++) {
    uint32_t _time = pulse_times[i];
    if (_time == 0)
      break;
    if (_time >= (3500 - TOLERANCE) && _time <= (3900 + TOLERANCE)) {
      sprintf(dump_buffer, "\n%lu - START", pulse_times[i]);
      bits = 0;
      byte = 0;
    } else if (_time >= (400 - TOLERANCE) && _time <= (800 + TOLERANCE)) {
      if ((bits + 1) % 10 != 0) {
        sprintf(dump_buffer, "%lu  - 1", pulse_times[i]);
      } else {
        sprintf(dump_buffer, "%lu  - 1 EOM", pulse_times[i]);
      }
      byte <<= 1;
      byte |= 0x01;
    } else if (_time >= (1300 - TOLERANCE) && _time <= (1700 + TOLERANCE)) {
      if ((bits + 1) % 10 != 0) {
        sprintf(dump_buffer, "%lu - 0", pulse_times[i]);
      } else {
        sprintf(dump_buffer, "%lu - 0 *", pulse_times[i]);
      }
      byte <<= 1;
    } else {
      sprintf(dump_buffer, "%lu", pulse_times[i]);
    }

    if (bits && ((bits % 10) == 0)) {
      write_str(dump_buffer);
      byte >>= 2;
      sprintf(dump_buffer, " 0x%02x\n", byte);
      write_str(dump_buffer);
      byte = 0;
    } else {
      strcat(dump_buffer, "\n");
      write_str(dump_buffer);
    }
    bits++;
    if (i % 10 == 0) {
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  }
}

void *cec_frame_capture(void *ptr) {
  if (ptr) {  // disable capture and restore frame rx isr pointer
    cec_hal_rx_irq_disable();
    cec_hal_swap_rx_isr(ptr);
    ptr = NULL;
    cec_hal_rx_irq_low();
    pulse_times[edge_count++] = -1;
  } else {  // initialise the capture and return the previous rx isr pointer
    memset(pulse_times, 0, sizeof(uint32_t) * MAX_EDGE_TIME_CAPTURES);
    ptr = cec_hal_swap_rx_isr(&frame_rx_capture_isr);
    edge_rising = false;
    cec_hal_rx_irq_low();
  }
  return ptr;
}

void cec_frame_rxint(void) {  // diagnostic for user force rx int enable (temporary)
  cec_hal_rx_irq_low();
}
