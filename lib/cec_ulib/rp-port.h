#ifndef _RP_PORT_H_
#define _RP_PORT_H_

#include <stdint.h>

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

// for cec-frame.c
#include "bsp/board.h"
#include "pico/stdlib.h"

#endif  // _RP_PORT_H_
