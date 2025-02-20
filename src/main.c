#include <stdio.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "bsp/board.h"
#include "hardware/timer.h"
#include "pico/stdlib.h"

#include "cec-log.h"
#include "hdmi-cec.h"
#include "usb-cdc.h"
#include "usb_hid.h"

#define USBD_STACK_SIZE (512)
#define HID_STACK_SIZE (256)
#define CDC_STACK_SIZE (2048)
#define BLINK_STACK_SIZE (128)
#define CEC_STACK_SIZE (2048)
#define CEC_QUEUE_LENGTH (16)

void blink_task(void *param) {
  static uint32_t blink_delay = 1000;
  static bool state = true;

  while (true) {
    gpio_put(PICO_DEFAULT_LED_PIN, state);
    state = !state;
    vTaskDelay(pdMS_TO_TICKS(blink_delay));
  }
}

void cdc_task(void *param);

int main() {
  static StaticQueue_t xStaticCECQueue;
  static uint8_t storageCECQueue[CEC_QUEUE_LENGTH * sizeof(uint8_t)];

  static StackType_t stackBlink[BLINK_STACK_SIZE];
  static StackType_t stackCEC[CEC_STACK_SIZE];
  static StackType_t stackHID[HID_STACK_SIZE];
  static StackType_t stackCDC[CDC_STACK_SIZE];
  static StackType_t stackUSBD[USBD_STACK_SIZE];

  static StaticTask_t xBlinkTCB;
  static StaticTask_t xCECTCB;
  static StaticTask_t xHIDTCB;
  static StaticTask_t xUSBDTCB;
  static StaticTask_t xCDCTCB;

  static TaskHandle_t xBlinkTask;
  static TaskHandle_t xUSBDTask;
  static TaskHandle_t xHIDTask;
  static TaskHandle_t xCDCTask;

  stdio_init_all();
  board_init();

  alarm_pool_init_default();

  gpio_init(PICO_DEFAULT_LED_PIN);
  gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

  // HID key queue
  QueueHandle_t cec_q =
      xQueueCreateStatic(CEC_QUEUE_LENGTH, sizeof(uint8_t), &storageCECQueue[0], &xStaticCECQueue);

  xBlinkTask =
      xTaskCreateStatic(blink_task, "Blink", BLINK_STACK_SIZE, NULL, 1, &stackBlink[0], &xBlinkTCB);
  xCECTask = xTaskCreateStatic(cec_task, CEC_TASK_NAME, CEC_STACK_SIZE, &cec_q,
                               configMAX_PRIORITIES - 1, &stackCEC[0], &xCECTCB);
  xHIDTask = xTaskCreateStatic(hid_task, "hid", HID_STACK_SIZE, &cec_q, configMAX_PRIORITIES - 2,
                               &stackHID[0], &xHIDTCB);
  xUSBDTask = xTaskCreateStatic(usb_device_task, "usbd", USBD_STACK_SIZE, NULL,
                                configMAX_PRIORITIES - 3, &stackUSBD[0], &xUSBDTCB);
  xCDCTask = xTaskCreateStatic(cdc_task, "cdc", CDC_STACK_SIZE, NULL, configMAX_PRIORITIES - 5,
                               &stackCDC[0], &xCDCTCB);

  (void)xCECTask;
  (void)xBlinkTask;
  (void)xHIDTask;
  (void)xCDCTask;
  (void)xUSBDTask;

  cec_log_init();

  vTaskStartScheduler();

  return 0;
}
