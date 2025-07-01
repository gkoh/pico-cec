#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "bsp/board.h"
#include "hardware/timer.h"
#include "pico/stdlib.h"

#include "config.h"

#include "blink.h"
#include "cec-frame.h"
#include "cec-log.h"
#include "cec-task.h"
#include "usb-cdc.h"
#include "usb_hid.h"
#include "ws2812.h"

int main() {
  static StaticQueue_t xCECQueue;
  static uint8_t storageCECQueue[CEC_QUEUE_LENGTH * sizeof(uint8_t)];

  static StackType_t stackLED[LED_STACK_SIZE];
  static StackType_t stackCEC[CEC_STACK_SIZE];
  static StackType_t stackHID[HID_STACK_SIZE];
  static StackType_t stackCDC[CDC_STACK_SIZE];
  static StackType_t stackUSB[USB_STACK_SIZE];

  static StaticTask_t xLEDTCB;
  static StaticTask_t xCECTCB;
  static StaticTask_t xHIDTCB;
  static StaticTask_t xUSBTCB;
  static StaticTask_t xCDCTCB;

  static TaskHandle_t xUSBTask;
  static TaskHandle_t xHIDTask;
  static TaskHandle_t xCDCTask;

  blink_init();

  stdio_init_all();
  board_init();

  alarm_pool_init_default();

  // HID key queue
  QueueHandle_t cec_q;
  cec_q = xQueueCreateStatic(CEC_QUEUE_LENGTH, sizeof(uint8_t), &storageCECQueue[0], &xCECQueue);

  xBlinkTask = xTaskCreateStatic(blink_task, LED_TASK_NAME, LED_STACK_SIZE, NULL, LED_PRIORITY,
                                 &stackLED[0], &xLEDTCB);
  xCECTask = xTaskCreateStatic(cec_task, CEC_TASK_NAME, CEC_STACK_SIZE, &cec_q, CEC_PRIORITY,
                               &stackCEC[0], &xCECTCB);
  xHIDTask = xTaskCreateStatic(hid_task, HID_TASK_NAME, HID_STACK_SIZE, &cec_q, HID_PRIORITY,
                               &stackHID[0], &xHIDTCB);
  xUSBTask = xTaskCreateStatic(usb_task, USB_TASK_NAME, USB_STACK_SIZE, NULL, USB_PRIORITY,
                               &stackUSB[0], &xUSBTCB);
  xCDCTask = xTaskCreateStatic(cdc_task, CDC_TASK_NAME, CDC_STACK_SIZE, NULL, CDC_PRIORITY,
                               &stackCDC[0], &xCDCTCB);

  (void)xBlinkTask;
  (void)xCECTask;
  (void)xHIDTask;
  (void)xUSBTask;
  (void)xCDCTask;

  cec_log_init();

  vTaskStartScheduler();

  return 0;
}
