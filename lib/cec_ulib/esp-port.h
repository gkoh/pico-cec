#ifndef _ESP_PORT_H_
#define _ESP_PORT_H_

#include <stdint.h>

#if __INTELLISENSE__
#define __FILE_NAME__ __FILE__
#endif

#if defined(__XTENSA__) || defined(__riscv)

#include <esp_log.h>
// https://gcc.gnu.org/onlinedocs/gcc/Diagnostic-Pragmas.html
// clang-format off
#define DECLARE_TAG() \
  _Pragma("GCC diagnostic push") \
  _Pragma("GCC diagnostic ignored \"-Wunused-variable\"") \
  static const char *TAG = __FILE_NAME__; \
  _Pragma("GCC diagnostic pop")
// clang-format on

int64_t esp_timer_get_time(void);  // from esp_timer.h

#define cec_hal_time32() esp_timer_get_time()
#define cec_hal_time64() esp_timer_get_time()

#else
#error
#endif  // __XTENSA__

#endif  // _ESP_PORT_H_
