#include <driver/gpio.h>
#include <driver/uart.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_timer.h>

#include "options.h"

#ifdef USE_GPIO_TASK_HANDLER
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

#include "esp-port.h"
#include "portable.h"
DECLARE_TAG()

#include "sdkconfig.h"

#define UART_TXD (CONFIG_UART_TXD)
#define UART_RXD (CONFIG_UART_RXD)
#define UART_RTS (UART_PIN_NO_CHANGE)
#define UART_CTS (UART_PIN_NO_CHANGE)

#define UART_BAUD_RATE (CONFIG_UART_BAUD_RATE)
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

////////////////////////////////////////////////////////////////////////////////
// CEC RX interrupt handling
//
#define ESP_INTR_FLAG_DEFAULT 0

static gpio_irq_callback_t gpio_irq_callback;
uint64_t prev_edge_time;

// take the gpio interrupt handling out of the interrupt context (for debugging)
#ifdef USE_GPIO_TASK_HANDLER

#ifdef USE_GPIO_INTERRUPT_LEVEL_TRIGGERED
#pragma GCC error "Probably cannot use GPIO task handler with level detect interrupts enabled"
#endif

#define GPIO_STACK_SIZE (8096)
static StackType_t stackGPIO[GPIO_STACK_SIZE];
static StaticTask_t xGPIOTCB;

#ifdef USE_GPIO_TASK_QUEUE
#define GPIO_QUEUE_LENGTH (2)  // we should only really need the one slot
static StaticQueue_t xStaticGPIOQueue;
static QueueHandle_t gpio_evt_queue = NULL;
static uint8_t storageGPIOQueue[GPIO_QUEUE_LENGTH * sizeof(uint64_t)];
#else
#define NOTIFY_GPIO ((UBaseType_t)0)
#endif  // USE_GPIO_TASK_QUEUE

static TaskHandle_t xGPIOTask;

static void gpio_task(void *arg) {
  uint64_t edge_time;
  for (;;) {
#ifdef USE_GPIO_TASK_QUEUE
    if (xQueueReceive(gpio_evt_queue, &edge_time, portMAX_DELAY)) {
      gpio_irq_callback(edge_time);
    }
#else
    edge_time = ulTaskNotifyTakeIndexed(NOTIFY_GPIO, pdTRUE, portMAX_DELAY);
    if (edge_time > 0) {
      gpio_irq_callback(edge_time);
    }
#endif  // USE_GPIO_TASK_QUEUE
  }
}
#endif  // USE_GPIO_TASK_HANDLER

static void IRAM_ATTR gpio_isr_handler(void *arg) {
  uint32_t gpio_num = (uint32_t)arg;
  uint64_t edge_time = esp_timer_get_time();

  gpio_set_intr_type(gpio_num, GPIO_INTR_DISABLE);
#ifdef USE_GPIO_TASK_HANDLER
  BaseType_t pxHigherPriorityTaskWoken;
#ifdef USE_GPIO_TASK_QUEUE
  xQueueSendFromISR(gpio_evt_queue, &edge_time, &pxHigherPriorityTaskWoken);
#else
  xTaskNotifyIndexedFromISR(xGPIOTask, NOTIFY_GPIO, edge_time, eSetValueWithOverwrite,
                            &pxHigherPriorityTaskWoken);
#endif  // USE_GPIO_TASK_QUEUE
  if (pdTRUE == pxHigherPriorityTaskWoken) {
    portYIELD_FROM_ISR();  // indicate we need to yield at the end of ISR
  }
#else
  gpio_irq_callback(edge_time);
#endif  // USE_GPIO_TASK_HANDLER
}

void gpio_set_irq_callback(gpio_irq_callback_t callback) {
  gpio_irq_callback = callback;
}

