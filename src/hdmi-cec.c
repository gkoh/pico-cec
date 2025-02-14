#include <stdarg.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "hardware/timer.h"
#include "pico/stdlib.h"
#include "tusb.h"

#include "hdmi-cec.h"
#include "hdmi-ddc.h"

/* Intercept HDMI CEC commands, convert to a keypress and send to HID task
 * handler.
 *
 * Based (mostly ripped) from the Arduino version by Szymon Slupik:
 * https://github.com/SzymonSlupik/CEC-Tiny-Pro
 * which itself is based on the original code by Thomas Sowell:
 * https://github.com/tsowell/avr-hdmi-cec-volume/tree/master
 */

#define NOTIFY_RX ((UBaseType_t)0)
#define NOTIFY_TX ((UBaseType_t)1)

typedef struct {
  const char *name;
  uint8_t key;
} command_t;

/*
 * Key mapping from HDMI user control to HID keyboard entry.
 */
const command_t keymap[256] = {[0x00] = {"User Control Select", HID_KEY_ENTER},
                               [0x01] = {"User Control Up", HID_KEY_ARROW_UP},
                               [0x02] = {"User Control Down", HID_KEY_ARROW_DOWN},
                               [0x03] = {"User Control Left", HID_KEY_ARROW_LEFT},
                               [0x04] = {"User Control Right", HID_KEY_ARROW_RIGHT},
                               [0x0a] = {"User Control Options", HID_KEY_C},
                               [0x0d] = {"User Control Exit", HID_KEY_BACKSPACE},
                               [0x20] = {"User Control 0", HID_KEY_0},
                               [0x21] = {"User Control 1", HID_KEY_1},
                               [0x22] = {"User Control 2", HID_KEY_2},
                               [0x23] = {"User Control 3", HID_KEY_3},
                               [0x24] = {"User Control 4", HID_KEY_4},
                               [0x25] = {"User Control 5", HID_KEY_5},
                               [0x26] = {"User Control 6", HID_KEY_6},
                               [0x27] = {"User Control 7", HID_KEY_7},
                               [0x28] = {"User Control 8", HID_KEY_8},
                               [0x29] = {"User Control 9", HID_KEY_9},
                               [0x35] = {"User Control Display Information", HID_KEY_I},
                               [0x44] = {"User Control Play", HID_KEY_P},
                               [0x45] = {"User Control Stop", HID_KEY_X},
                               [0x46] = {"User Control Pause", HID_KEY_SPACE},
                               [0x48] = {"User Control Rewind", HID_KEY_R},
                               [0x49] = {"User Control Fast Forward", HID_KEY_F},
                               [0x51] = {"User Control Subtitle", HID_KEY_L}};

typedef enum {
  CEC_ID_IMAGE_VIEW_ON = 0x04,
  CEC_ID_TEXT_VIEW_ON = 0x0d,
  CEC_ID_STANDBY = 0x36,
  CEC_ID_USER_CONTROL_PRESSED = 0x44,
  CEC_ID_USER_CONTROL_RELEASED = 0x45,
  CEC_ID_GIVE_OSD_NAME = 0x46,
  CEC_ID_SET_OSD_NAME = 0x47,
  CEC_ID_SYSTEM_AUDIO_MODE_REQUEST = 0x70,
  CEC_ID_GIVE_AUDIO_STATUS = 0x71,
  CEC_ID_SET_SYSTEM_AUDIO_MODE = 0x72,
  CEC_ID_GIVE_SYSTEM_AUDIO_MODE_STATUS = 0x7d,
  CEC_ID_SYSTEM_AUDIO_MODE_STATUS = 0x7e,
  CEC_ID_REPORT_AUDIO_STATUS = 0x7a,
  CEC_ID_ROUTING_CHANGE = 0x80,
  CEC_ID_ACTIVE_SOURCE = 0x82,
  CEC_ID_GIVE_PHYSICAL_ADDRESS = 0x83,
  CEC_ID_REPORT_PHYSICAL_ADDRESS = 0x84,
  CEC_ID_REQUEST_ACTIVE_SOURCE = 0x85,
  CEC_ID_SET_STREAM_PATH = 0x86,
  CEC_ID_DEVICE_VENDOR_ID = 0x87,
  CEC_ID_GIVE_DEVICE_VENDOR_ID = 0x8c,
  CEC_ID_MENU_STATUS = 0x8e,
  CEC_ID_GIVE_DEVICE_POWER_STATUS = 0x8f,
  CEC_ID_REPORT_POWER_STATUS = 0x90,
  CEC_ID_GET_MENU_LANGUAGE = 0x81,
  CEC_ID_INACTIVE_SOURCE = 0x9d,
  CEC_ID_CEC_VERSION = 0x9e,
  CEC_ID_GET_CEC_VERSION = 0x9f,
  CEC_ID_VENDOR_COMMAND_WITH_ID = 0xa0,
  CEC_ID_ABORT = 0xff,
} cec_id_t;

