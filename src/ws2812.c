// From LaserBearIndustries 2025
#include <stdint.h>
#include <stdlib.h>

#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "pico/sem.h"
#include "pico/stdlib.h"
#include "ws2812.h"
#include "ws2812.pio.h"

static semaphore_t mutex;

void ws2812_put_pixel(uint32_t pixel_grb) {
  // don't block, cannot delay CEC handling
  if (sem_try_acquire(&mutex)) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
    sem_release(&mutex);
  }
}
void ws2812_put_rgb(uint8_t red, uint8_t green, uint8_t blue) {
  uint32_t mask = (green << 16) | (red << 8) | (blue << 0);
  ws2812_put_pixel(mask);
}

void ws2812_init(unsigned int pin) {
  sem_init(&mutex, 1, 1);

  // Get the default PIO (PIO0) and allocate a state machine (SM0)
  PIO pio = pio0;
  int sm = 0;
  uint offset = pio_add_program(pio, &ws2812_program);

  ws2812_program_init(pio, sm, offset, pin, 800000, true);
}
