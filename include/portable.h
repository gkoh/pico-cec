#ifndef _PORTABLE_H_
#define _PORTABLE_H_

#if defined(__XTENSA__) || defined(__riscv)
#include "esp-idf.h"
#include "project_info.h"

// TODO: probably move this into esp-idf.h
#ifdef USE_USB_CDC
#include <tusb.h>
#include "tinyusb.h"
#include "tusb.h"
#endif

#ifndef DEBUG
#undef ESP_LOGE
#undef ESP_LOGW
#undef ESP_LOGI
#undef ESP_LOGD
#undef ESP_LOGV
#define ESP_LOGE(tag, fmt, ...) \
  do {                          \
  } while (0)
#define ESP_LOGW(tag, fmt, ...) \
  do {                          \
  } while (0)
#define ESP_LOGI(tag, fmt, ...) \
  do {                          \
  } while (0)
#define ESP_LOGD(tag, fmt, ...) \
  do {                          \
  } while (0)
#define ESP_LOGV(tag, fmt, ...) \
  do {                          \
  } while (0)
#endif  // DEBUG

#else  // !__XTENSA__

#include "../build/project_info.h"

#define IRAM_ATTR
#define DECLARE_TAG()
#define cec_id_event_log(a)
#define cec_id_event_log_start()

#define ESP_LOGE(tag, fmt, ...) \
  do {                          \
  } while (0)
#define ESP_LOGW(tag, fmt, ...) \
  do {                          \
  } while (0)
#define ESP_LOGI(tag, fmt, ...) \
  do {                          \
  } while (0)
#define ESP_LOGD(tag, fmt, ...) \
  do {                          \
  } while (0)
#define ESP_LOGV(tag, fmt, ...) \
  do {                          \
  } while (0)

// for blink.c
#include "bsp/board.h"
#include "pico/stdlib.h"

// for cec-config.c
#include "class/hid/hid.h"
#include "tusb.h"

// for cec-frame.c
// #include "pico/stdlib.h"

// for debug.c
#include "hardware/timer.h"
// #include "pico/stdlib.h"

// for freertos_hook.c
#include "common/tusb_common.h"

// for cec-task.c
// #include "class/hid/hid.h"
// #include "pico/stdlib.h"
// #include "tusb.h"

// for ddc.c
#include "hardware/i2c.h"
// #include "pico/stdlib.h"

// for main.c
// #include "bsp/board.h"
// #include "hardware/timer.h"
// #include "pico/stdlib.h"

// for nvs.c
#include <hardware/flash.h>
#include <hardware/sync.h>
//// #include "crc/crc32.h"

// for usb-cdc.c
#include <hardware/watchdog.h>
#include <pico/bootrom.h>
// #include <tusb.h>

// for usb_descriptors.c
// #include "tusb.h"

// for usb_hid.c
// #include "bsp/board.h"
// #include "pico/stdlib.h"
// #include "tusb.h"

// for ws2812.c
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "pico/sem.h"
// #include "pico/stdlib.h"

#endif  // __XTENSA__

#endif  // _PORTABLE_H_
