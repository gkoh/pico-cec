// clang-format off

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "cec-hal.h"
DECLARE_TAG()

#include "cec-config.h"
#include "cec-frame.h"
#include "cec-id.h"
#include "cec-log.h"
#include "cec-task.h"

#define _LOG_BR "\r\n"
#define CEC_QUEUE_LENGTH (16)

// #define CEC_TASK_TRACE
#ifndef CEC_TASK_TRACE
// #undef ESP_LOGE
#undef ESP_LOGW
#undef ESP_LOGI
#undef ESP_LOGD
#undef ESP_LOGV
// #define ESP_LOGE(tag, fmt, ...) do {} while (0)
#define ESP_LOGW(tag, fmt, ...) \
  do {                          \
  } while (0)
#define ESP_LOGI(tag, fmt, ...) \
  do {                          \
  } while (0)
#define ESP_LOGD(tag, fmt, ...) do {} while (0)
#define ESP_LOGV(tag, fmt, ...) do {} while (0)
#endif  // CEC_TASK_TRACE

/** The running CEC configuration. */
static cec_config_t cec_config = {0x0};

/** The status of the CEC connection. */
static bool cec_active = false;

bool cec_get_status(void) {
  return cec_active;
}

// HDMI logical addresses
// 2 dimensional array of valid logical addresses for playback and recording
// only.
#define NUM_LADDRESS 4
#define NUM_TYPES 6
static const uint8_t laddress[NUM_TYPES][NUM_LADDRESS] = {
    {0x00, 0x0e, 0x00, 0x00},  // TV
    {0x01, 0x02, 0x09, 0x0e},  // Recording Device
    {0x0f, 0x0f, 0x0f, 0x0f},  // Reserved
    {0x03, 0x06, 0x07, 0x0e},  // Tuner + 0x0a
    {0x04, 0x08, 0x0b, 0x0e},  // Playback Device
    {0x05, 0x0e, 0x0e, 0x0e},  // Audio System
};

// clang-format off
//
// typedef enum {
//     CDT_TV,
//     CDT_RECORDING_DEVICE,
//     CDT_PLAYBACK_DEVICE,
//     CDT_TUNER,
//     CDT_AUDIO_SYSTEM,
//     CDT_OTHER,                   // Not a real CEC type..
// } CEC_DEVICE_TYPE;
//
// typedef enum {
//     CLA_TV                 = 0,  // (0x00)
//     CLA_RECORDING_DEVICE_1 = 1,  // (0x01)
//     CLA_RECORDING_DEVICE_2 = 2,  // (0x02)
//     CLA_TUNER_1            = 3,  // (0x03)
//     CLA_PLAYBACK_DEVICE_1  = 4,  // (0x04)
//     CLA_AUDIO_SYSTEM       = 5,  // (0x05)
//     CLA_TUNER_2            = 6,  // (0x06)
//     CLA_TUNER_3            = 7,  // (0x07)
//     CLA_PLAYBACK_DEVICE_2  = 8,  // (0x08)
//     CLA_RECORDING_DEVICE_3 = 9,  // (0x09)
//     CLA_TUNER_4            = 10, // (0x0a)
//     CLA_PLAYBACK_DEVICE_3  = 11, // (0x0b)
//     CLA_RESERVED_1         = 12, // (0x0c)
//     CLA_RESERVED_2         = 13, // (0x0d)
//     CLA_FREE_USE           = 14, // (0x0e)
//     CLA_UNREGISTERED       = 15, // (0x0f)
// } CEC_LOGICAL_ADDRESS;
//
// int CEC_LogicalDevice::_validLogicalAddresses[6][5] = {
//  {CLA_TV,                  CLA_FREE_USE,           CLA_UNREGISTERED,       CLA_UNREGISTERED, CLA_UNREGISTERED, },
//  {CLA_RECORDING_DEVICE_1,  CLA_RECORDING_DEVICE_2, CLA_RECORDING_DEVICE_3, CLA_UNREGISTERED, CLA_UNREGISTERED, },
//  {CLA_PLAYBACK_DEVICE_1,   CLA_PLAYBACK_DEVICE_2,  CLA_PLAYBACK_DEVICE_3,  CLA_UNREGISTERED, CLA_UNREGISTERED, },
//  {CLA_TUNER_1,             CLA_TUNER_2,            CLA_TUNER_3,            CLA_TUNER_4,      CLA_UNREGISTERED, },
//  {CLA_AUDIO_SYSTEM,        CLA_UNREGISTERED,       CLA_UNREGISTERED,       CLA_UNREGISTERED, CLA_UNREGISTERED, },
//  {CLA_UNREGISTERED,        CLA_UNREGISTERED,       CLA_UNREGISTERED,       CLA_UNREGISTERED, CLA_UNREGISTERED, },
// };
//
// clang-format on

