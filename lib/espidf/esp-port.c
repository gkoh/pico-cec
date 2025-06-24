#include <inttypes.h>
#include <stdio.h>
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "driver/gpio.h"
#include "driver/uart.h"

#include "esp-port.h"
#include "portable.h"

#define UART_TXD (17)
#define UART_RXD (16)
#define UART_RTS (UART_PIN_NO_CHANGE)
#define UART_CTS (UART_PIN_NO_CHANGE)

#define UART_BAUD_RATE (115200)
#define UART_BUF_SIZE (128)

void uart_init(void) {
  uart_config_t uart_config = {
      .baud_rate = UART_BAUD_RATE,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_SCLK_DEFAULT,
  };
  int intr_alloc_flags = 0;
#if CONFIG_UART_ISR_IN_IRAM
  intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif
  ESP_ERROR_CHECK(
      uart_driver_install(UART_PORT_NUM, UART_BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
  ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
  ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, UART_TXD, UART_RXD, UART_RTS, UART_CTS));
}

void irq_set_enabled(int a, int b) {}

void IRAM_ATTR gpio_set_irq_enabled(uint gpio, uint32_t event_mask, bool enabled) {
  if (enabled) {
    if (event_mask == (GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL)) {
      gpio_set_intr_type(gpio, GPIO_INTR_ANYEDGE);
    }
    if (event_mask == (GPIO_IRQ_EDGE_RISE)) {
      gpio_set_intr_type(gpio, GPIO_INTR_POSEDGE);
    }
    if (event_mask == (GPIO_IRQ_EDGE_FALL)) {
      gpio_set_intr_type(gpio, GPIO_INTR_NEGEDGE);
    }
  } else {
    gpio_set_intr_type(gpio, GPIO_INTR_DISABLE);
  }
}

////////////////////////////////////////////////////////////////////////////////
// CEC RX interrupt handling
//
#define ESP_INTR_FLAG_DEFAULT 0

void gpio_acknowledge_irq(uint gpio, uint32_t events) {}

static gpio_irq_callback_t hdmi_rx_frame_callback;
uint64_t prev_edge_time;

//#define USE_GPIO_TASK_HANDLER // enable this to get the tx frame handling out of the interrupt
//context (for debugging)
#ifdef USE_GPIO_TASK_HANDLER
#define GPIO_STACK_SIZE (8096)
static StackType_t stackGPIO[GPIO_STACK_SIZE];
static StaticTask_t xGPIOTCB;
#define GPIO_QUEUE_LENGTH (2)  // we should only really need the one slot
static StaticQueue_t xStaticGPIOQueue;
static QueueHandle_t gpio_evt_queue = NULL;
static uint8_t storageGPIOQueue[GPIO_QUEUE_LENGTH * sizeof(uint64_t)];
static TaskHandle_t xGPIOTask;

static void gpio_task(void *arg) {
  uint64_t edge_time;
  for (;;) {
    if (xQueueReceive(gpio_evt_queue, &edge_time, portMAX_DELAY)) {
      hdmi_rx_frame_callback(edge_time);
    }
  }
}
#endif  // USE_GPIO_TASK_HANDLER

static void IRAM_ATTR gpio_isr_handler(void *arg) {
  uint32_t gpio_num = (uint32_t)arg;
  uint64_t edge_time = time_us_64();

  if (edge_time - prev_edge_time < 100)
    return;
  prev_edge_time = edge_time;

  gpio_set_intr_type(gpio_num, GPIO_INTR_DISABLE);
#ifdef USE_GPIO_TASK_HANDLER
  xQueueSendFromISR(gpio_evt_queue, &edge_time, NULL);
  portYIELD_FROM_ISR();  // indicate we need to yield at the end of ISR
#else
  hdmi_rx_frame_callback(edge_time);
#endif  // USE_GPIO_TASK_HANDLER
}

void esp_cec_rx_init(uint gpio, gpio_irq_callback_t callback) {
  hdmi_rx_frame_callback = callback;

#ifdef USE_GPIO_TASK_HANDLER
  // Create a queue to handle gpio event from isr (we could have just used a global for the
  // timestamp and an ipc signal)
  gpio_evt_queue = xQueueCreateStatic(
      GPIO_QUEUE_LENGTH, sizeof(uint64_t), &storageGPIOQueue[0],
      &xStaticGPIOQueue);  // TODO: changed from xQueueCreate to xQueueCreateStatic
  if (!gpio_evt_queue) {
    ESP_LOGE(TAG, "Creating GPIO queue failed");
    return;
  }
  // Start gpio task
  xGPIOTask = xTaskCreateStatic(gpio_task, "gpio_task", GPIO_STACK_SIZE, &gpio_evt_queue,
                                configMAX_PRIORITIES - 1, &stackGPIO[0], &xGPIOTCB);  // TODO: ditto
  (void)xGPIOTask;
#endif  // USE_GPIO_TASK_HANDLER

  gpio_set_intr_type(gpio, GPIO_INTR_ANYEDGE);
  gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
  gpio_isr_handler_add(gpio, gpio_isr_handler, (void *)gpio);
}

////////////////////////////////////////////////////////////////////////////////
// CEC frame tx timer/interrupt handling
//
static alarm_callback_t cec_alarm_callback = NULL;
static void *cec_alarm_user_data = NULL;

static esp_timer_handle_t oneshot_timer;
static int64_t last_timer_value;
extern int64_t frame_time_start;
extern char *frame_type_str;

