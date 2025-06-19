#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"

#include "hdmi-cec-link.h"
#include "hdmi-cec-log.h"

#define NOTIFY_RX ((UBaseType_t)0)
#define NOTIFY_TX ((UBaseType_t)1)

/* CEC statistics. */
static hdmi_cec_stats_t cec_stats;  // TODO: remove dup

/**
 * Calculate next offset as time since boot.
 */
static uint64_t time_next(uint64_t start, uint64_t next) {
  return (next - (time_us_64() - start));
}

/**
 * Pull the CEC line high at the specified time.
 */
static int64_t ack_high(alarm_id_t alarm, void *user_data) {
  gpio_set_dir(CEC_PIN, GPIO_IN);

  return 0;
}

TaskHandle_t xCECTask;

uint8_t rx_buffer[16] = {0x0};
hdmi_message_t rx_message = {.data = &rx_buffer[0], .len = 0};
hdmi_frame_t rx_frame = {.message = &rx_message};

static void hdmi_rx_frame_isr(uint gpio, uint32_t events) {
  uint64_t low_time = 0;
  gpio_acknowledge_irq(gpio, events);
  gpio_set_irq_enabled(CEC_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);

  //  printf("state = %d, byte = %d, bit = %d\n", rx_frame.state, rx_frame.byte, rx_frame.bit);

  switch (rx_frame.state) {
    case HDMI_FRAME_STATE_START_LOW:
      rx_frame.start = time_us_64();
      rx_frame.state = HDMI_FRAME_STATE_START_HIGH;
      gpio_set_irq_enabled(CEC_PIN, GPIO_IRQ_EDGE_RISE, true);
      return;
    case HDMI_FRAME_STATE_START_HIGH:
      low_time = time_us_64() - rx_frame.start;
      if (low_time >= 3500 && low_time <= 3900) {
        rx_frame.first = true;
        rx_frame.byte = 0;
        rx_frame.bit = 0;
        rx_frame.state = HDMI_FRAME_STATE_DATA_LOW;
        gpio_set_irq_enabled(CEC_PIN, GPIO_IRQ_EDGE_FALL, true);
      } else {
        rx_frame.state = HDMI_FRAME_STATE_ABORT;
        xTaskNotifyIndexedFromISR(xCECTask, NOTIFY_RX, 0, eNoAction, NULL);
      }
      return;
    case HDMI_FRAME_STATE_EOM_LOW:
      rx_frame.byte++;
      rx_frame.bit = 0;
    case HDMI_FRAME_STATE_DATA_LOW: {
      uint64_t min_time = rx_frame.first ? 4300 : 2050;
      uint64_t max_time = rx_frame.first ? 4700 : 2750;
      uint64_t bit_time = time_us_64() - rx_frame.start;
      if (bit_time >= min_time && bit_time <= max_time) {
        rx_frame.start = time_us_64();
        if (rx_frame.state == HDMI_FRAME_STATE_EOM_LOW) {
          rx_frame.state = HDMI_FRAME_STATE_EOM_HIGH;
        } else {
          rx_frame.state = HDMI_FRAME_STATE_DATA_HIGH;
        }
        rx_frame.first = false;
        gpio_set_irq_enabled(CEC_PIN, GPIO_IRQ_EDGE_RISE, true);
      } else {
        rx_frame.state = HDMI_FRAME_STATE_ABORT;
        xTaskNotifyIndexedFromISR(xCECTask, NOTIFY_RX, 0, eNoAction, NULL);
      }
    }
      return;
    case HDMI_FRAME_STATE_EOM_HIGH:
    case HDMI_FRAME_STATE_DATA_HIGH:
      low_time = time_us_64() - rx_frame.start;
      uint8_t bit = false;
      if (low_time >= 400 && low_time <= 800) {
        bit = true;
      } else if (low_time >= 1300 && low_time <= 1700) {
        bit = false;
      } else {
        rx_frame.state = HDMI_FRAME_STATE_ABORT;
        xTaskNotifyIndexedFromISR(xCECTask, NOTIFY_RX, 0, eNoAction, NULL);
        return;
      }
      if (rx_frame.state == HDMI_FRAME_STATE_EOM_HIGH) {
        rx_frame.eom = bit;
        rx_frame.state = HDMI_FRAME_STATE_ACK_LOW;
      } else {
        rx_frame.message->data[rx_frame.byte] <<= 1;
        rx_frame.message->data[rx_frame.byte] |= bit ? 0x01 : 0x00;
        rx_frame.bit++;
        if (rx_frame.bit > 7) {
          rx_frame.state = HDMI_FRAME_STATE_EOM_LOW;
        } else {
          rx_frame.state = HDMI_FRAME_STATE_DATA_LOW;
        }
      }
      gpio_set_irq_enabled(CEC_PIN, GPIO_IRQ_EDGE_FALL, true);
      return;
    case HDMI_FRAME_STATE_ACK_LOW:
      rx_frame.start = time_us_64();
      // send ack by changing ack from 1 to 0
      uint8_t tgt_addr = rx_frame.message->data[0] & 0x0f;
      if ((tgt_addr != 0x0f) && (tgt_addr == rx_frame.address)) {
        rx_frame.state = HDMI_FRAME_STATE_ACK_END;
        gpio_set_dir(CEC_PIN, GPIO_OUT);  // pull low, then schedule pull high
        add_alarm_at(from_us_since_boot(rx_frame.start + 1500), ack_high, NULL, true);
        rx_frame.ack = true;
      }
      rx_frame.state = HDMI_FRAME_STATE_ACK_HIGH;
      gpio_set_irq_enabled(CEC_PIN, GPIO_IRQ_EDGE_RISE, true);
      return;
    case HDMI_FRAME_STATE_ACK_HIGH:
      low_time = time_us_64() - rx_frame.start;
      if ((low_time >= 400 && low_time <= 800) || (low_time >= 1300 && low_time <= 1700)) {
        rx_frame.state = HDMI_FRAME_STATE_ACK_END;
      } else {
        rx_frame.state = HDMI_FRAME_STATE_ABORT;
        xTaskNotifyIndexedFromISR(xCECTask, NOTIFY_RX, 0, eNoAction, NULL);
        return;
      }
      // fall through
    case HDMI_FRAME_STATE_ACK_END:
      if (rx_frame.eom) {
        rx_frame.state = HDMI_FRAME_STATE_END;
      } else {
        rx_frame.state = HDMI_FRAME_STATE_DATA_LOW;
        gpio_set_irq_enabled(CEC_PIN, GPIO_IRQ_EDGE_FALL, true);
        return;
      }
      // finish receiving frame
    case HDMI_FRAME_STATE_END:
    default:
      rx_frame.message->len = rx_frame.byte;
      xTaskNotifyIndexedFromISR(xCECTask, NOTIFY_RX, 0, eNoAction, NULL);
  }
}

