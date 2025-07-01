#include "bsp/board.h"
#include "pico/stdlib.h"

#include "blink.h"
#include "ws2812.h"

TaskHandle_t xBlinkTask;

void blink_init(void) {
#ifdef PICO_DEFAULT_WS2812_POWER_PIN
  gpio_init(PICO_DEFAULT_WS2812_POWER_PIN);
  gpio_set_dir(PICO_DEFAULT_WS2812_POWER_PIN, GPIO_OUT);
  gpio_put(PICO_DEFAULT_WS2812_POWER_PIN, true);
#endif

  // RGB = solid green on boot
  ws2812_init(PICO_DEFAULT_WS2812_PIN);
  ws2812_put_rgb(0, 0x78, 0);

#ifdef PICO_DEFAULT_LED_PIN
  gpio_init(PICO_DEFAULT_LED_PIN);
  gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
#endif
}

void blink_set(blink_state_t state) {
  switch (state) {
    case BLINK_STATE_GREEN_ON:
      ws2812_put_rgb(0, 0x78, 0);
      break;
    case BLINK_STATE_OFF:
      ws2812_put_rgb(0, 0, 0);
      break;
    default:
      break;
  }
}

void blink_set_blink(blink_state_t state) {
  xTaskNotify(xBlinkTask, (uint32_t)state, eSetValueWithOverwrite);
}

void blink_task(void *param) {
  uint32_t blink_delay = 1000;
  bool state = true;
  blink_state_t rgb_state = BLINK_STATE_BLUE_2HZ;

  ws2812_put_rgb(0, 0, 0);

  while (true) {
    uint32_t new_rgb = 0;

    if (xTaskNotifyWait(0, UINT32_MAX, &new_rgb, 1) == pdTRUE) {
      rgb_state = new_rgb;
    }

#ifdef PICO_DEFAULT_LED_PIN
    // heartbeat
    gpio_put(PICO_DEFAULT_LED_PIN, state);
#endif

    // RGB
    if (state) {
      switch (rgb_state) {
        case BLINK_STATE_BLUE_2HZ:
          ws2812_put_rgb(0, 0, 0x78);
          break;
        case BLINK_STATE_GREEN_2HZ:
          ws2812_put_rgb(0, 0x78, 0);
          break;
        case BLINK_STATE_RED_2HZ:
          ws2812_put_rgb(0x78, 0, 0);
          break;
        default:
          // do nothing
          break;
      }
    } else {
      ws2812_put_rgb(0, 0, 0);
    }

    state = !state;

    vTaskDelay(pdMS_TO_TICKS(blink_delay));
  }
}
