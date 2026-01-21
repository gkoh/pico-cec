#ifndef _RP_PORT_H_
#define _RP_PORT_H_

#include <stdint.h>

//
// define away things which are specific to the esp port
//
#define IRAM_ATTR  // declares to the esp-idf linker to place function in non-cached program memory
#define DECLARE_TAG()  // places a const string tag in each module for use with ESP_LOGx macros

// support for asynchronous output of high level cec protocol events to the esp-idf monitor
// which is not implemented in the pico-pi build
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

// for cec-frame.c
#include "bsp/board.h"
#include "pico/stdlib.h"

#define cec_hal_time32() time_us_64()
#define cec_hal_time64() time_us_64()

#endif  // _RP_PORT_H_