uint8_t recv_frame(uint8_t *pld, uint8_t address) {
  // printf("recv_frame\n");
  rx_frame.address = address;
  rx_frame.state = HDMI_FRAME_STATE_START_LOW;
  rx_frame.ack = false;
  memset(&rx_frame.message->data[0], 0, 16);
  gpio_set_irq_enabled(CEC_PIN, GPIO_IRQ_EDGE_FALL, true);
  ulTaskNotifyTakeIndexed(NOTIFY_RX, pdTRUE, portMAX_DELAY);
  memcpy(pld, rx_frame.message->data, rx_frame.message->len);
  // printf("high water mark = %lu\n", uxTaskGetStackHighWaterMark(xCECTask));

  log_cec_frame(&rx_frame, true);

  if (rx_frame.state == HDMI_FRAME_STATE_ABORT) {
    // printf("ABORT\n");
    cec_stats.rx_abort_frames++;
    return 0;
  }

  cec_stats.rx_frames++;
  return rx_frame.message->len;
}

static int64_t hdmi_tx_callback(alarm_id_t alarm, void *user_data) {
  hdmi_frame_t *frame = (hdmi_frame_t *)user_data;

  uint64_t low_time = 0;
  switch (frame->state) {
    case HDMI_FRAME_STATE_START_LOW:
      gpio_set_dir(CEC_PIN, GPIO_OUT);
      frame->start = time_us_64();
      frame->state = HDMI_FRAME_STATE_START_HIGH;
      return time_next(frame->start, 3700);
    case HDMI_FRAME_STATE_START_HIGH:
      gpio_set_dir(CEC_PIN, GPIO_IN);
      frame->state = HDMI_FRAME_STATE_DATA_LOW;
      return time_next(frame->start, 4500);
    case HDMI_FRAME_STATE_DATA_LOW:
      gpio_set_dir(CEC_PIN, GPIO_OUT);
      frame->start = time_us_64();
      low_time = (frame->message->data[frame->byte] & (1 << frame->bit)) ? 600 : 1500;
      frame->state = HDMI_FRAME_STATE_DATA_HIGH;
      return time_next(frame->start, low_time);
    case HDMI_FRAME_STATE_DATA_HIGH:
      gpio_set_dir(CEC_PIN, GPIO_IN);
      if (frame->bit--) {
        frame->state = HDMI_FRAME_STATE_DATA_LOW;
      } else {
        frame->byte++;
        frame->state = HDMI_FRAME_STATE_EOM_LOW;
      }
      return time_next(frame->start, 2400);
    case HDMI_FRAME_STATE_EOM_LOW:
      gpio_set_dir(CEC_PIN, GPIO_OUT);
      low_time = (frame->byte < frame->message->len) ? 1500 : 600;
      frame->start = time_us_64();
      frame->state = HDMI_FRAME_STATE_EOM_HIGH;
      return time_next(frame->start, low_time);
    case HDMI_FRAME_STATE_EOM_HIGH:
      gpio_set_dir(CEC_PIN, GPIO_IN);
      frame->state = HDMI_FRAME_STATE_ACK_LOW;
      return time_next(frame->start, 2400);
    case HDMI_FRAME_STATE_ACK_LOW:
      gpio_set_dir(CEC_PIN, GPIO_OUT);
      frame->start = time_us_64();
      frame->state = HDMI_FRAME_STATE_ACK_HIGH;
      return time_next(frame->start, 600);
    case HDMI_FRAME_STATE_ACK_HIGH:
      gpio_set_dir(CEC_PIN, GPIO_IN);
      if (frame->byte < frame->message->len) {
        frame->bit = 7;
        frame->state = HDMI_FRAME_STATE_DATA_LOW;
        return time_next(frame->start, 2400);
      } else {
        frame->state = HDMI_FRAME_STATE_ACK_WAIT;
        // middle of safe sample period (0.85ms, 1.25ms)
        return time_next(frame->start, (850 + 1250) / 2);
      }
    case HDMI_FRAME_STATE_ACK_WAIT:
      // handle follower sending ack
      if (gpio_get(CEC_PIN) == false) {
        frame->ack = true;
      }
      frame->state = HDMI_FRAME_STATE_END;
      return time_next(frame->start, 2400);
    case HDMI_FRAME_STATE_END:
    default:
      xTaskNotifyIndexedFromISR(xCECTask, NOTIFY_TX, 0, eNoAction, NULL);
      return 0;
  }
}

