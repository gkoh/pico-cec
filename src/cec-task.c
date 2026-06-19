#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "class/hid/hid.h"
#include "pico/stdlib.h"
#include "tusb.h"

#include "blink.h"
#include "cec-config.h"
#include "cec-frame.h"
#include "cec-id.h"
#include "cec-log.h"
#include "cec-task.h"
#include "ddc.h"
#include "nvs.h"

/* Intercept HDMI CEC commands, convert to a keypress and send to HID task
 * handler.
 *
 * Significantly rewritten from the initial Arduino version by Szymon Slupik:
 * https://github.com/SzymonSlupik/CEC-Tiny-Pro
 * which itself is based on the original code by Thomas Sowell:
 * https://github.com/tsowell/avr-hdmi-cec-volume/tree/master
 */

#define CEC_MSG_QUEUE_LENGTH (8)

typedef struct {
  uint8_t addr;
  cec_id_t id;
} send_msg_t;

/** The running CEC configuration. */
static cec_config_t config = {0x0};

// HDMI logical addresses
// 2 dimensional array of valid logical addresses for playback and recording
// only.
#define NUM_LADDRESS 4
#define NUM_TYPES 6
static const uint8_t laddress[NUM_TYPES][NUM_LADDRESS] = {
    {0x00, 0x00, 0x00, 0x00},  // TV
    {0x01, 0x02, 0x09, 0x0f},  // Recording Device
    {0x0f, 0x0f, 0x0f, 0x0f},  // Reserved
    {0x03, 0x06, 0x07, 0x0f},  // Tuner + 0x0a
    {0x04, 0x08, 0x0b, 0x0f},  // Playback Device
    {0x05, 0x05, 0x05, 0x05},  // Audio System
};

/* The HDMI address for this device.  Respond to CEC sent to this address. */
static uint8_t laddr = 0x0f;

/* The HDMI physical address. */
static uint16_t paddr = 0x0000;

/* Active state. */
static uint16_t active_addr = 0x0000;

/* Audio state. */
static bool audio_status = false;

/* Outbound TX queue: holds opcodes to be sent as broadcast frames. */
#define CEC_TX_QUEUE_LEN 1
static QueueHandle_t cec_tx_queue = NULL;
static StaticQueue_t cec_tx_queue_buf;
static uint8_t cec_tx_queue_storage[CEC_TX_QUEUE_LEN * sizeof(send_msg_t)];

/* Construct the frame address header. */
#define HEADER0(iaddr, daddr) ((iaddr << 4) | daddr)

static void cec_feature_abort(uint8_t initiator,
                              uint8_t destination,
                              uint8_t msg,
                              cec_abort_t reason) {
  cec_message_t message = {
      .header = HEADER0(initiator, destination),
      .opcode = CEC_ID_FEATURE_ABORT,
      .operand = {msg, reason},
      .len = 4,
  };

  cec_frame_send(&message);
}

static void device_vendor_id(uint8_t initiator, uint8_t destination, uint32_t vendor_id) {
  cec_message_t message = {
      .header = HEADER0(initiator, destination),
      .opcode = CEC_ID_DEVICE_VENDOR_ID,
      .operand = {(vendor_id >> 16) & 0x0ff, (vendor_id >> 8) & 0x0ff, (vendor_id >> 0) & 0x0ff},
      .len = 5,
  };

  cec_frame_send(&message);
}

static void report_power_status(uint8_t initiator, uint8_t destination, uint8_t power_status) {
  cec_message_t message = {.header = HEADER0(initiator, destination),
                           .opcode = CEC_ID_REPORT_POWER_STATUS,
                           .operand = {power_status},
                           .len = 3};

  cec_frame_send(&message);
}

static void set_system_audio_mode(uint8_t initiator,
                                  uint8_t destination,
                                  uint8_t system_audio_mode) {
  cec_message_t message = {
      .header = HEADER0(initiator, destination),
      .opcode = CEC_ID_SET_SYSTEM_AUDIO_MODE,
      .operand = {system_audio_mode},
      .len = 3,
  };

  cec_frame_send(&message);
}

static void report_audio_status(uint8_t initiator, uint8_t destination, uint8_t audio_status) {
  cec_message_t message = {.header = HEADER0(initiator, destination),
                           .opcode = CEC_ID_REPORT_AUDIO_STATUS,
                           .operand = {audio_status},
                           .len = 3};

  cec_frame_send(&message);
}

static void system_audio_mode_status(uint8_t initiator,
                                     uint8_t destination,
                                     uint8_t system_audio_mode_status) {
  cec_message_t message = {
      .header = HEADER0(initiator, destination),
      .opcode = CEC_ID_SYSTEM_AUDIO_MODE_STATUS,
      .operand = {system_audio_mode_status},
      .len = 3,
  };

  cec_frame_send(&message);
}