const char *cec_message[] = {
    [CEC_ID_IMAGE_VIEW_ON] = "Image View On",
    [CEC_ID_TEXT_VIEW_ON] = "Text View On",
    [CEC_ID_STANDBY] = "Standby",
    [CEC_ID_USER_CONTROL_PRESSED] = "User Control Pressed",
    [CEC_ID_USER_CONTROL_RELEASED] = "User Control Released",
    [CEC_ID_GIVE_OSD_NAME] = "Give OSD Name",
    [CEC_ID_SET_OSD_NAME] = "Set OSD Name",
    [CEC_ID_SYSTEM_AUDIO_MODE_REQUEST] = "System Audio Mode Request",
    [CEC_ID_GIVE_AUDIO_STATUS] = "Give Audio Status",
    [CEC_ID_SET_SYSTEM_AUDIO_MODE] = "Set System Audio Mode",
    [CEC_ID_GIVE_SYSTEM_AUDIO_MODE_STATUS] = "Give System Audio Mode",
    [CEC_ID_SYSTEM_AUDIO_MODE_STATUS] = "System Audio Mode Status",
    [CEC_ID_ROUTING_CHANGE] = "Routing Change",
    [CEC_ID_ACTIVE_SOURCE] = "Active Source",
    [CEC_ID_GIVE_PHYSICAL_ADDRESS] = "Give Physical Address",
    [CEC_ID_REPORT_PHYSICAL_ADDRESS] = "Report Physical Address",
    [CEC_ID_REQUEST_ACTIVE_SOURCE] = "Request Active Source",
    [CEC_ID_SET_STREAM_PATH] = "Set Stream Path",
    [CEC_ID_DEVICE_VENDOR_ID] = "Device Vendor ID",
    [CEC_ID_GIVE_DEVICE_VENDOR_ID] = "Give Device Vendor ID",
    [CEC_ID_MENU_STATUS] = "Menu Status",
    [CEC_ID_GIVE_DEVICE_POWER_STATUS] = "Give Device Power Status",
    [CEC_ID_REPORT_POWER_STATUS] = "Report Power Status",
    [CEC_ID_GET_MENU_LANGUAGE] = "Get Menu Language",
    [CEC_ID_CEC_VERSION] = "CEC Version",
    [CEC_ID_GET_CEC_VERSION] = "Get CEC Version",
    [CEC_ID_VENDOR_COMMAND_WITH_ID] = "Vendor Command With ID",
    [CEC_ID_ABORT] = "Abort",
};

#define DEFAULT_TYPE 0x04  // HDMI Playback 1

// HDMI Playback logical addresses
#define NUM_ADDRESS 4
static const uint8_t address[NUM_ADDRESS] = {0x04, 0x08, 0x0b, 0x0f};

/* The HDMI address for this device.  Respond to CEC sent to this address. */
static uint8_t laddr = address[0];

/* Construct the frame address header. */
#define HEADER0(iaddr, daddr) ((iaddr << 4) | daddr)

TaskHandle_t xCECTask;

/**
 * Get milliseconds since boot.
 */
static uint64_t uptime_ms(void) {
  return (time_us_64() / 1000);
}

/**
 * Print a timestamped trace log prefix.
 */
static void log_prefix(uint8_t initiator, uint8_t destination, bool send) {
  printf("[%10llu] %02x %s %02x: ", uptime_ms(), send ? initiator : destination, send ? "->" : "<-",
         send ? destination : initiator);
}

/**
 * Print a formatted timestamped trace log.
 */
__attribute__((format(printf, 4, 5))) static void log_printf(uint8_t initiator,
                                                             uint8_t destination,
                                                             bool send,
                                                             const char *fmt,
                                                             ...) {
  va_list ap;
  va_start(ap, fmt);
  log_prefix(initiator, destination, send);
  vprintf(fmt, ap);
  va_end(ap);
}

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

