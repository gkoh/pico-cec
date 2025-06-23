#ifndef _PORTABLE_H_
#define _PORTABLE_H_

#ifdef __XTENSA__
#include "../lib/espidf/portable.h"

#ifndef PICO_CEC_VERSION
#define PICO_CEC_VERSION "esp32"
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

#ifndef PICO_CEC_VERSION
#define PICO_CEC_VERSION "unknown"
#endif

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

#endif  // __XTENSA__

#ifdef USE_PORTABLE
// for blink.c
#include "bsp/board.h"
#include "pico/stdlib.h"

// for cec-config.c
#include "class/hid/hid.h"
#include "tusb.h"

// for cec-frame.c
#include "pico/stdlib.h"

// for debug.c
#include "hardware/timer.h"
#include "pico/stdlib.h"

// for freertos_hook.c
#include "common/tusb_common.h"

// for hdmi-cec.c
#include "class/hid/hid.h"
#include "pico/stdlib.h"
#include "tusb.h"

// for hdmi-ddc.c
#include "hardware/i2c.h"
#include "pico/stdlib.h"

// for main.c
#include "bsp/board.h"
#include "hardware/timer.h"
#include "pico/stdlib.h"

// for nvs.c
#include <hardware/flash.h>
#include <hardware/sync.h>
//#include "crc/crc32.h"

// for usb-cdc.c
#include <hardware/watchdog.h>
#include <pico/bootrom.h>
#include <tusb.h>

// for usb_descriptors.c
#include "tusb.h"

// for usb_hid.c
#include "bsp/board.h"
#include "pico/stdlib.h"
#include "tusb.h"

// for ws2812.c
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "pico/sem.h"
#include "pico/stdlib.h"

#endif  // USE_PORTABLE

#endif  // _PORTABLE_H_
