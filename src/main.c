/* Intercept HDMI CEC commands, convert to a keypress and send to HID task
 * handler.
 *
 * Based (mostly ripped) from the Arduino version by Szymon Slupik:
 * https://github.com/SzymonSlupik/CEC-Tiny-Pro
 * which itself is based on the original code by Thomas Sowell:
 * https://github.com/tsowell/avr-hdmi-cec-volume/tree/master
 */

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "portable.h"
DECLARE_TAG()

#include "config.h"

#include "blink.h"
#include "cec-cmd.h"
#include "cec-frame.h"
#include "cec-log.h"
#include "cec-task.h"
#include "usb-cdc.h"
#include "usb-hid.h"
#include "ws2812.h"

#include "nvs.h"

#include "console.h"

#ifndef CEC_PIN
#define CEC_PIN 3  // GPIO3 == D10 (Seeed Studio XIAO RP2040)
#endif

static QueueHandle_t hid_q;  // HID key queue

static config_t config = {0x0};

// void cec_user_control(uint8_t key, bool pressed) {
//   if (pressed) {
//     blink_set(BLINK_STATE_GREEN_ON);
//     command_t command = config.keymap[key];
//     if (command.name != NULL) {
//       xQueueSend(cec_q, &command.key, pdMS_TO_TICKS(10));
//     }
//   } else {
//     blink_set(BLINK_STATE_OFF);
//     key = HID_KEY_NONE;
//     xQueueSend(cec_q, &key, pdMS_TO_TICKS(10));
//   }
// }

// void cec_status(bool active) {
//   if (active) {
//     blink_set_blink(BLINK_STATE_GREEN_2HZ);
//   } else {  // standby
//     blink_set_blink(BLINK_STATE_BLUE_2HZ);
//   }
// }

static int echo_addr = 0;
static int echo_rate = 0;
void set_echo(int addr, int rate) {
  echo_addr = addr;
  echo_rate = rate;
}

//
// The 'heart' of the application
// Recieves CEC events and translates them into HID keystrokes for transmission
// This largely decouples the CEC and USB functionality into two sides of the application
//
// Receive a user control event 'key' from the cec queue, translate it with the
//  configuration keymap, and send the result to the HID queue
void key_task(void *param) {
  int i = 0;
  while (1) {
    // Poll every 100ms
    uint8_t key = HID_KEY_NONE;
    if (cec_read(&key, pdMS_TO_TICKS(100))) {
      if (key != 0xFF) {
        blink_set(BLINK_STATE_GREEN_ON);
        command_t command = config.keymap[key];
        if (command.name != NULL) {
          xQueueSend(hid_q, &command.key, pdMS_TO_TICKS(10));
        }
      } else {
        blink_set(BLINK_STATE_OFF);
        key = HID_KEY_NONE;
        xQueueSend(hid_q, &key, pdMS_TO_TICKS(10));
      }
    } else {
      if (echo_rate != 0) {
        if (++i % echo_rate == 0) {
          // send echo message
          cec_cmd_send("echo", echo_addr);
        }
      }
    }
  }
}

int main() {
  static StaticQueue_t xHIDQueue;
  static uint8_t storageHIDQueue[HID_QUEUE_LENGTH * sizeof(uint8_t)];

  static StackType_t stackLED[LED_STACK_SIZE];
  static StackType_t stackHID[HID_STACK_SIZE];
  static StackType_t stackCDC[CDC_STACK_SIZE];
  static StackType_t stackUSB[USB_STACK_SIZE];
  static StackType_t stackKEY[KEY_STACK_SIZE];

  static StaticTask_t xLEDTCB;
  static StaticTask_t xHIDTCB;
  static StaticTask_t xUSBTCB;
  static StaticTask_t xCDCTCB;
  static StaticTask_t xKEYTCB;

  TaskHandle_t xUSBTask;
  TaskHandle_t xHIDTask;
  TaskHandle_t xCDCTask;
  TaskHandle_t xKEYTask;

  blink_init();

  stdio_init_all();
  board_init();
  alarm_pool_init_default();

  nvs_load_config(&config);
  config.cec.gpio_pin = CEC_PIN;

  hid_q = xQueueCreateStatic(HID_QUEUE_LENGTH, sizeof(uint8_t), &storageHIDQueue[0], &xHIDQueue);

  xLEDTask = xTaskCreateStatic(led_task, LED_TASK_NAME, LED_STACK_SIZE, NULL, LED_PRIORITY,
                               &stackLED[0], &xLEDTCB);

  //
  // TODO: encapsulate the usb initialisatin or not?
  //       and if so, pass all the freertos parameters or have them as system wide defines?
  //
  //  cec_usb_init(LOG_STACK_SIZE, LOG_PRIORITY, console_output);
  //
  xHIDTask = xTaskCreateStatic(hid_task, HID_TASK_NAME, HID_STACK_SIZE, &hid_q, HID_PRIORITY,
                               &stackHID[0], &xHIDTCB);
  xUSBTask = xTaskCreateStatic(usb_task, USB_TASK_NAME, USB_STACK_SIZE, NULL, USB_PRIORITY,
                               &stackUSB[0], &xUSBTCB);
  xCDCTask = xTaskCreateStatic(cdc_task, CDC_TASK_NAME, CDC_STACK_SIZE, NULL, CDC_PRIORITY,
                               &stackCDC[0], &xCDCTCB);
  xKEYTask = xTaskCreateStatic(key_task, KEY_TASK_NAME, KEY_STACK_SIZE, NULL, KEY_PRIORITY,
                               &stackKEY[0], &xKEYTCB);
  (void)xHIDTask;
  (void)xUSBTask;
  (void)xCDCTask;
  (void)xKEYTask;

  cec_init(config.cec, console_put);

  vTaskStartScheduler();  // no-op on esp32 port as rtos is already running, required for pico-sdk builds

  return 0;
}
