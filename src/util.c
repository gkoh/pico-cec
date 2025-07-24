#include <pico/stdlib.h>

#include "pico-cec/util.h"

/**
 * Get milliseconds since boot. (~ since the log task started)
 */
uint64_t util_uptime_ms(void) {
  return (time_us_64() / 1000);
}
