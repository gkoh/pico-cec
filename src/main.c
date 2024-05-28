#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

#include "bsp/board.h"
#include "hardware/timer.h"
#include "pico/stdlib.h"

#include "hdmi-cec.h"

#define USBD_STACK_SIZE (4096)
#define HID_STACK_SIZE (1024)
#define BLINK_STACK_SIZE (128)
#define CEC_STACK_SIZE (512)

void usb_device_task(void *);
void hid_task(void *);

void vBlinkTask(void *) {
  static bool state = true;

  gpio_init(PICO_DEFAULT_LED_PIN);
  gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

  while (true) {
    gpio_put(PICO_DEFAULT_LED_PIN, state);
    state = !state;
    vTaskDelay(pdMS_TO_TICKS(250));
  }
}

int main() {
  stdio_init_all();
  board_init();

  alarm_pool_init_default();

  gpio_init(CECPIN);
  gpio_disable_pulls(CECPIN);
  gpio_set_dir(CECPIN, GPIO_IN);

  printf("%s\n", "HDMI CEC Decoder and Audio System Emulator");
  printf("%s\n", "Based on the original work by Thomas Sowell");
  printf("%s\n", "Arduino adaoptation (C) 2021 Szymon Słupik");
  printf("%s\n", "------------------------------------------");
  if (ACTIVE) {
    printf("%s\n", "************** ACTIVE MODE ***************");
  } else {
    printf("%s\n", "************** PASSIVE MODE **************");
  }

  // HID key queue
  QueueHandle_t q = xQueueCreate(16, sizeof(uint8_t));
  //TaskHandle_t xUSBDTask;
  //TaskHandle_t xHIDTask;

  xTaskCreate(cec_task, CEC_TASK_NAME, CEC_STACK_SIZE, &q, configMAX_PRIORITIES - 1, &xCECTask);
  //xTaskCreate(usb_device_task, "usbd", USBD_STACK_SIZE, NULL, configMAX_PRIORITIES - 3, &xUSBDTask);
  //xTaskCreate(hid_task, "hid", HID_STACK_SIZE, &q, configMAX_PRIORITIES - 3, &xHIDTask);
  //xTaskCreate(vBlinkTask, "Blink Task", BLINK_STACK_SIZE, NULL, 1, NULL);

#if 0
  // usbd, hid runs on core 1
  vTaskCoreAffinitySet(xUSBDTask, 0x01);
  vTaskCoreAffinitySet(xHIDTask, 0x01);

  // CEC runs on core 0
  vTaskCoreAffinitySet(xCECTask, 0x00);
#endif

  vTaskStartScheduler();

  return 0;
}
