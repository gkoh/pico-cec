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
#else
#define DECLARE_TAG()
#endif  // __XTENSA__

int64_t esp_timer_get_time(void);  // from esp_timer.h
#define time_us_64() esp_timer_get_time()
#define from_us_since_boot(t) (t)

#endif  // _ESP_PORT_H_