/* The HDMI address for this device.  Respond to CEC sent to this address. */
static uint8_t laddr = 0x0f;

/* The HDMI physical address. */
static uint16_t paddr = 0x0000;

/* Active state. */
static uint16_t active_addr = 0x0000;

/* Audio state. */
static bool audio_status = false;

// void save_logical_address(uint8_t addr) __attribute__((weak));
// uint8_t load_logical_address(void) __attribute__((weak));
void __attribute__((weak)) save_logical_address(uint8_t addr) {
  laddr = addr;
}
uint8_t __attribute__((weak)) load_logical_address(void) {
  return laddr;
}
uint16_t __attribute__((weak)) ddc_get_physical_address(void) {
  return 0;
}

static void cec_echo_respond(uint8_t initiator,
                             uint8_t destination,
                             uint8_t msg,
                             cec_abort_t reason) {
  uint8_t pld[4] = {HEADER0(initiator, destination), CEC_ID_ECHO_RESPOND, msg, reason};

  ESP_LOGI(TAG, "cec_echo_respond(%d, %d, %d, %d)", initiator, destination, msg, reason);
  cec_frame_send(4, pld, false);
}

static void cec_feature_abort(uint8_t initiator,
                              uint8_t destination,
                              uint8_t msg,
                              cec_abort_t reason) {
  uint8_t pld[4] = {HEADER0(initiator, destination), CEC_ID_FEATURE_ABORT, msg, reason};

  ESP_LOGI(TAG, "cec_feature_abort(%d, %d, %d, %d)", initiator, destination, msg, reason);
  cec_frame_send(4, pld, false);
}

static void device_vendor_id(uint8_t initiator, uint8_t destination, uint32_t vendor_id) {
  uint8_t pld[5] = {HEADER0(initiator, destination), CEC_ID_DEVICE_VENDOR_ID,
                    (vendor_id >> 16) & 0x0ff, (vendor_id >> 8) & 0x0ff, (vendor_id >> 0) & 0x0ff};

  ESP_LOGD(TAG, "device_vendor_id(%d, %d, %ld)", initiator, destination, vendor_id);
  cec_frame_send(5, pld, false);
}

static void report_power_status(uint8_t initiator, uint8_t destination, uint8_t power_status) {
  uint8_t pld[3] = {HEADER0(initiator, destination), CEC_ID_REPORT_POWER_STATUS, power_status};

  ESP_LOGI(TAG, "report_power_status(%d, %d, %d)", initiator, destination, power_status);
  cec_frame_send(3, pld, false);
}

static void set_system_audio_mode(uint8_t initiator,
                                  uint8_t destination,
                                  uint8_t system_audio_mode) {
  uint8_t pld[3] = {HEADER0(initiator, destination), CEC_ID_SET_SYSTEM_AUDIO_MODE,
                    system_audio_mode};

  ESP_LOGI(TAG, "set_system_audio_mode(%d, %d, %d)", initiator, destination, system_audio_mode);
  cec_frame_send(3, pld, false);
}

static void report_audio_status(uint8_t initiator, uint8_t destination, uint8_t audio_status) {
  uint8_t pld[3] = {HEADER0(initiator, destination), CEC_ID_REPORT_AUDIO_STATUS, audio_status};

  ESP_LOGI(TAG, "report_audio_status(%d, %d, %d)", initiator, destination, audio_status);
  cec_frame_send(3, pld, false);
}

static void system_audio_mode_status(uint8_t initiator,
                                     uint8_t destination,
                                     uint8_t system_audio_mode_status) {
  uint8_t pld[3] = {HEADER0(initiator, destination), CEC_ID_SYSTEM_AUDIO_MODE_STATUS,
                    system_audio_mode_status};

  ESP_LOGI(TAG, "system_audio_mode_status(%d, %d, %d)", initiator, destination,
           system_audio_mode_status);
  cec_frame_send(3, pld, false);
}

static void set_osd_name(uint8_t initiator, uint8_t destination) {
  uint8_t pld[10] = {
      HEADER0(initiator, destination), CEC_ID_SET_OSD_NAME, 'P', 'i', 'c', 'o', '-', 'C', 'E', 'C'};

  ESP_LOGI(TAG, "set_osd_name(%d, %d)", initiator, destination);
  cec_frame_send(10, pld, false);
}

static void report_physical_address(uint8_t initiator,
                                    uint8_t destination,
                                    uint16_t physical_address,
                                    uint8_t device_type) {
  uint8_t pld[5] = {HEADER0(initiator, destination), CEC_ID_REPORT_PHYSICAL_ADDRESS,
                    (physical_address >> 8) & 0x0ff, (physical_address >> 0) & 0x0ff, device_type};

  ESP_LOGD(TAG, "report_physical_address(%d, %d, %d, %d)", initiator, destination, physical_address,
           device_type);
  cec_frame_send(5, pld, false);
}