static void set_osd_name(uint8_t initiator, uint8_t destination) {
  const char *osd_name = CEC_OSD_NAME;
  uint8_t name_len = strnlen(osd_name, CEC_OSD_NAME_MAX_LEN);

  cec_message_t message = {
      .header = HEADER0(initiator, destination),
      .opcode = CEC_ID_SET_OSD_NAME,
      .len = 2 + name_len,
  };

  memcpy(message.operand, osd_name, name_len);

  cec_frame_send(&message);
}

static void report_physical_address(uint8_t initiator,
                                    uint8_t destination,
                                    uint16_t physical_address,
                                    uint8_t device_type) {
  cec_message_t message = {
      .header = HEADER0(initiator, destination),
      .opcode = CEC_ID_REPORT_PHYSICAL_ADDRESS,
      .operand = {(physical_address >> 8) & 0x0ff, (physical_address >> 0) & 0x0ff, device_type},
      .len = 5,
  };

  cec_frame_send(&message);
}

static void report_cec_version(uint8_t initiator, uint8_t destination) {
  cec_message_t message = {
      .header = HEADER0(initiator, destination),
      .opcode = CEC_ID_CEC_VERSION,
      .operand = {0x04},  // 0x04 = 1.3a
      .len = 3,
  };

  cec_frame_send(&message);
}

bool cec_ping(uint8_t destination) {
  cec_message_t message = {
      .header = HEADER0(destination, destination),
      .len = 1,
  };
  return cec_frame_send(&message);
}

bool cec_send_msg(uint8_t address, uint8_t opcode) {
  send_msg_t msg = {.addr = address, .id = opcode};

  if (cec_tx_queue == NULL) {
    return false;
  }

  if (xQueueSend(cec_tx_queue, &msg, pdMS_TO_TICKS(10)) != pdTRUE) {
    return false;
  }
  xTaskNotifyIndexed(xCECTask, NOTIFY_RX, NOTIFY_RX_TX, eSetBits);
  return true;
}

static void image_view_on(uint8_t initiator, uint8_t destination) {
  cec_message_t message = {
      .header = HEADER0(initiator, destination),
      .opcode = CEC_ID_IMAGE_VIEW_ON,
      .len = 2,
  };

  cec_frame_send(&message);
}

static void active_source(uint8_t initiator, uint16_t physical_address) {
  cec_message_t message = {
      .header = HEADER0(initiator, 0x0f),
      .opcode = CEC_ID_ACTIVE_SOURCE,
      .operand = {(physical_address >> 8) & 0x0ff, (physical_address >> 0) & 0x0ff},
      .len = 4,
  };

  cec_frame_send(&message);
}

static void menu_status(uint8_t initiator, uint8_t destination, bool menu_state) {
  uint8_t state = menu_state ? (uint8_t)CEC_MENU_ACTIVATE : (uint8_t)CEC_MENU_DEACTIVATE;
  cec_message_t message = {
      .header = HEADER0(initiator, destination),
      .opcode = CEC_ID_MENU_STATUS,
      .operand = {state},
      .len = 3,
  };

  cec_frame_send(&message);
}

static uint8_t allocate_logical_address(cec_config_t *config) {
  if (config->logical_address != 0x00 && config->logical_address != 0x0f) {
    return config->logical_address;
  }

  // Treat 0x00 or 0x0f as auto-allocate
  uint8_t a;
  for (unsigned int i = 0; i < NUM_LADDRESS; i++) {
    a = laddress[config->device_type][i];
    cec_log_submitf("Attempting to allocate logical address 0x%01hhx"_LOG_BR, a);
    if (!cec_ping(a)) {
      break;
    }
  }

  cec_log_submitf("Allocated logical address 0x%02x"_LOG_BR, a);
  return a;
}

uint16_t get_physical_address(const cec_config_t *config) {
  return (config->physical_address == 0x0000) ? ddc_get_physical_address()
                                              : config->physical_address;
}

uint16_t cec_get_physical_address(void) {
  return paddr;
}

uint8_t cec_get_logical_address(void) {
  return laddr;
}

