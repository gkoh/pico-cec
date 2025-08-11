#include "cec-hal.h"
#include "cec-frame.h"
// DECLARE_TAG()

//
// TODO: consider splitting this out into separate esp & pico source modules with common header
//

#if defined(__XTENSA__) || defined(__riscv)

#include "esp-port.h"
// #define TAG "cec-hal.c"
#include <driver/gpio.h>
#include <esp_timer.h>

static esp_timer_handle_t oneshot_timer_handle;

////////////////////////////////////////////////////////////////////////////////
// CEC gpio and interrupt control
//

bool cec_hal_bus_get(void) {
  return gpio_get_level(CEC_PIN);
}
void cec_hal_bus_high(void) {
  gpio_set_direction(CEC_PIN, GPIO_MODE_INPUT);
}
void cec_hal_bus_low(void) {
  gpio_set_direction(CEC_PIN, GPIO_MODE_OUTPUT);
}
void IRAM_ATTR cec_hal_rx_irq(uint32_t event_mask, bool enabled) {
  if (enabled) {
    gpio_set_intr_type(CEC_PIN, GPIO_INTR_ANYEDGE);
    gpio_intr_enable(CEC_PIN);
  } else {
    gpio_set_intr_type(CEC_PIN, GPIO_INTR_DISABLE);
    gpio_intr_disable(CEC_PIN);
  }
}
void IRAM_ATTR cec_hal_rx_irq_disable(void) {
  gpio_intr_disable(CEC_PIN);
}
#else
bool cec_hal_bus_get(void) {
  return gpio_get(CEC_PIN);
}
void cec_hal_bus_high(void) {
  gpio_set_dir(CEC_PIN, GPIO_IN);
}
void cec_hal_bus_low(void) {
  gpio_set_dir(CEC_PIN, GPIO_OUT);
}
void cec_hal_rx_irq(uint32_t event_mask, bool enabled) {
  gpio_set_irq_enabled(CEC_PIN, event_mask, enabled);
}
void IRAM_ATTR cec_hal_rx_irq_disable(void) {
  gpio_set_irq_enabled(CEC_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
}
#endif  // __XTENSA__

////////////////////////////////////////////////////////////////////////////////
// CEC RX gpio interrupt handling
//

static cec_frame_rx_callback_t cec_frame_rx_callback;

#if defined(__XTENSA__) || defined(__riscv)
static void esp_rx_isr(void *arg) {
  //   uint32_t gpio_num = (uint32_t)arg;

  cec_hal_rx_irq(GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
  uint32_t edge_time = esp_timer_get_time();
  cec_frame_rx_callback(edge_time);
}
#else
static void pico_rx_isr(unsigned int gpio, uint32_t events) {
  gpio_acknowledge_irq(gpio, events);
  cec_hal_rx_irq(GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
  uint32_t edge_time = time_us_64();
  cec_frame_rx_callback(edge_time);
}
#endif  // __XTENSA__

////////////////////////////////////////////////////////////////////////////////
// CEC TX timer interrupt handling
//

static cec_frame_tx_callback_t cec_frame_tx_callback = NULL;
static void *cec_frame_tx_user_data = NULL;

#if defined(__XTENSA__) || defined(__riscv)
static void IRAM_ATTR frame_tx_timer_callback(void *) {
  int64_t next = cec_frame_tx_callback(cec_frame_tx_user_data);  // call down to cec-frame layer
  if (next > 0) {                                                // delay time for next 'alarm'
    ESP_ERROR_CHECK(esp_timer_start_once(oneshot_timer_handle, next));
  }
}
#else
static int64_t IRAM_ATTR frame_tx_alarm_callback(alarm_id_t alarm, void *user_data) {
  return cec_frame_tx_callback(cec_frame_tx_user_data);  // call down to cec-frame layer
}
#endif  // __XTENSA__

void cec_hal_frame_tx(uint32_t time, cec_frame_tx_callback_t callback, void *user_data) {
  cec_frame_tx_callback = callback;
  cec_frame_tx_user_data = user_data;
#if defined(__XTENSA__) || defined(__riscv)
  if (user_data) {
    time = 0;  // fires timer immediately
  } else {     // complete the ACK pulse for the RX handler, which passes NULL for the user_data
    // WARNING: we end up here in the gpio interrupt/callback context
    time = time - esp_timer_get_time();
  }
  ESP_ERROR_CHECK(esp_timer_start_once(oneshot_timer_handle, time));
#else
  add_alarm_at(time, frame_tx_alarm_callback, user_data, true);  // pico-sdk
#endif  // __XTENSA__
}

////////////////////////////////////////////////////////////////////////////////
//
//

// for cec-util.c module to install & restore capture isr
void *cec_hal_swap_rx_isr(void *callback) {
  void *existing = cec_frame_rx_callback;
  cec_frame_rx_callback = callback;
  return existing;
}

void cec_hal_YIELD_FROM_ISR(BaseType_t woken) {
  if (woken) {
#if defined(__XTENSA__) || defined(__riscv)
    portYIELD_FROM_ISR();
#else
    portYIELD_FROM_ISR(woken);
#endif  // __XTENSA__
  }
}

void cec_hal_init(unsigned int gpio, cec_frame_rx_callback_t callback) {
  cec_frame_rx_callback = callback;
#if defined(__XTENSA__) || defined(__riscv)
  gpio_pullup_en(gpio);
  gpio_pulldown_dis(gpio);
  gpio_set_direction(gpio, GPIO_MODE_INPUT);

  const esp_timer_create_args_t oneshot_timer_args = {.callback = &frame_tx_timer_callback,
                                                      .arg = (void *)NULL,
                                                      .dispatch_method = ESP_TIMER_ISR,
                                                      .name = "one-shot"};
  ESP_ERROR_CHECK(esp_timer_create(&oneshot_timer_args, &oneshot_timer_handle));

#define ESP_INTR_FLAG_DEFAULT 0
  gpio_set_intr_type(gpio, GPIO_INTR_ANYEDGE);  // TODO: seems irrelevant at this time
  gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
  gpio_isr_handler_add(gpio, esp_rx_isr, (void *)gpio);
#else
  gpio_init(gpio);
  gpio_disable_pulls(gpio);
  gpio_set_dir(gpio, GPIO_IN);
  gpio_set_irq_callback(&pico_rx_isr);  // pico-sdk
  irq_set_enabled(IO_IRQ_BANK0, true);  // pico-sdk
#endif  // __XTENSA__
}