static void report_cec_version(uint8_t initiator, uint8_t destination) {
  // 0x04 = 1.3a
  uint8_t pld[3] = {HEADER0(initiator, destination), CEC_ID_CEC_VERSION, 0x04};

  ESP_LOGI(TAG, "report_cec_version(%d, %d)", initiator, destination);
  cec_frame_send(3, pld, false);
}

static void image_view_on(uint8_t initiator, uint8_t destination) {
  uint8_t pld[2] = {HEADER0(initiator, destination), CEC_ID_IMAGE_VIEW_ON};

  ESP_LOGI(TAG, "image_view_on(%d, %d)", initiator, destination);
  cec_frame_send(2, pld, false);
}

static void active_source(uint8_t initiator, uint16_t physical_address) {
  uint8_t pld[4] = {HEADER0(initiator, 0x0f), CEC_ID_ACTIVE_SOURCE, (physical_address >> 8) & 0x0ff,
                    (physical_address >> 0) & 0x0ff};

  ESP_LOGI(TAG, "active_source(%d, %d)", initiator, physical_address);
  cec_frame_send(4, pld, false);
}

static uint8_t allocate_logical_address(cec_config_t *config) {
  if (config->logical_address != 0x00 && config->logical_address != 0x0f) {
    return config->logical_address;
  }
  // Treat 0x00 or 0x0f as auto-allocate
  uint8_t a;
#if 1
  int i = 0;
  int j = 0;
  uint8_t addr = load_logical_address();
  ESP_LOGD(TAG, "Loaded saved logical address %u", addr);
  if (addr == 0x00 || addr == 0xff) {
    addr = laddress[config->device_type][0];
    ESP_LOGD(TAG, "Defaulting to address %u at index 0", addr);
  }

  for (i = NUM_LADDRESS; i > 0; i--) {
    if (addr == laddress[config->device_type][i - 1]) {
      ESP_LOGD(TAG, "Found our current logical address at device_type index %d", i - 1);
      break;
    }
  }
  do {
    a = laddress[config->device_type][i - 1];
    // cec_log_submitf("Attempting to allocate logical address 0x%01hhx"_LOG_BR, a);
    ESP_LOGI(TAG, "Attempting to allocate logical address 0x%02x", a);
    if (!cec_frame_ping(a)) {
      ESP_LOGD(TAG, "cec_frame_ping(0x%02x) NACK", a);
      if (a != addr) {
        save_logical_address(a);
      }
      break;
    } else {
      ESP_LOGD(TAG, "cec_frame_ping(0x%02x) ACK", a);
    }
    if (++i >= NUM_LADDRESS)
      i = 0;
  } while (++j < NUM_LADDRESS);
#else
  for (unsigned int i = 0; i < NUM_LADDRESS; i++) {
    a = laddress[config->device_type][i];
    cec_log_submitf("Attempting to allocate logical address 0x%01hhx" _LOG_BR, a);
    ESP_LOGI(TAG, "Attempting to allocate logical address 0x%01hhx", a);
    if (!cec_frame_ping(a)) {
      ESP_LOGI(TAG, "cec_frame_ping(0x%01hhx) NACK", a);
      break;
    } else {
      ESP_LOGI(TAG, "cec_frame_ping(0x%01hhx) ACK", a);
    }
  }
#endif
  ESP_LOGI(TAG, "Allocated logical address 0x%02x", a);
  return a;
}

