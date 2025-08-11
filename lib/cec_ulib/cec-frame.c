#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "cec-hal.h"
DECLARE_TAG()

#include "cec-frame.h"
#include "cec-log.h"

#define NOTIFY_RX ((UBaseType_t)0)
#define NOTIFY_TX ((UBaseType_t)1)

#define TOLERANCE 50
#define OUT_OF_SPEC 1500

#define CEC_FRAME_MAXSIZE (16)

typedef enum {
  CEC_MSG_TX_PACKET = 0,  // Transmit a full CEC packet
  CEC_MSG_RX_READY,       // Signal that an RX packet is ready in the global buffer
  CEC_MSG_RX_ERROR        // RX error occurred (timing, framing, etc.)
} cec_msg_type_t;

typedef struct {
  cec_msg_type_t type;  // Message type
  union {
    struct {
      uint8_t len;
      uint8_t data[CEC_FRAME_MAXSIZE];
    } tx;

    struct {
      uint8_t len;
    } rx_ready;

    struct {
      uint8_t error_code;  // Application-defined error (e.g., bit timing issue) (unused)
      uint32_t bit_time;
      bool first;
      uint32_t frame_state;
    } rx_error;
  };
} cec_msg_t;

#define CEC_FRAME_QUEUE_LENGTH (5)
static StaticQueue_t xStaticTxFrameQueue;
static QueueHandle_t cec_frame_queue = NULL;
static uint8_t txFrameQueue[CEC_FRAME_QUEUE_LENGTH * sizeof(cec_msg_t)];

TaskHandle_t xCECTask;

static uint8_t rx_buffer[16] = {0x0};
static cec_message_t rx_message = {.data = &rx_buffer[0], .len = 0};
static cec_frame_t rx_frame = {.message = &rx_message};

static cec_frame_stats_t cec_stats;

static int monitor_mode = 0;

void cec_frame_set_monitor_mode(int mode) {
  monitor_mode = mode;
}
int cec_frame_get_monitor_mode(void) {
  return monitor_mode;
}
void cec_frame_clear_stats(void) {
  memset(&cec_stats, 0, sizeof(cec_stats));
}
void cec_frame_get_stats(cec_frame_stats_t *stats) {
  *stats = cec_stats;
}

/**
 * Calculate next offset as time since boot. (TODO: this comment is misleading as it is not
 * returning a time since boot, at least for the esp32 port)
 */
static inline uint32_t time_next(uint32_t start, uint32_t next) {
  return (next - (time_us_64() - start));
}

/**
 * Pull the CEC line high at the specified time.
 */
static uint32_t IRAM_ATTR ack_high(void *user_data) {
  cec_hal_bus_high();
  return 0;
}

