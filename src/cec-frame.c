#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "portable.h"
DECLARE_TAG()

#include "cec-frame.h"
#include "cec-log.h"

#define NOTIFY_RX ((UBaseType_t)0)
#define NOTIFY_TX ((UBaseType_t)1)

#define TOLERANCE 50

typedef enum {
  CEC_MSG_TX_PACKET = 0,  // Transmit a full CEC packet
  CEC_MSG_RX_READY,       // Signal that an RX packet is ready in the global buffer
  CEC_MSG_RX_ERROR        // RX error occurred (timing, framing, etc.)
} cec_msg_type_t;

#define CEC_FRAME_MAXSIZE (16)

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
      uint64_t bit_time;
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
static inline uint64_t time_next(uint64_t start, uint64_t next) {
  return (next - (time_us_64() - start));
}

/**
 * Pull the CEC line high at the specified time.
 */
static int64_t IRAM_ATTR ack_high(alarm_id_t alarm, void *user_data) {
  gpio_set_dir(CEC_PIN, GPIO_IN);
  return 0;
}

#define OUT_OF_SPEC 1500

static void IRAM_ATTR frame_rx_isr(uint64_t edge_time) {
  uint64_t low_time = 0;

  cec_msg_t signal;
  signal.type = CEC_MSG_RX_ERROR;  // default to signaling RX ERROR (frame abort)
  signal.rx_error.bit_time = edge_time - rx_frame.start;  // save the frame bit time
  signal.rx_error.frame_state = rx_frame.state;           // save the current frame state

  switch (rx_frame.state) {
    case CEC_FRAME_STATE_START_LOW:
      rx_frame.start = edge_time;
      rx_frame.state = CEC_FRAME_STATE_START_HIGH;
      gpio_set_irq_enabled(CEC_PIN, GPIO_IRQ_EDGE_RISE, true);
      return;
    case CEC_FRAME_STATE_START_HIGH:
      low_time = edge_time - rx_frame.start;
      if (low_time >= (3500 - TOLERANCE) && low_time <= (3900 + OUT_OF_SPEC)) {
        rx_frame.first = true;
        rx_frame.byte = 0;
        rx_frame.bit = 0;
        rx_frame.state = CEC_FRAME_STATE_DATA_LOW;
        gpio_set_irq_enabled(CEC_PIN, GPIO_IRQ_EDGE_FALL, true);
        return;
      } else {
        rx_frame.state = CEC_FRAME_STATE_ABORT;
      }
      break;
    case CEC_FRAME_STATE_EOM_LOW:
      rx_frame.byte++;
      rx_frame.bit = 0;
      //  fall thru..
    case CEC_FRAME_STATE_DATA_LOW: {
      uint64_t min_time = rx_frame.first ? (4300 - TOLERANCE) : (2050 - TOLERANCE);
      uint64_t max_time = rx_frame.first ? (4700 + OUT_OF_SPEC) : (2750 + TOLERANCE);
      uint64_t bit_time = edge_time - rx_frame.start;
      if (bit_time >= min_time && bit_time <= max_time) {
        rx_frame.start = edge_time;
        if (rx_frame.state == CEC_FRAME_STATE_EOM_LOW) {
          rx_frame.state = CEC_FRAME_STATE_EOM_HIGH;
        } else {
          rx_frame.state = CEC_FRAME_STATE_DATA_HIGH;
        }
        rx_frame.first = false;
        gpio_set_irq_enabled(CEC_PIN, GPIO_IRQ_EDGE_RISE, true);
        return;
      } else {
        rx_frame.state = CEC_FRAME_STATE_ABORT;
      }
    } break;
    case CEC_FRAME_STATE_EOM_HIGH:
    case CEC_FRAME_STATE_DATA_HIGH:
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
      gpio_set_irq_enabled(CEC_PIN, GPIO_IRQ_EDGE_FALL, true);
      return;
    case CEC_FRAME_STATE_ACK_LOW:
      rx_frame.start = edge_time;
      // send ack by changing ack from 1 to 0
      uint8_t tgt_addr = rx_frame.message->data[0] & 0x0f;
      if (!monitor_mode && tgt_addr != 0x0f && tgt_addr == rx_frame.address) {
        rx_frame.state = CEC_FRAME_STATE_ACK_END;  // TODO: remove, gets overwritten below?
        gpio_set_dir(CEC_PIN, GPIO_OUT);           // pull low, then schedule float high
        add_alarm_at(from_us_since_boot(rx_frame.start + 1500), ack_high, NULL, true);
        rx_frame.ack = true;
      }
      rx_frame.state = CEC_FRAME_STATE_ACK_HIGH;
      gpio_set_irq_enabled(CEC_PIN, GPIO_IRQ_EDGE_RISE, true);
      return;
    case CEC_FRAME_STATE_ACK_HIGH:  // we see our own ACK_HIGH rising edge?
      low_time = edge_time - rx_frame.start;
      if ((low_time >= (400 - TOLERANCE) && low_time <= (800 + TOLERANCE))
          || (low_time >= (1300 - TOLERANCE) && low_time <= (1700 + TOLERANCE))) {
        rx_frame.state = CEC_FRAME_STATE_ACK_END;
      } else {
        rx_frame.state = CEC_FRAME_STATE_ABORT;
        break;
      }
      // fall through
    case CEC_FRAME_STATE_ACK_END:
      if (rx_frame.eom) {
        rx_frame.state = CEC_FRAME_STATE_END;
      } else {
        rx_frame.state = CEC_FRAME_STATE_DATA_LOW;
        gpio_set_irq_enabled(CEC_PIN, GPIO_IRQ_EDGE_FALL, true);
        return;
      }
      // finish receiving frame
    case CEC_FRAME_STATE_END:
    default:
      signal.type = CEC_MSG_RX_READY;
      rx_frame.message->len = rx_frame.byte;
      break;
  }
  gpio_set_irq_enabled(CEC_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
  BaseType_t woken = pdFALSE;
  xQueueSendToFrontFromISR(cec_frame_queue, &signal, &woken);
  if (woken) {
#ifdef __XTENSA__
    portYIELD_FROM_ISR();
#else
    portYIELD_FROM_ISR(woken);
#endif  // __XTENSA__
  }
}

#ifndef __XTENSA__
static void pico_rx_isr(uint gpio, uint32_t events) {
  gpio_acknowledge_irq(gpio, events);
  gpio_set_irq_enabled(CEC_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
  uint64_t edge_time = time_us_64();
  frame_rx_isr(edge_time);
}
#endif  // __XTENSA__

void *get_frame_rx_isr(void) {  // for access by cec-util module
#ifdef __XTENSA__
  return frame_rx_isr;
#else
  return pico_rx_isr;
#endif
}

static int64_t IRAM_ATTR frame_tx_callback(alarm_id_t alarm, void *user_data);

/*
 * WARNING: pdMS_TO_TICKS() gotchas
 *
 * https://www.freertos.org/FreeRTOS_Support_Forum_Archive/February_2016/freertos_On_pdMS_TO_TICKS_macro_definition_317c7160j.html
 */

/*
CEC 9.1 Signal Free Time
Before attempting to transmit or re-transmit a frame, a device shall ensure that the CEC line has
been inactive for a number of bit periods. This signal free time is defined as the time since the
start of the final bit of the previous frame.
The length of the required signal free time depends on the current status of the control signal
line and the initiating device. The different signal free times required are summarized in the
following table:

CEC Table 4 Signal Free Time
PreconditionSignal Free Time (nominal data bit periods)
Present initiator wants to send another frame immediately after its previous frame >= 7
New initiator wants to send a frame >= 5
Previous attempt to send frame unsuccessful >= 3
This means that there is an opportunity for other devices to gain access to the CEC line during the
periods mentioned above to send their own messages after the current device has finished sending
its current message.
*/
#define IDLE_WAIT 7
#define MAX_WAIT (10 * 16)  // 2.4ms * 10 bits per frame * 16 frames per packet

static bool bus_is_idle(int idle_wait, int max_wait) {
  int i = 0;
  int j = 0;
  while (i < idle_wait) {  // we wait at least idle_wait * 2.4 milliseconds
    vTaskDelay(pdMS_TO_TICKS(2.4));
    if (gpio_get(CEC_PIN)) {
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
  gpio_set_irq_enabled(CEC_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
  cec_message_t message = {data, len};
  cec_frame_t frame = {
      .message = &message,
      .bit = 7,
      .byte = 0,
      .start = 0,
      .ack = false,
      .state = CEC_FRAME_STATE_START_LOW,
  };
  add_alarm_at(from_us_since_boot(time_us_64()), frame_tx_callback, &frame, true);
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

uint8_t cec_frame_recv(uint8_t *pld, uint8_t address) {
  cec_msg_t item;

  rx_frame.address = address;
  rx_frame.state = CEC_FRAME_STATE_START_LOW;
  rx_frame.ack = false;
  memset(&rx_frame.message->data[0], 0, 16);
  gpio_set_irq_enabled(CEC_PIN, GPIO_IRQ_EDGE_FALL, true);

  if (xQueueReceive(cec_frame_queue, &item, portMAX_DELAY) == pdTRUE) {
    if (item.type == CEC_MSG_RX_READY) {
      if (rx_frame.message->len) {  // TODO: possibly redundant, replace with assert?
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

// static int64_t IRAM_ATTR frame_tx_callback(alarm_id_t alarm, void *user_data) {
static int64_t frame_tx_callback(alarm_id_t alarm, void *user_data) {
  cec_frame_t *frame = (cec_frame_t *)user_data;
  uint64_t low_time = 0;

  switch (frame->state) {
    case CEC_FRAME_STATE_START_LOW:
      gpio_set_dir(CEC_PIN, GPIO_OUT);
      frame->start = time_us_64();
      frame->state = CEC_FRAME_STATE_START_HIGH;
      return time_next(frame->start, 3700);
    case CEC_FRAME_STATE_START_HIGH:
      gpio_set_dir(CEC_PIN, GPIO_IN);
      frame->state = CEC_FRAME_STATE_DATA_LOW;
      return time_next(frame->start, 4500);
    case CEC_FRAME_STATE_DATA_LOW:
      gpio_set_dir(CEC_PIN, GPIO_OUT);
      frame->start = time_us_64();
      low_time = (frame->message->data[frame->byte] & (1 << frame->bit)) ? 600 : 1500;
      frame->state = CEC_FRAME_STATE_DATA_HIGH;
      return time_next(frame->start, low_time);
    case CEC_FRAME_STATE_DATA_HIGH:
      gpio_set_dir(CEC_PIN, GPIO_IN);
      if (frame->bit--) {
        frame->state = CEC_FRAME_STATE_DATA_LOW;
      } else {
        frame->byte++;
        frame->state = CEC_FRAME_STATE_EOM_LOW;
      }
      return time_next(frame->start, 2400);
    case CEC_FRAME_STATE_EOM_LOW:
      gpio_set_dir(CEC_PIN, GPIO_OUT);
      low_time = (frame->byte < frame->message->len) ? 1500 : 600;
      frame->start = time_us_64();
      frame->state = CEC_FRAME_STATE_EOM_HIGH;
      return time_next(frame->start, low_time);
    case CEC_FRAME_STATE_EOM_HIGH:
      gpio_set_dir(CEC_PIN, GPIO_IN);
      frame->state = CEC_FRAME_STATE_ACK_LOW;
      return time_next(frame->start, 2400);
    case CEC_FRAME_STATE_ACK_LOW:
      gpio_set_dir(CEC_PIN, GPIO_OUT);
      frame->start = time_us_64();
      frame->state = CEC_FRAME_STATE_ACK_HIGH;
      return time_next(frame->start, 600);
    case CEC_FRAME_STATE_ACK_HIGH:
      gpio_set_dir(CEC_PIN, GPIO_IN);
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
      if (gpio_get(CEC_PIN) == false) {
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

bool force = false;

bool cec_frame_send(uint8_t pldcnt, uint8_t *pld) {
  if (monitor_mode && !force)
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

// Allow cec_frame_ping() to synchronously return the ACK result while keeping everything else
// async.
bool cec_frame_ping(uint8_t destination) {
  uint8_t pld[1] = {HEADER0(destination, destination)};
  bool ack = false;

  // ESP_LOGD(TAG, "cec_frame_ping(%d)", destination);
  if (bus_is_idle(IDLE_WAIT * 3, MAX_WAIT)) {
    ack = cec_frame_transmit(&pld[0], 1);
    gpio_set_irq_enabled(CEC_PIN, GPIO_IRQ_EDGE_FALL, true);
  }
  return ack;
}

void cec_frame_init(void) {
  gpio_init(CEC_PIN);
  gpio_disable_pulls(CEC_PIN);
  gpio_set_dir(CEC_PIN, GPIO_IN);

  cec_frame_queue = xQueueCreateStatic(CEC_FRAME_QUEUE_LENGTH, sizeof(cec_msg_t), &txFrameQueue[0],
                                       &xStaticTxFrameQueue);
  if (!cec_frame_queue) {
    ESP_LOGE(TAG, "Creating CEC frame queue failed");
    return;
  }
#ifdef __XTENSA__
  esp_cec_rx_init(CEC_PIN, &frame_rx_isr);
#else
  gpio_set_irq_callback(&pico_rx_isr);
  irq_set_enabled(IO_IRQ_BANK0, true);
#endif  // __XTENSA__
  gpio_set_irq_enabled(CEC_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
}