static uint16_t get_physical_address(const cec_config_t *config) {
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

  if (cec_config.monitor_mode) {  // TODO: we may yet support multiple monitor modes
    cec_frame_set_monitor_mode(true);
  } else {
    cec_frame_set_monitor_mode(false);
  }
  
  // start the cec protocol event id log task (esp port only)
  cec_id_event_log_start();

  // pause for EDID to settle
  vTaskDelay(pdMS_TO_TICKS(cec_config.edid_delay_ms));

  cec_frame_init(cec_config.gpio_pin);

  paddr = get_physical_address(&cec_config);
  ESP_LOGI(TAG, "physical address: %d", paddr);
  cec_log_submitf("physical address: %d", paddr);
  laddr = allocate_logical_address(&cec_config);
  ESP_LOGI(TAG, "logical address: %d", laddr);
  cec_log_submitf("logical address: %d", laddr);

  while (true) {
    uint8_t pld[16] = {0x0};
    uint8_t pldcnt;
    uint8_t initiator, destination;
    uint8_t user_control = 0xFF;  // magic 'no-op' number, as zero is valid control code
    uint8_t no_active = 0;

    pldcnt = cec_frame_recv(pld, laddr);
    // printf("pldcnt = %u\n", pldcnt);
    // ESP_LOGD(TAG, "pldcnt = %u", pldcnt);
    initiator = (pld[0] & 0xf0) >> 4;
    destination = pld[0] & 0x0f;

    if ((pldcnt > 1)) {
      // ESP_LOGD(TAG, "pldcnt = %u, pld[1] = %u", pldcnt, pld[1]);
      switch (pld[1]) {
        case CEC_ID_IMAGE_VIEW_ON:
          break;
        case CEC_ID_TEXT_VIEW_ON:
          break;
        case CEC_ID_STANDBY:
          if (destination == laddr || destination == 0x0f) {
            active_addr = 0x0000;
            // blink_set_blink(BLINK_STATE_BLUE_2HZ);
            cec_active = false;
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
          paddr = get_physical_address(&cec_config);
          laddr = allocate_logical_address(&cec_config);
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
            paddr = get_physical_address(&cec_config);
            laddr = allocate_logical_address(&cec_config);  // TODO: why are we doing this?
            if (paddr != 0x0000) {
              report_physical_address(laddr, 0x0f, paddr, cec_config.device_type);
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
            // blink_set_blink(BLINK_STATE_GREEN_2HZ);
            cec_active = true;
          }
          break;
        case CEC_ID_DEVICE_VENDOR_ID:
          // On broadcast receive, do the same
          if ((initiator == 0x00) && (destination == 0x0f)) {
            device_vendor_id(laddr, 0x0f, cec_config.vendor_id);
          }
          break;
        case CEC_ID_GIVE_DEVICE_VENDOR_ID:
          if (destination == laddr)
            device_vendor_id(laddr, 0x0f, cec_config.vendor_id);
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
            report_physical_address(laddr, 0x0f, paddr, cec_config.device_type);
          break;
        case CEC_ID_USER_CONTROL_PRESSED:
          if (destination == laddr) {
            user_control = pld[2];
            xQueueSend(*q, &user_control, pdMS_TO_TICKS(10));
          }
          break;
        case CEC_ID_USER_CONTROL_RELEASED:
          if (destination == laddr) {
            user_control = 0xFF;  // magic 'no-user_control' number, as zero is valid
            xQueueSend(*q, &user_control, pdMS_TO_TICKS(10));
          }
          break;
        case CEC_ID_ECHO_REQUEST:
          if (destination == laddr) {
            cec_echo_respond(laddr, initiator, pld[1], CEC_ID_ECHO_RESPOND);
          }
          break;
        case CEC_ID_ECHO_RESPOND:
          if (destination == laddr) {
            // cec_echo_respond(laddr, initiator, pld[1], CEC_ID_ECHO_REQUEST);
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
      cec_id_event_log(pld[1]);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

TaskHandle_t xCECTask;  // referenced internally by cec-frame.c
// static QueueHandle_t xCECQueue;
static QueueHandle_t cec_q;

static StaticQueue_t xCECQueue;
static uint8_t storageCECQueue[CEC_QUEUE_LENGTH * sizeof(uint8_t)];
static StackType_t stackCEC[CEC_STACK_SIZE];
static StaticTask_t xCECTCB;  // we don't really need this, currently unused

bool cec_write(uint8_t *data, uint8_t len) {
  return cec_frame_send(len, data, true);
}

// // TODO: if we are providing a raw write, perhaps we should also implement a raw read
// int cec_read(uint8_t *buf, uint8_t size) {
//   // Our stack cannot essentially operate as a stream device, so this could be futile..
//   return 0;
// }

bool cec_read(uint8_t *user_control, int timeout_ticks) {
  if (cec_q != NULL) {
    BaseType_t r = xQueueReceive(cec_q, user_control, timeout_ticks);
    if (r == pdTRUE) {
      return true;
    }
  } else {
    vTaskDelay(timeout_ticks);
  }
  return false;
}

bool cec_init(cec_config_t config, log_callback_t log_callback) {
  cec_config = config;
  cec_q = xQueueCreateStatic(CEC_QUEUE_LENGTH, sizeof(uint8_t), &storageCECQueue[0], &xCECQueue);
  configASSERT((cec_q));
  xCECTask = xTaskCreateStatic(cec_task, CEC_TASK_NAME, CEC_STACK_SIZE, &cec_q, CEC_PRIORITY,
                               &stackCEC[0], &xCECTCB);
  if (log_callback != NULL) {
    cec_log_init(log_callback);
  }
  return true;
}

bool cec_gpio(unsigned int gpio) {
  cec_config_set_default(&cec_config);
  cec_config.gpio_pin = gpio;
  return cec_init(cec_config, NULL);
}