static bool hdmi_tx_frame(uint8_t *data, uint8_t len) {
  unsigned char i = 0;

  // wait 7 bit times of idle before sending
  while (i < 7) {
    vTaskDelay(pdMS_TO_TICKS(2.4));
    if (gpio_get(CEC_PIN)) {
      i++;
    } else {
      // reset
      i = 0;
    }
  }

  hdmi_message_t message = {data, len};
  hdmi_frame_t frame = {.message = &message,
                        .bit = 7,
                        .byte = 0,
                        .start = 0,
                        .ack = false,
                        .state = HDMI_FRAME_STATE_START_LOW};
  add_alarm_at(from_us_since_boot(time_us_64()), hdmi_tx_callback, &frame, true);
  ulTaskNotifyTakeIndexed(NOTIFY_TX, pdTRUE, portMAX_DELAY);
  // printf("high water mark = %lu\n", uxTaskGetStackHighWaterMark(xCECTask));
  log_cec_frame(&frame, false);

  if (frame.ack) {
    cec_stats.tx_frames++;
  } else {
    cec_stats.tx_noack_frames++;
  }

  return frame.ack;
}

bool send_frame(uint8_t pldcnt, uint8_t *pld) {
  // disable GPIO ISR for sending
  gpio_set_irq_enabled(CEC_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
  return hdmi_tx_frame(pld, pldcnt);
}

void cec_get_stats(hdmi_cec_stats_t *stats) {
  *stats = cec_stats;
}

void hdmi_link_init(void) {
  gpio_init(CEC_PIN);
  gpio_disable_pulls(CEC_PIN);
  gpio_set_dir(CEC_PIN, GPIO_IN);
  gpio_set_irq_callback(&hdmi_rx_frame_isr);
  irq_set_enabled(IO_IRQ_BANK0, true);
  gpio_set_irq_enabled(CEC_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
}