static void IRAM_ATTR frame_rx_isr(uint32_t edge_time) {
  uint32_t low_time = 0;

  cec_msg_t signal;
  signal.type = CEC_MSG_RX_ERROR;  // default to signaling RX ERROR (frame abort)
  signal.rx_error.bit_time = edge_time - rx_frame.start;  // save the frame bit time
  signal.rx_error.frame_state = rx_frame.state;           // save the current frame state

  switch (rx_frame.state) {
    case CEC_FRAME_STATE_START_LOW:  // 0
      rx_frame.start = edge_time;
      rx_frame.state = CEC_FRAME_STATE_START_HIGH;
      cec_hal_rx_irq(GPIO_IRQ_EDGE_RISE, true);
      return;
    case CEC_FRAME_STATE_START_HIGH:  // 1
      low_time = edge_time - rx_frame.start;
      if (low_time >= (3500 - TOLERANCE) && low_time <= (3900 + OUT_OF_SPEC)) {
        rx_frame.first = true;
        rx_frame.byte = 0;
        rx_frame.bit = 0;
        rx_frame.state = CEC_FRAME_STATE_DATA_LOW;
        cec_hal_rx_irq(GPIO_IRQ_EDGE_FALL, true);
        return;
      } else {
        rx_frame.state = CEC_FRAME_STATE_ABORT;
      }
      break;
    case CEC_FRAME_STATE_EOM_LOW:  // 4
      rx_frame.byte++;
      rx_frame.bit = 0;
      //  fall thru..
    case CEC_FRAME_STATE_DATA_LOW: {  // 2
      uint32_t min_time = rx_frame.first ? (4300 - TOLERANCE) : (2050 - TOLERANCE);
      uint32_t max_time = rx_frame.first ? (4700 + OUT_OF_SPEC) : (2750 + TOLERANCE);
      uint32_t bit_time = edge_time - rx_frame.start;
      if (bit_time >= min_time && bit_time <= max_time) {
        rx_frame.start = edge_time;
        if (rx_frame.state == CEC_FRAME_STATE_EOM_LOW) {
          rx_frame.state = CEC_FRAME_STATE_EOM_HIGH;
        } else {
          rx_frame.state = CEC_FRAME_STATE_DATA_HIGH;
        }
        rx_frame.first = false;
        cec_hal_rx_irq(GPIO_IRQ_EDGE_RISE, true);
        return;
      } else {
        rx_frame.state = CEC_FRAME_STATE_ABORT;
      }
    } break;
    case CEC_FRAME_STATE_EOM_HIGH:   // 5
    case CEC_FRAME_STATE_DATA_HIGH:  // 3
      low_time = edge_time - rx_frame.start;
      uint8_t bit = false;
      if (low_time >= (400 - TOLERANCE) && low_time <= (800 + TOLERANCE)) {
        bit = true;
      } else if (low_time >= (1300 - TOLERANCE) && low_time <= (1700 + TOLERANCE)) {
        bit = false;
      } else {
        rx_frame.state = CEC_FRAME_STATE_ABORT;
        break;
      }
      if (rx_frame.state == CEC_FRAME_STATE_EOM_HIGH) {
        rx_frame.eom = bit;
        rx_frame.state = CEC_FRAME_STATE_ACK_LOW;
      } else {
        rx_frame.message->data[rx_frame.byte] <<= 1;
        rx_frame.message->data[rx_frame.byte] |= bit ? 0x01 : 0x00;
        rx_frame.bit++;
        if (rx_frame.bit > 7) {
          rx_frame.state = CEC_FRAME_STATE_EOM_LOW;
        } else {
          rx_frame.state = CEC_FRAME_STATE_DATA_LOW;
        }
      }
      cec_hal_rx_irq(GPIO_IRQ_EDGE_FALL, true);
      return;
    case CEC_FRAME_STATE_ACK_LOW:  // 6
      rx_frame.start = edge_time;
      // send ack by changing ack from 1 to 0
      uint8_t tgt_addr = rx_frame.message->data[0] & 0x0f;
      if (!monitor_mode && tgt_addr != 0x0f && tgt_addr == rx_frame.address) {
        rx_frame.state = CEC_FRAME_STATE_ACK_END;  // TODO: remove, gets overwritten below?
        cec_hal_bus_low();                         // pull low, then schedule pull high
        cec_hal_frame_tx(from_us_since_boot(rx_frame.start + 1500), ack_high, NULL);
        rx_frame.ack = true;
      }
      rx_frame.state = CEC_FRAME_STATE_ACK_HIGH;
      cec_hal_rx_irq(GPIO_IRQ_EDGE_RISE, true);
      return;
    case CEC_FRAME_STATE_ACK_HIGH:  // 7 - we see our own ACK_HIGH rising edge?
      low_time = edge_time - rx_frame.start;
      if ((low_time >= (400 - TOLERANCE) && low_time <= (800 + TOLERANCE))
          || (low_time >= (1300 - TOLERANCE) && low_time <= (1700 + TOLERANCE))) {
        rx_frame.state = CEC_FRAME_STATE_ACK_END;
      } else {
        rx_frame.state = CEC_FRAME_STATE_ABORT;
        break;
      }
      // fall through
    case CEC_FRAME_STATE_ACK_END:  // 9
      if (rx_frame.eom) {
        rx_frame.state = CEC_FRAME_STATE_END;
      } else {
        rx_frame.state = CEC_FRAME_STATE_DATA_LOW;
        cec_hal_rx_irq(GPIO_IRQ_EDGE_FALL, true);
        return;
      }
      // finish receiving frame
    case CEC_FRAME_STATE_END:  // 10
    default:
      signal.type = CEC_MSG_RX_READY;
      rx_frame.message->len = rx_frame.byte;
      break;
  }
  cec_hal_rx_irq(GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
  BaseType_t woken = pdFALSE;
  xQueueSendToFrontFromISR(cec_frame_queue, &signal, &woken);
  cec_hal_YIELD_FROM_ISR(woken);
}

static uint32_t frame_tx_callback(void *user_data);

#define IDLE_WAIT 7
#define MAX_WAIT (10 * 16)  // 2.4ms * 10 bits per frame * 16 frames per packet/message

static bool bus_is_idle(int idle_wait, int max_wait) {
  int i = 0;
  int j = 0;
  while (i < idle_wait) {  // we wait at least idle_wait * 2.4 milliseconds
    vTaskDelay(pdMS_TO_TICKS(2.4));
    if (cec_hal_bus_get()) {
      i++;
    } else {
      i = 0;                   // reset idle wait count
      if (j++ > (max_wait)) {  // bus is not becoming available..
        ESP_LOGE(TAG, "bus_is_idle(%d) timeout", idle_wait);
        return false;
      }
    }
  }
  if (j > 0) {  // the bus was busy, but is now free
    ESP_LOGI(TAG, "bus_is_idle(%d) %d", idle_wait, j);
  }
  return true;
}

static bool cec_frame_transmit(uint8_t *data, uint8_t len) {
  // disable GPIO ISR before sending
  cec_hal_rx_irq(GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
  cec_message_t message = {data, len};
  cec_frame_t frame = {
      .message = &message,
      .bit = 7,
      .byte = 0,
      .start = 0,
      .ack = false,
      .state = CEC_FRAME_STATE_START_LOW,
  };
  cec_hal_frame_tx(from_us_since_boot(time_us_64()), frame_tx_callback, &frame);
  // block task until transmission complete
  ulTaskNotifyTakeIndexed(NOTIFY_TX, pdTRUE, portMAX_DELAY);
  cec_log_frame(&frame, false);
  if (frame.ack) {
    cec_stats.tx_frames++;
  } else {
    cec_stats.tx_noack_frames++;
  }
  return frame.ack;
}

uint32_t dwell_period_max;
static uint32_t dwell_time_start;
static cec_msg_type_t dwell_type;

uint8_t cec_frame_recv(uint8_t *pld, uint8_t address) {
  cec_msg_t item;

  // how long were we away before coming back ready for the next receive wait
  if (dwell_time_start != 0) {
    uint32_t dwell_period = time_us_64() - dwell_time_start;
    if (dwell_type == CEC_MSG_RX_READY) {  // only monitoring the maximum rx-packet processing time
      if (dwell_period > cec_stats.dwell_period_max) {
        cec_stats.dwell_period_max = dwell_period;
      }
    }
  }

  rx_frame.address = address;
  rx_frame.state = CEC_FRAME_STATE_START_LOW;
  rx_frame.ack = false;
  memset(&rx_frame.message->data[0], 0, 16);
  cec_hal_rx_irq(GPIO_IRQ_EDGE_FALL, true);

  if (xQueueReceive(cec_frame_queue, &item, portMAX_DELAY) == pdTRUE) {
    dwell_type = 0;

    if (item.type == CEC_MSG_RX_READY) {
      if (rx_frame.message->len) {  // TODO: possibly redundant, replace with assert?

        dwell_time_start = time_us_64();
        dwell_type = CEC_MSG_RX_READY;

        cec_stats.rx_frames++;
        cec_log_frame(&rx_frame, true);
        if (monitor_mode) {
          cec_log_raw_frame(&rx_frame);
        } else {
          memcpy(pld, rx_frame.message->data, rx_frame.message->len);
          return rx_frame.message->len;
        }
      }
    } else if (item.type == CEC_MSG_TX_PACKET) {
      // It's a TX item that we got from the queue
      // ESP_LOGD(TAG, "cec_frame_recv() CEC_MSG_TX_PACKET");
      if (bus_is_idle(IDLE_WAIT, MAX_WAIT)) {  // Proceed to transmit
        cec_frame_transmit(item.tx.data, item.tx.len);
      } else {  // re-queue the transmit packet
        ESP_LOGI(TAG, "cec_frame_recv() CEC_MSG_TX_PACKET - re-queueing");
        // Ensure at least one slot remains for RX ISR to notify
        if (uxQueueSpacesAvailable(cec_frame_queue) > 1) {
          if (xQueueSendToFront(cec_frame_queue, &item, 0) != pdTRUE) {
            // TODO: this could be an assert as it should never happen
            ESP_LOGE(TAG, "cec_frame_recv() CEC_MSG_TX_PACKET - failed to queue");
          }
        } else {  // Reject/drop TX packet to prevent RX ISR from being blocked
          ESP_LOGE(TAG, "cec_frame_recv() CEC_MSG_TX_PACKET - reject, queue full");
        }
      }
    } else if (item.type == CEC_MSG_RX_ERROR) {
      cec_stats.rx_abort_frames++;
      if (!bus_is_idle(IDLE_WAIT * 3 + 1, MAX_WAIT)) {
        cec_stats.idle_timeouts++;
      }
      ESP_LOGE(TAG, "CEC_MSG_RX_ERROR %lu, %lu", item.rx_error.frame_state,
               (uint32_t)item.rx_error.bit_time);
    } else {  // should never get here
      ESP_LOGE(TAG, "cec_frame_recv() item.type %d", item.type);
    }
  }
  return 0;  // we did not receive a frame
}

static uint32_t IRAM_ATTR frame_tx_callback(void *user_data) {
  cec_frame_t *frame = (cec_frame_t *)user_data;
  uint32_t low_time = 0;

  switch (frame->state) {
    case CEC_FRAME_STATE_START_LOW:
      cec_hal_bus_low();
      frame->start = time_us_64();
      frame->state = CEC_FRAME_STATE_START_HIGH;
      return time_next(frame->start, 3700);
    case CEC_FRAME_STATE_START_HIGH:
      cec_hal_bus_high();
      frame->state = CEC_FRAME_STATE_DATA_LOW;
      return time_next(frame->start, 4500);
    case CEC_FRAME_STATE_DATA_LOW:
      cec_hal_bus_low();
      frame->start = time_us_64();
      low_time = (frame->message->data[frame->byte] & (1 << frame->bit)) ? 600 : 1500;
      frame->state = CEC_FRAME_STATE_DATA_HIGH;
      return time_next(frame->start, low_time);
    case CEC_FRAME_STATE_DATA_HIGH:
      cec_hal_bus_high();
      if (frame->bit--) {
        frame->state = CEC_FRAME_STATE_DATA_LOW;
      } else {
        frame->byte++;
        frame->state = CEC_FRAME_STATE_EOM_LOW;
      }
      return time_next(frame->start, 2400);
    case CEC_FRAME_STATE_EOM_LOW:
      cec_hal_bus_low();
      low_time = (frame->byte < frame->message->len) ? 1500 : 600;
      frame->start = time_us_64();
      frame->state = CEC_FRAME_STATE_EOM_HIGH;
      return time_next(frame->start, low_time);
    case CEC_FRAME_STATE_EOM_HIGH:
      cec_hal_bus_high();
      frame->state = CEC_FRAME_STATE_ACK_LOW;
      return time_next(frame->start, 2400);
    case CEC_FRAME_STATE_ACK_LOW:
      cec_hal_bus_low();
      frame->start = time_us_64();
      frame->state = CEC_FRAME_STATE_ACK_HIGH;
      return time_next(frame->start, 600);
    case CEC_FRAME_STATE_ACK_HIGH:
      cec_hal_bus_high();
      if (frame->byte < frame->message->len) {
        frame->bit = 7;
        frame->state = CEC_FRAME_STATE_DATA_LOW;
        return time_next(frame->start, 2400);
      } else {
        frame->state = CEC_FRAME_STATE_ACK_WAIT;
        // middle of safe sample period (0.85ms, 1.25ms)
        return time_next(frame->start, (850 + 1250) / 2);
      }
    case CEC_FRAME_STATE_ACK_WAIT:
      // handle follower sending ack
      if (cec_hal_bus_get() == false) {
        frame->ack = true;
      }
      frame->state = CEC_FRAME_STATE_END;
      return time_next(frame->start, 2400);
    case CEC_FRAME_STATE_END:
    default:
      xTaskNotifyIndexedFromISR(xCECTask, NOTIFY_TX, 0, eNoAction, NULL);
      return 0;
  }
}

bool cec_frame_send(uint8_t pldcnt, uint8_t *pld, bool force) {
  if (monitor_mode && !force)
    return false;
  if (cec_frame_queue == NULL)
    return false;

  cec_msg_t msg = {
      .type = CEC_MSG_TX_PACKET,
      .tx.len = pldcnt,
  };
  memcpy(&(msg.tx.data), pld, pldcnt);

  // Ensure at least one slot remains for RX ISR to notify
  if (uxQueueSpacesAvailable(cec_frame_queue) <= 1) {
    ESP_LOGE(TAG, "cec_frame_send() reject, queue full");
    return false;  // Reject TX to prevent RX ISR from being blocked
  }
  if (xQueueSendToBack(cec_frame_queue, &msg, 0) != pdTRUE) {
    // TODO: this could be an assert as it should never happen
    ESP_LOGE(TAG, "cec_frame_send() failed to queue");
    return false;
  }
  return true;
}

// Define ping to synchronously return the ACK result while keeping everything else async.
bool cec_frame_ping(uint8_t destination) {
  uint8_t pld[1] = {HEADER0(destination, destination)};
  bool ack = false;

  // ESP_LOGD(TAG, "cec_frame_ping(%d)", destination);
  if (bus_is_idle(IDLE_WAIT * 3, MAX_WAIT)) {
    ack = cec_frame_transmit(&pld[0], 1);
    cec_hal_rx_irq(GPIO_IRQ_EDGE_FALL, true);
  }
  return ack;
}

void cec_frame_init(void) {
  cec_frame_queue = xQueueCreateStatic(CEC_FRAME_QUEUE_LENGTH, sizeof(cec_msg_t), &txFrameQueue[0],
                                       &xStaticTxFrameQueue);
  if (!cec_frame_queue) {
    ESP_LOGE(TAG, "Creating CEC frame queue failed");
    return;
  }
  cec_hal_init(CEC_PIN, &frame_rx_isr);
  cec_hal_rx_irq(GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
}