void gpio_isr_init(unsigned int gpio, gpio_irq_callback_t callback) {
  gpio_irq_callback = callback;

#ifdef USE_GPIO_TASK_HANDLER
  // Create a queue to handle gpio event from isr (we could have just used a global for the
  // timestamp and an ipc signal)
#ifdef USE_GPIO_TASK_QUEUE
  gpio_evt_queue = xQueueCreateStatic(
      GPIO_QUEUE_LENGTH, sizeof(uint64_t), &storageGPIOQueue[0],
      &xStaticGPIOQueue);
  if (!gpio_evt_queue) {
    ESP_LOGE(TAG, "Creating GPIO queue failed");
    return;
  }
  xGPIOTask = xTaskCreateStatic(gpio_task, "gpio_task", GPIO_STACK_SIZE, &gpio_evt_queue,
                                configMAX_PRIORITIES - 1, &stackGPIO[0], &xGPIOTCB);
#else
  xGPIOTask = xTaskCreateStatic(gpio_task, "gpio_task", GPIO_STACK_SIZE, NULL,
                                configMAX_PRIORITIES - 1, &stackGPIO[0], &xGPIOTCB);
#endif
  (void)xGPIOTask;
#endif  // USE_GPIO_TASK_HANDLER

  gpio_set_intr_type(gpio, GPIO_INTR_ANYEDGE);  // TODO: seems irrelevant at this time
  gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
  gpio_isr_handler_add(gpio, gpio_isr_handler, (void *)gpio);
}

////////////////////////////////////////////////////////////////////////////////
// Support for cec frame tx timer/interrupt handling
//
static timer_callback_t timer_callback = NULL;
static void *timer_user_data = NULL;

static esp_timer_handle_t oneshot_timer_handle;
static int64_t last_timer_value;

// int64_t frame_time_start;  // temporary for debugging diagnostics only
// char *frame_type_str;      // temporary for debugging diagnostics only

// This callback is fixed in the timer initialisation, so we use a second callback
//  pointer to allow the application code to configure which 'alarm' it wants invoked
static void IRAM_ATTR oneshot_timer_callback(void *arg) {
  int64_t time_since_boot = esp_timer_get_time();
  int64_t next = timer_callback(0, timer_user_data);  // call down to cec-frame layer
  if (next > 0) {                                     // delay time for next 'alarm'
    ESP_ERROR_CHECK(esp_timer_start_once(oneshot_timer_handle, next));
  } else if (next < 0) {
    // TODO: investigate what is going on with negative delay times being returned by the callback
    // (update: now fixed, so this could be removed) (was caused by calling ESP_LOGx from callback)
    ESP_ERROR_CHECK(esp_timer_start_once(oneshot_timer_handle, 0));
    ESP_LOGE(TAG, "ERROR: timer, next: %lld us", next);
  } else {  // finished a frame tx or frame rx ack
    // ESP_LOGV(TAG, "%s frame complete: %ld us", frame_type_str,
    //          (uint32_t)(time_since_boot - frame_time_start));
  }
  last_timer_value = time_since_boot;
}

void timer_init(void) {
  const esp_timer_create_args_t oneshot_timer_args = {
      .callback = &oneshot_timer_callback,
      /* argument specified here will be passed to timer callback function */
      .arg = (void *)NULL,
      .dispatch_method = ESP_TIMER_ISR,
      .name = "one-shot"};
  ESP_ERROR_CHECK(esp_timer_create(&oneshot_timer_args, &oneshot_timer_handle));
  /* The timer has been created but is not running yet */
  ESP_LOGD(TAG, "esp_timer_init() created timer %p", oneshot_timer_handle);
}

void IRAM_ATTR timer_start(uint64_t time, timer_callback_t callback, void *user_data) {
  timer_callback = callback;
  timer_user_data = user_data;

#ifdef USE_GPIO_TASK_HANDLER
//  ESP_LOGD(TAG, "add_alarm_at(%lu,, %p,)", (uint32_t)time, user_data);
#endif  // USE_GPIO_TASK_HANDLER

  last_timer_value = esp_timer_get_time();

  if (user_data) {

    // frame_time_start = last_timer_value;
    // frame_type_str = "TX";

    // ESP_LOGD(TAG, "Starting frame tx: %ld", (int32_t)(time - last_timer_value));
    ESP_ERROR_CHECK(esp_timer_start_once(oneshot_timer_handle, 0));  // fires timer immediately
  } else {  // complete the ACK pulse for the RX handler, which passes NULL for the user_data
    // WARNING: we end up here in the gpio interrupt/callback context
    // ESP_LOGD(TAG, "Frame rxack pulse: %ld", (int32_t)(time - last_timer_value));
    ESP_ERROR_CHECK(esp_timer_start_once(oneshot_timer_handle, (time - esp_timer_get_time())));
  }
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