static void oneshot_timer_callback(void *arg) {
  int64_t time_since_boot = esp_timer_get_time();
  int64_t next = cec_alarm_callback(0, cec_alarm_user_data);
  if (next > 0) {  // delay time for next alarm
    ESP_ERROR_CHECK(esp_timer_start_once(oneshot_timer, next));
  } else if (next < 0) {
    // TODO: investigate what is going on with negative delay times being returned by the callback
    ESP_ERROR_CHECK(esp_timer_start_once(oneshot_timer, 0));
    ESP_LOGE(TAG, "ERROR: timer, next: %lld us", next);
  } else {  // finished a frame tx or frame rx ack
    ESP_LOGV(TAG, "%s frame complete: %ld us", frame_type_str,
             (uint32_t)(time_since_boot - frame_time_start));
  }
  last_timer_value = time_since_boot;
}

void alarm_pool_init_default() {
  const esp_timer_create_args_t oneshot_timer_args = {
      .callback = &oneshot_timer_callback,
      /* argument specified here will be passed to timer callback function */
      .arg = (void *)NULL,
      .name = "one-shot"};
  ESP_ERROR_CHECK(esp_timer_create(&oneshot_timer_args, &oneshot_timer));
  /* The timer has been created but is not running yet */
  ESP_LOGD(TAG, "alarm_pool_init_default() created timer %p", oneshot_timer);
}

alarm_id_t IRAM_ATTR add_alarm_at(absolute_time_t time,
                                  alarm_callback_t callback,
                                  void *user_data,
                                  bool fire_if_past) {
  cec_alarm_callback = callback;
  cec_alarm_user_data = user_data;

#ifdef USE_GPIO_TASK_HANDLER
  ESP_LOGD(TAG, "add_alarm_at(%lld,, %p,)", time, user_data);
#endif  // USE_GPIO_TASK_HANDLER

  last_timer_value = esp_timer_get_time();

  if (user_data) {  // directly invoke the TX handler for start of frame

    frame_time_start = last_timer_value;
    frame_type_str = "TX";

    // ESP_LOGD(TAG, "Starting frame tx: %ld", (int32_t)(time - last_timer_value));
    ESP_ERROR_CHECK(esp_timer_start_once(oneshot_timer, 0));
  } else {  // complete the ACK pulse for the RX handler, which passes NULL for the user_data
    // WARNING: we end up here in the gpio interrupt/callback context
    // ESP_LOGD(TAG, "Frame rxack pulse: %ld", (int32_t)(time - last_timer_value));
    ESP_ERROR_CHECK(esp_timer_start_once(oneshot_timer, (time - esp_timer_get_time())));
  }
  return 0;  // unused
}

/* scary tid-bit from the silicon errata

[GPIO] Within the same group of GPIO pins, edge interrupts cannot be used together with other
interrupts Affected revisions: v0.0 v1.0 v1.1 v3.0 v3.1

Description:
GPIO0 ~ GPIO31 share a set of interrupt configuration registers and belong to one group, GPIO32 ~
GPIO39 share another set of registers and belong to another group, and RTC GPIO0 ~ GPIO17 share yet
another set of registers and belong to yet another group. If one GPIO pad within a group is
configured with edge-triggered interrupt, then other interrupts (including both edge-triggered and
level-triggered interrupts within that group cannot be configured. There is no such limitation for
level-triggered interrupts, which means, if there are no edge-triggered interrupts configured within
a group, then there can be any number of level-triggered interrupts in that group.

Reason:
When the following three sets of STATUS/W1TS/W1TC registers for GPIOs are being operated,
edge-triggered interrupts may not be properly triggered within the same group. When the following
registers are being operated, edge-triggered interrupts for GPIO_STATUS_REG may not be properly
triggered: GPIO_STATUS_W1TS_REG GPIO_STATUS_W1TC_REG GPIO_STATUS_REG

When the following registers are being operated, edge-triggered interrupts for GPIO_STATUS1_REG may
not be properly triggered: GPIO_STATUS1_W1TS_REG GPIO_STATUS1_W1TC_REG GPIO_STATUS1_REG

When the following registers are being operated, edge-triggered interrupts for
RTCIO_RTC_GPIO_STATUS_REG may not be properly triggered: RTCIO_RTC_GPIO_STATUS_W1TS_REG
  RTCIO_RTC_GPIO_STATUS_W1TC_REG
  RTCIO_RTC_GPIO_STATUS_REG

Workarounds:
Simulate edge-triggered interrupts using level-triggered interrupts, as outlined below.
To trigger a GPIO interrupt on a rising edge, follow the steps:
Set the GPIO interrupt type to high.
After the CPU services the interrupt, change the GPIO interrupt type to low. A second interrupt
occurs at this time, and the CPU needs to ignore the interrupt service routine. To trigger a GPIO
interrupt on a falling edge, follow the steps: Set the GPIO interrupt type to low. After the CPU
services the interrupt, change the GPIO interrupt type to high. A second interrupt occurs at this
time, and the CPU needs to ignore the interrupt service routine.

Solution:
No fix scheduled.
*/

/* maybe install a direct isr handler to avoid all the delay introduced by the framework one..

.section .iram1,"ax"
 .global     xt_highint5
 .type       xt_highint5,@function
 .align      4
 .literal .GPIO_STATUS_W1TC_REG, 0x3FF4404C
 .literal .GPIO_OUT_W1TS_REG, 0x3FF44008
 .literal .GPIO_OUT_W1TC_REG, 0x3FF4400C
 .literal .GPIO__NUM_15, (1<<15)
 .literal .GPIO__NUM_4,  (1<<4)
xt_highint5:
l32r a14, .GPIO_OUT_W1TS_REG
l32r a15, .GPIO__NUM_15
s32i a15, a14, 0
l32r a14, .GPIO_STATUS_W1TC_REG
l32r a15, .GPIO__NUM_4
s32i a15, a14, 0
rsr     a0, EXCSAVE_5
rfi     5
*/