uint8_t rx_buffer[16] = {0x0};
hdmi_message_t rx_message = {.data = &rx_buffer[0], .len = 0};
hdmi_frame_t rx_frame = {.message = &rx_message};

static void hdmi_rx_frame_isr(uint gpio, uint32_t events) {
  uint64_t low_time = 0;
  gpio_acknowledge_irq(gpio, events);
  gpio_set_irq_enabled(CEC_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
  // printf("state = %d, byte = %d, bit = %d\n", rx_frame.state, rx_frame.byte, rx_frame.bit);
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

static uint8_t recv_frame(uint8_t *pld, uint8_t address) {
  // printf("recv_frame\n");
  rx_frame.address = address;
  rx_frame.state = HDMI_FRAME_STATE_START_LOW;
  rx_frame.ack = false;
  memset(&rx_frame.message->data[0], 0, 16);
  gpio_set_irq_enabled(CEC_PIN, GPIO_IRQ_EDGE_FALL, true);
  ulTaskNotifyTakeIndexed(NOTIFY_RX, pdTRUE, portMAX_DELAY);
  memcpy(pld, rx_frame.message->data, rx_frame.message->len);
  // printf("high water mark = %lu\n", uxTaskGetStackHighWaterMark(xCECTask));

  if (rx_frame.state == HDMI_FRAME_STATE_ABORT) {
    // printf("ABORT\n");
    return 0;
  }

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
  return frame.ack;
}

static bool send_frame(uint8_t pldcnt, uint8_t *pld) {
  // disable GPIO ISR for sending
  gpio_set_irq_enabled(CEC_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
  return hdmi_tx_frame(pld, pldcnt);
}

static void device_vendor_id(uint8_t initiator, uint8_t destination, uint32_t vendor_id) {
  uint8_t pld[5] = {(initiator << 4) | destination, 0x87, (vendor_id >> 16) & 0x0ff,
                    (vendor_id >> 8) & 0x0ff, (vendor_id >> 0) & 0x0ff};

  send_frame(5, pld);
  log_printf(initiator, destination, false, "[%s]\n", cec_message[CEC_ID_DEVICE_VENDOR_ID]);
}

static void report_power_status(uint8_t initiator, uint8_t destination, uint8_t power_status) {
  uint8_t pld[3] = {(initiator << 4) | destination, 0x90, power_status};

  send_frame(3, pld);
  log_printf(initiator, destination, false, "[%s]\n", cec_message[CEC_ID_REPORT_POWER_STATUS]);
}

static void set_system_audio_mode(uint8_t initiator,
                                  uint8_t destination,
                                  uint8_t system_audio_mode) {
  uint8_t pld[3];

  pld[0] = (initiator << 4) | destination;
  pld[1] = CEC_ID_SET_SYSTEM_AUDIO_MODE;
  pld[2] = system_audio_mode;

  send_frame(3, pld);
  log_printf(initiator, destination, false, "[%s]\n", cec_message[CEC_ID_SET_SYSTEM_AUDIO_MODE]);
}

static void report_audio_status(uint8_t initiator, uint8_t destination, uint8_t audio_status) {
  uint8_t pld[3];

  pld[0] = (initiator << 4) | destination;
  pld[1] = CEC_ID_REPORT_AUDIO_STATUS;
  pld[2] = audio_status;

  send_frame(3, pld);
  log_printf(initiator, destination, false, "[%s]\n", cec_message[CEC_ID_REPORT_AUDIO_STATUS]);
}

static void system_audio_mode_status(uint8_t initiator,
                                     uint8_t destination,
                                     uint8_t system_audio_mode_status) {
  uint8_t pld[3];

  pld[0] = (initiator << 4) | destination;
  pld[1] = CEC_ID_SYSTEM_AUDIO_MODE_STATUS;
  pld[2] = system_audio_mode_status;

  send_frame(3, pld);
  log_printf(initiator, destination, false, "[%s]\n", cec_message[CEC_ID_SYSTEM_AUDIO_MODE_STATUS]);
}

static void set_osd_name(uint8_t initiator, uint8_t destination) {
  uint8_t pld[10] = {
      (initiator << 4) | destination, CEC_ID_SET_OSD_NAME, 'P', 'i', 'c', 'o', '-', 'C', 'E', 'C'};

  send_frame(10, pld);
  log_printf(initiator, destination, false, "[%s]\n", cec_message[CEC_ID_SET_OSD_NAME]);
}

static void report_physical_address(uint8_t initiator,
                                    uint8_t destination,
                                    uint16_t physical_address,
                                    uint8_t device_type) {
  uint8_t pld[5] = {(initiator << 4) | destination, CEC_ID_REPORT_PHYSICAL_ADDRESS,
                    (physical_address >> 8) & 0x0ff, (physical_address >> 0) & 0x0ff, device_type};

  send_frame(5, pld);
  log_printf(initiator, destination, false, "[%s] %02x%02x\n",
             cec_message[CEC_ID_REPORT_PHYSICAL_ADDRESS], pld[2], pld[3]);
}

static void report_cec_version(uint8_t initiator, uint8_t destination) {
  // 0x04 = 1.3a
  uint8_t pld[3] = {HEADER0(initiator, destination), CEC_ID_CEC_VERSION, 0x04};
  send_frame(3, pld);
  log_printf(initiator, destination, false, "[%s]\n", cec_message[CEC_ID_CEC_VERSION]);
}

static bool ping(uint8_t destination) {
  uint8_t pld[1] = {HEADER0(destination, destination)};

  return send_frame(1, pld);
}

static void image_view_on(uint8_t initiator, uint8_t destination) {
  uint8_t pld[2] = {HEADER0(initiator, destination), CEC_ID_IMAGE_VIEW_ON};

  send_frame(2, pld);
  log_printf(initiator, destination, false, "[%s]\n", cec_message[CEC_ID_IMAGE_VIEW_ON]);
}

static void active_source(uint8_t initiator, uint16_t physical_address) {
  uint8_t pld[4] = {HEADER0(initiator, 0x0f), CEC_ID_ACTIVE_SOURCE, (physical_address >> 8) & 0x0ff,
                    (physical_address >> 0) & 0x0ff};

  send_frame(4, pld);
  log_printf(initiator, 0x0f, false, "[%s]\n", cec_message[CEC_ID_ACTIVE_SOURCE]);
}

static uint8_t allocate_logical_address(void) {
  uint8_t a;
  for (unsigned int i = 0; i < NUM_ADDRESS; i++) {
    a = address[i];
    printf("\nAttempting to allocate logical address 0x%02x\n", a);
    if (!ping(a)) {
      break;
    }
  }

  printf("Allocated logical address 0x%02x\n", a);
  return a;
}

void cec_task(void *data) {
  QueueHandle_t *q = (QueueHandle_t *)data;

  // pause 5000ms
  vTaskDelay(pdMS_TO_TICKS(5000));

  gpio_init(CEC_PIN);
  gpio_disable_pulls(CEC_PIN);
  gpio_set_dir(CEC_PIN, GPIO_IN);

  gpio_set_irq_callback(&hdmi_rx_frame_isr);
  irq_set_enabled(IO_IRQ_BANK0, true);
  gpio_set_irq_enabled(CEC_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);

  uint16_t paddr = ddc_get_physical_address();
  laddr = allocate_logical_address();

  while (true) {
    uint8_t pld[16] = {0x0};
    uint8_t pldcnt, pldcntrcvd;
    uint8_t initiator, destination;
    uint8_t key = HID_KEY_NONE;

    pldcnt = recv_frame(pld, laddr);
    // printf("pldcnt = %u\n", pldcnt);
    pldcntrcvd = pldcnt;
    initiator = (pld[0] & 0xf0) >> 4;
    destination = pld[0] & 0x0f;
    log_prefix(initiator, destination, true);

    if ((pldcnt > 1)) {
      const char *message = cec_message[pld[1]];
      if (strlen(message) > 0) {
        printf("[%s]\n", cec_message[pld[1]]);
      } else {
        printf("[%x]\n", pld[1]);
      }

      switch (pld[1]) {
        case CEC_ID_IMAGE_VIEW_ON:
          break;
        case CEC_ID_TEXT_VIEW_ON:
          break;
        case CEC_ID_STANDBY:
          printf("<*> [Turn the display OFF]\n");
          break;
        case CEC_ID_SYSTEM_AUDIO_MODE_REQUEST:
          if (destination == laddr)
            set_system_audio_mode(laddr, 0x0f, 1);
          break;
        case CEC_ID_GIVE_AUDIO_STATUS:
          if (destination == laddr)
            report_audio_status(laddr, initiator, 0x32);  // volume 50%, mute off
          break;
        case CEC_ID_SET_SYSTEM_AUDIO_MODE:
          break;
        case CEC_ID_GIVE_SYSTEM_AUDIO_MODE_STATUS:
          if (destination == laddr)
            system_audio_mode_status(laddr, initiator, 1);
          break;
        case CEC_ID_SYSTEM_AUDIO_MODE_STATUS:
          break;
        case CEC_ID_ROUTING_CHANGE:
          paddr = ddc_get_physical_address();
          image_view_on(laddr, 0x00);
          break;
        case CEC_ID_ACTIVE_SOURCE:
          printf("<*> [Turn the display ON]\n");
          break;
        case CEC_ID_REPORT_PHYSICAL_ADDRESS:
          // On broadcast receive, do the same
          if ((initiator == 0x00) && (destination == 0x0f)) {
            paddr = ddc_get_physical_address();
            laddr = allocate_logical_address();
            if (paddr != 0x0000) {
              report_physical_address(laddr, 0x0f, paddr, DEFAULT_TYPE);
            }
          }
          break;
        case CEC_ID_REQUEST_ACTIVE_SOURCE:
        case CEC_ID_SET_STREAM_PATH:
          if (paddr != 0x0000) {
            active_source(laddr, paddr);
          }
          break;
        case CEC_ID_DEVICE_VENDOR_ID:
          // On broadcast receive, do the same
          if ((initiator == 0x00) && (destination == 0x0f)) {
            device_vendor_id(laddr, 0x0f, 0x0010FA);
          }
          break;
        case CEC_ID_GIVE_DEVICE_VENDOR_ID:
          if (destination == laddr)
            device_vendor_id(laddr, 0x0f, 0x0010FA);
          break;
        case CEC_ID_MENU_STATUS:
          break;
        case CEC_ID_GIVE_DEVICE_POWER_STATUS:
          if (destination == laddr)
            report_power_status(laddr, initiator, 0x00);
          /* Hack for Google Chromecast to force it sending V+/V- if no CEC TV is present */
          if (destination == 0)
            report_power_status(0, initiator, 0x00);
          break;
        case CEC_ID_REPORT_POWER_STATUS:
          break;
        case CEC_ID_GET_MENU_LANGUAGE:
          break;
        case CEC_ID_INACTIVE_SOURCE:
          break;
        case CEC_ID_CEC_VERSION:
          break;
        case CEC_ID_GET_CEC_VERSION:
          if (destination == laddr) {
            report_cec_version(laddr, initiator);
          }
          break;
        case CEC_ID_GIVE_OSD_NAME:
          if (destination == laddr)
            set_osd_name(laddr, initiator);
          break;
        case CEC_ID_SET_OSD_NAME:
          break;
        case CEC_ID_GIVE_PHYSICAL_ADDRESS:
          if (destination == laddr && paddr != 0x0000)
            report_physical_address(laddr, 0x0f, paddr, DEFAULT_TYPE);
          break;
        case CEC_ID_USER_CONTROL_PRESSED:
          gpio_put(PICO_DEFAULT_LED_PIN, true);
          switch (pld[2]) {
            case 0x41:
              printf("[User Control Volume Up]");
              break;
            case 0x42:
              printf("[User Control Volume Down]");
              break;
            default: {
              command_t command = keymap[pld[2]];
              if (command.name != NULL) {
                printf(command.name);
                xQueueSend(*q, &command.key, pdMS_TO_TICKS(10));
              } else {
                printf("Unmapped command: 0x%02x\n", pld[2]);
              }
            }
          }
          break;
        case CEC_ID_USER_CONTROL_RELEASED:
          gpio_put(PICO_DEFAULT_LED_PIN, false);
          key = HID_KEY_NONE;
          xQueueSend(*q, &key, pdMS_TO_TICKS(10));
          break;
        case CEC_ID_ABORT:
          printf("[Abort]");
          break;
        case CEC_ID_VENDOR_COMMAND_WITH_ID:
          printf("[Vendor Command with ID]");
          for (int i = 0; i < pldcnt; i++) {
            printf(" %02x", pld[i]);
          }
          printf("\n");
          break;
        default:
          if (pldcntrcvd > 1)
            printf("???: %x", pld[1]);  // undecoded command
          break;
      }
    } else {
      // single byte polling message
      printf("[Polling Message]\n");
    }
  }
}