void cec_task(void *param) {
  QueueHandle_t *q = (QueueHandle_t *)param;

  /* Menu state. */
  bool menu_state = false;

  // load configuration
  nvs_load_config(&config);

  cec_tx_queue = xQueueCreateStatic(CEC_TX_QUEUE_LEN, sizeof(send_msg_t), cec_tx_queue_storage,
                                    &cec_tx_queue_buf);

  // pause for EDID to settle
  vTaskDelay(pdMS_TO_TICKS(config.edid_delay_ms));

  cec_frame_init();

  paddr = get_physical_address(&config);
  laddr = allocate_logical_address(&config);

  while (true) {
    cec_message_t msg = {0x0};
    uint8_t key = HID_KEY_NONE;
    uint8_t no_active = 0;

    cec_frame_recv(&msg, laddr);

    send_msg_t tx_req;
    if (xQueueReceive(cec_tx_queue, &tx_req, 0) == pdTRUE) {
      cec_message_t tx_msg = {
          .header = HEADER0(laddr, tx_req.addr),
          .opcode = tx_req.id,
          .len = 2,
      };
      cec_frame_send(&tx_msg);
      continue;
    }

    if (msg.len > 1) {
      uint8_t initiator = (msg.header & 0xf0) >> 4;
      uint8_t destination = msg.header & 0x0f;

      switch (msg.opcode) {
        case CEC_ID_IMAGE_VIEW_ON:
          break;
        case CEC_ID_TEXT_VIEW_ON:
          break;
        case CEC_ID_STANDBY:
          if (destination == laddr || destination == 0x0f) {
            active_addr = 0x0000;
            blink_set_blink(BLINK_STATE_BLUE_2HZ);
          }
          break;
        case CEC_ID_SYSTEM_AUDIO_MODE_REQUEST:
          if (destination == laddr) {
            set_system_audio_mode(laddr, initiator, audio_status);
          }
          break;
        case CEC_ID_GIVE_AUDIO_STATUS:
          if (destination == laddr) {
            report_audio_status(laddr, initiator, 0x32);  // volume 50%, mute off
          }
          break;
        case CEC_ID_SET_SYSTEM_AUDIO_MODE:
          if (destination == laddr || destination == 0x0f) {
            audio_status = (msg.operand[0] == 1);
          }
          break;
        case CEC_ID_GIVE_SYSTEM_AUDIO_MODE_STATUS:
          if (destination == laddr)
            system_audio_mode_status(laddr, initiator, audio_status);
          break;
        case CEC_ID_SYSTEM_AUDIO_MODE_STATUS:
          break;
        case CEC_ID_ROUTING_CHANGE:
          active_addr = (msg.operand[2] << 8) | msg.operand[3];
          paddr = get_physical_address(&config);
          laddr = allocate_logical_address(&config);
          if (paddr == active_addr) {
            image_view_on(laddr, 0x00);
            active_source(laddr, paddr);
            no_active = 0;
          }
          break;
        case CEC_ID_ACTIVE_SOURCE:
          active_addr = (msg.operand[0] << 8) | msg.operand[1];
          no_active = 0;
          break;
        case CEC_ID_REPORT_PHYSICAL_ADDRESS:
          // On broadcast receive, do the same
          if ((initiator == 0x00) && (destination == 0x0f)) {
            paddr = get_physical_address(&config);
            laddr = allocate_logical_address(&config);
            if (paddr != 0x0000) {
              report_physical_address(laddr, 0x0f, paddr, config.device_type);
            }
          }
          break;
        case CEC_ID_REQUEST_ACTIVE_SOURCE:
          no_active++;
          if (paddr == active_addr || no_active > 2) {
            image_view_on(laddr, 0x00);
            active_source(laddr, paddr);
            no_active = 0;
          }
          break;
        case CEC_ID_SET_STREAM_PATH:
          if (paddr == ((msg.operand[0] << 8) | msg.operand[1])) {
            active_addr = paddr;
            image_view_on(laddr, 0x00);
            active_source(laddr, paddr);
            menu_state = true;
            menu_status(laddr, 0x00, menu_state);
            no_active = 0;
            blink_set_blink(BLINK_STATE_GREEN_2HZ);
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
        case CEC_ID_MENU_REQUEST:
          if (destination == laddr) {
            cec_menu_t request = msg.operand[0];
            switch (request) {
              case CEC_MENU_ACTIVATE:
                menu_state = true;
                break;
              case CEC_MENU_DEACTIVATE:
                menu_state = false;
                break;
              case CEC_MENU_QUERY:
                break;
            }
            menu_status(laddr, initiator, menu_state);
          }
          break;
        case CEC_ID_GIVE_DEVICE_POWER_STATUS:
          if (destination == laddr)
            report_power_status(laddr, initiator, active_addr != paddr);
#if 0
          /* Hack for Google Chromecast to force it sending V+/V- if no CEC TV is present */
          if (destination == 0)
            report_power_status(0, initiator, 0x00);
#endif
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
            report_physical_address(laddr, 0x0f, paddr, config.device_type);
          break;
        case CEC_ID_USER_CONTROL_PRESSED:
          if (destination == laddr) {
            blink_set(BLINK_STATE_GREEN_ON);
            command_t command = config.keymap[msg.operand[0]];
            if (command.name != NULL) {
              xQueueSend(*q, &command.key, pdMS_TO_TICKS(10));
            }
          }
          break;
        case CEC_ID_USER_CONTROL_RELEASED:
          if (destination == laddr) {
            blink_set(BLINK_STATE_OFF);
            key = HID_KEY_NONE;
            xQueueSend(*q, &key, pdMS_TO_TICKS(10));
          }
          break;
        case CEC_ID_ABORT:
          if (destination == laddr) {
            cec_feature_abort(laddr, initiator, msg.opcode, CEC_ABORT_REFUSED);
          }
          break;
        case CEC_ID_FEATURE_ABORT:
          break;
        case CEC_ID_VENDOR_COMMAND_WITH_ID:
          break;
        default:
          if (destination == laddr) {
            cec_feature_abort(laddr, initiator, msg.opcode, CEC_ABORT_UNRECOGNIZED);
          }
          break;
      }
    }
  }
}
