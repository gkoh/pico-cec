#include <stdio.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "bsp/board.h"
#include "hardware/timer.h"
#include "pico/stdlib.h"

#include "hdmi-cec.h"

#define USBD_STACK_SIZE (4096)
#define HID_STACK_SIZE (1024)
#define BLINK_STACK_SIZE (128)
#define CEC_STACK_SIZE (2048)

void usb_device_task(void *);
void hid_task(void *);

void vBlinkTask(void *) {
  static bool state = true;

  while (true) {
    gpio_put(PICO_DEFAULT_LED_PIN, state);
    state = !state;
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

int main() {
  stdio_init_all();
  board_init();

  alarm_pool_init_default();

  gpio_init(PICO_DEFAULT_LED_PIN);
  gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

  gpio_init(CECPIN);
  gpio_disable_pulls(CECPIN);
  gpio_set_dir(CECPIN, GPIO_IN);

  // HID key queue
  QueueHandle_t q = xQueueCreate(16, sizeof(uint8_t));

  TaskHandle_t xBlinkTask;
  TaskHandle_t xUSBDTask;
  TaskHandle_t xHIDTask;

  xTaskCreate(vBlinkTask, "Blink Task", BLINK_STACK_SIZE, NULL, 1, &xBlinkTask);
  xTaskCreate(cec_task, CEC_TASK_NAME, CEC_STACK_SIZE, &q, configMAX_PRIORITIES - 1, &xCECTask);
  xTaskCreate(hid_task, "hid", HID_STACK_SIZE, &q, configMAX_PRIORITIES - 2, &xHIDTask);
  xTaskCreate(usb_device_task, "usbd", USBD_STACK_SIZE, NULL, configMAX_PRIORITIES - 3, &xUSBDTask);

  vTaskCoreAffinitySet(xCECTask, (1 << 0));
  vTaskCoreAffinitySet(xBlinkTask, (1 << 0));
  vTaskCoreAffinitySet(xHIDTask, (1 << 1));
  vTaskCoreAffinitySet(xUSBDTask, (1 << 1));

  vTaskStartScheduler();

  return 0;
}
