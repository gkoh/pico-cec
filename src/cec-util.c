#include <stdio.h>
#include <string.h>

#include "portable.h"
DECLARE_TAG()

#include "cec-frame.h"
#include "cec-util.h"

#include "usb-cdc.h"  // for cdc_printfln(..) - temporary

#define MAX_EDGE_TIME_CAPTURES 512
static uint32_t pulse_times[MAX_EDGE_TIME_CAPTURES];
static uint32_t prev_edge_time;
static int edge_count = 0;
static bool edge_rising = false;

static void frame_rx_capture_isr(uint64_t edge_time) {
  uint32_t pulse_time = edge_time - prev_edge_time;
  prev_edge_time = edge_time;
  if (edge_count == MAX_EDGE_TIME_CAPTURES)
    edge_count = 0;

  if (edge_rising) {
    pulse_times[edge_count++] = pulse_time;
    gpio_set_irq_enabled(CEC_PIN, GPIO_IRQ_EDGE_FALL, true);
    edge_rising = false;
  } else {
    gpio_set_irq_enabled(CEC_PIN, GPIO_IRQ_EDGE_RISE, true);
    edge_rising = true;
  }
}

#if 0
static char dump_buffer[16];
void cec_frame_dump(void) {
  for (int i = 0; i < MAX_EDGE_TIME_CAPTURES; i++) {
    sprintf(dump_buffer, "%lu\n", pulse_times[i]);
    tud_cdc_write_str(dump_buffer);
    if (i % 10 == 0) {
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }
}
#else

#define TOLERANCE 0

static char dump_buffer[32];
void cec_frame_dump(void) {
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
      tud_cdc_write_str(dump_buffer);
      byte >>= 2;
      sprintf(dump_buffer, " 0x%02x\n", byte);
      tud_cdc_write_str(dump_buffer);
      byte = 0;
    } else {
      strcat(dump_buffer, "\n");
      tud_cdc_write_str(dump_buffer);
    }
    bits++;
    if (i % 10 == 0) {
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  }
}
#endif

#ifdef __XTENSA__
typedef void (*gpio_irq_callback_t)(uint64_t edge_time);
void gpio_set_irq_callback(
    gpio_irq_callback_t callback);  // for changing the callback after initialisation
#else
static void pico_frame_rx_capture_isr(uint gpio, uint32_t events) {
  gpio_acknowledge_irq(gpio, events);
  //  gpio_set_irq_enabled(CEC_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
  uint64_t edge_time = time_us_64();
  frame_rx_capture_isr(edge_time);
}
#endif  // __XTENSA__

void cec_frame_capture(bool enable) {
  if (enable) {
    memset(pulse_times, 0, sizeof(uint32_t) * MAX_EDGE_TIME_CAPTURES);

#ifdef __XTENSA__
    gpio_set_irq_callback(&frame_rx_capture_isr);
#else
    gpio_set_irq_callback(&pico_frame_rx_capture_isr);
#endif  // __XTENSA__
    edge_rising = false;
    gpio_set_irq_enabled(CEC_PIN, GPIO_IRQ_EDGE_FALL, true);
  } else {
    gpio_set_irq_enabled(CEC_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
    gpio_set_irq_callback(get_frame_rx_isr());
    gpio_set_irq_enabled(CEC_PIN, GPIO_IRQ_EDGE_FALL, true);
    pulse_times[edge_count++] = -1;
  }
}

void cec_frame_rxint(void) {
  ESP_LOGI(TAG, "Forcing gpio irq on..");
  cdc_printfln("Forcing gpio irq on..");
  gpio_set_irq_enabled(CEC_PIN, GPIO_IRQ_EDGE_FALL, true);
}
