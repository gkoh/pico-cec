#include <stdio.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "hardware/timer.h"
#include "pico/stdlib.h"

#include "cec-frame.h"
#include "cec-task.h"

#define BLINK_STACK_SIZE (128)
#define CEC_STACK_SIZE (512)
#define CEC_QUEUE_LENGTH (16)

static void blink_task(void *param) {
  static uint32_t blink_delay = 1000;
  static bool state = true;

  while (true) {
    gpio_put(PICO_DEFAULT_LED_PIN, state);
    state = !state;
    vTaskDelay(pdMS_TO_TICKS(blink_delay));
  }
}

int main() {
  static StaticQueue_t xStaticCECQueue;
  static uint8_t storageCECQueue[CEC_QUEUE_LENGTH * sizeof(uint8_t)];

  static StackType_t stackBlink[BLINK_STACK_SIZE];
  static StackType_t stackCEC[CEC_STACK_SIZE];

  static StaticTask_t xBlinkTCB;
  static StaticTask_t xCECTCB;

  static TaskHandle_t xBlinkTask;

  stdio_init_all();

  alarm_pool_init_default();

  gpio_init(PICO_DEFAULT_LED_PIN);
  gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

  // HID key queue
  QueueHandle_t cec_q =
      xQueueCreateStatic(CEC_QUEUE_LENGTH, sizeof(uint8_t), &storageCECQueue[0], &xStaticCECQueue);

  xBlinkTask = xTaskCreateStatic(blink_task, "Blink Task", BLINK_STACK_SIZE, NULL, 1,
                                 &stackBlink[0], &xBlinkTCB);
  xCECTask = xTaskCreateStatic(cec_task, CEC_TASK_NAME, CEC_STACK_SIZE, &cec_q,
                               configMAX_PRIORITIES - 1, &stackCEC[0], &xCECTCB);

  (void)xBlinkTask;
  (void)xCECTask;

  vTaskStartScheduler();

  return 0;
}
