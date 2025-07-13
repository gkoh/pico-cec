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
 * Based (mostly ripped) from the Arduino version by Szymon Slupik:
 * https://github.com/SzymonSlupik/CEC-Tiny-Pro
 * which itself is based on the original code by Thomas Sowell:
 * https://github.com/tsowell/avr-hdmi-cec-volume/tree/master
 */

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

/* Construct the frame address header. */
#define HEADER0(iaddr, daddr) ((iaddr << 4) | daddr)

static void cec_feature_abort(uint8_t initiator,
                              uint8_t destination,
                              uint8_t msg,
                              cec_abort_t reason) {
  uint8_t pld[4] = {HEADER0(initiator, destination), CEC_ID_FEATURE_ABORT, msg, reason};

  cec_frame_send(4, pld);
}

static void device_vendor_id(uint8_t initiator, uint8_t destination, uint32_t vendor_id) {
  uint8_t pld[5] = {HEADER0(initiator, destination), CEC_ID_DEVICE_VENDOR_ID,
                    (vendor_id >> 16) & 0x0ff, (vendor_id >> 8) & 0x0ff, (vendor_id >> 0) & 0x0ff};

  cec_frame_send(5, pld);
}

static void report_power_status(uint8_t initiator, uint8_t destination, uint8_t power_status) {
  uint8_t pld[3] = {HEADER0(initiator, destination), CEC_ID_REPORT_POWER_STATUS, power_status};

  cec_frame_send(3, pld);
}

static void set_system_audio_mode(uint8_t initiator,
                                  uint8_t destination,
                                  uint8_t system_audio_mode) {
  uint8_t pld[3] = {HEADER0(initiator, destination), CEC_ID_SET_SYSTEM_AUDIO_MODE,
                    system_audio_mode};

  cec_frame_send(3, pld);
}

static void report_audio_status(uint8_t initiator, uint8_t destination, uint8_t audio_status) {
  uint8_t pld[3] = {HEADER0(initiator, destination), CEC_ID_REPORT_AUDIO_STATUS, audio_status};

  cec_frame_send(3, pld);
}

static void system_audio_mode_status(uint8_t initiator,
                                     uint8_t destination,
                                     uint8_t system_audio_mode_status) {
  uint8_t pld[3] = {HEADER0(initiator, destination), CEC_ID_SYSTEM_AUDIO_MODE_STATUS,
                    system_audio_mode_status};

  cec_frame_send(3, pld);
}

static void set_osd_name(uint8_t initiator, uint8_t destination) {
  uint8_t pld[10] = {
      HEADER0(initiator, destination), CEC_ID_SET_OSD_NAME, 'P', 'i', 'c', 'o', '-', 'C', 'E', 'C'};

  cec_frame_send(10, pld);
}

static void report_physical_address(uint8_t initiator,
                                    uint8_t destination,
                                    uint16_t physical_address,
                                    uint8_t device_type) {
  uint8_t pld[5] = {HEADER0(initiator, destination), CEC_ID_REPORT_PHYSICAL_ADDRESS,
                    (physical_address >> 8) & 0x0ff, (physical_address >> 0) & 0x0ff, device_type};

  cec_frame_send(5, pld);
}

static void report_cec_version(uint8_t initiator, uint8_t destination) {
  // 0x04 = 1.3a
  uint8_t pld[3] = {HEADER0(initiator, destination), CEC_ID_CEC_VERSION, 0x04};
  cec_frame_send(3, pld);
}

bool cec_ping(uint8_t destination) {
  uint8_t pld[1] = {HEADER0(destination, destination)};

  return cec_frame_send(1, pld);
}

static void image_view_on(uint8_t initiator, uint8_t destination) {
  uint8_t pld[2] = {HEADER0(initiator, destination), CEC_ID_IMAGE_VIEW_ON};

  cec_frame_send(2, pld);
}

static void active_source(uint8_t initiator, uint16_t physical_address) {
  uint8_t pld[4] = {HEADER0(initiator, 0x0f), CEC_ID_ACTIVE_SOURCE, (physical_address >> 8) & 0x0ff,
                    (physical_address >> 0) & 0x0ff};

  cec_frame_send(4, pld);
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

  // load configuration
  nvs_load_config(&config);

  // pause for EDID to settle
  vTaskDelay(pdMS_TO_TICKS(config.edid_delay_ms));

  cec_frame_init();

  paddr = get_physical_address(&config);
  laddr = allocate_logical_address(&config);

  while (true) {
    uint8_t pld[16] = {0x0};
    uint8_t pldcnt;
    uint8_t initiator, destination;
    uint8_t key = HID_KEY_NONE;
    uint8_t no_active = 0;

    pldcnt = cec_frame_recv(pld, laddr);
    // printf("pldcnt = %u\n", pldcnt);
    initiator = (pld[0] & 0xf0) >> 4;
    destination = pld[0] & 0x0f;

    if ((pldcnt > 1)) {
      switch (pld[1]) {
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
            audio_status = (pld[2] == 1);
          }
          break;
        case CEC_ID_GIVE_SYSTEM_AUDIO_MODE_STATUS:
          if (destination == laddr)
            system_audio_mode_status(laddr, initiator, audio_status);
          break;
        case CEC_ID_SYSTEM_AUDIO_MODE_STATUS:
          break;
        case CEC_ID_ROUTING_CHANGE:
          // uint16_t old_addr = (pld[2] << 8) | pld[3];
          active_addr = (pld[4] << 8) | pld[5];
          paddr = get_physical_address(&config);
          laddr = allocate_logical_address(&config);
          if (paddr == active_addr) {
            image_view_on(laddr, 0x00);
            active_source(laddr, paddr);
            no_active = 0;
          }
          break;
        case CEC_ID_ACTIVE_SOURCE:
          active_addr = (pld[2] << 8) | pld[3];
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
          if (paddr == ((pld[2] << 8) | pld[3])) {
            active_addr = paddr;
            image_view_on(laddr, 0x00);
            active_source(laddr, paddr);
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
            command_t command = config.keymap[pld[2]];
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
            cec_feature_abort(laddr, initiator, pld[1], CEC_ABORT_REFUSED);
          }
          break;
        case CEC_ID_FEATURE_ABORT:
          break;
        case CEC_ID_VENDOR_COMMAND_WITH_ID:
          break;
        default:
          if (destination == laddr) {
            cec_feature_abort(laddr, initiator, pld[1], CEC_ABORT_UNRECOGNIZED);
          }
          break;
      }
    }
  }
}
