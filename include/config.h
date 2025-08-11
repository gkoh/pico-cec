#ifndef CONFIG_H
#define CONFIG_H

#if defined(__XTENSA__) || defined(__riscv)

#include "sdkconfig.h"

// For the specific target SoC, sdkconfig.h defines the current target, i.e.
#if CONFIG_IDF_TARGET_ESP32
#define USE_ALTERNATE_UART
#elif CONFIG_IDF_TARGET_ESP32S2
#elif CONFIG_IDF_TARGET_ESP32C3
#define USE_USB_CDC
#elif CONFIG_IDF_TARGET_ESP32S3
#define USE_USB_CDC
#define USE_USB_HID
#elif CONFIG_IDF_TARGET_ESP32H4
#elif CONFIG_IDF_TARGET_ESP32C2
#elif CONFIG_IDF_TARGET_ESP32C6
#elif CONFIG_IDF_TARGET_ESP32H2
#endif

#endif  // __XTENSA__

#ifndef PICO_CEC_VERSION
#if defined(__XTENSA__) || defined(__riscv)
#define PICO_CEC_VERSION "esp32"
#else
#define PICO_CEC_VERSION "unknown"
#endif  // __XTENSA__
#endif  // PICO_CEC_VERSION

// Vanilla FreeRTOS specifies stack sizes in number of words whilst
// XTENSA FreeRTOS uses number of bytes
#ifndef STACK_WORDSIZE
#define STACK_WORDSIZE \
  1  // Needs better name, currently inverted meaning - STACK_BYTES? STACK_WORDBYTES??
     // Whatever, it needs to equal one for pico, and four with the esp32 port
     // Current defined in ./main/CMakeLists.txt for the esp32 port
#endif

#define USB_STACK_SIZE (512 * STACK_WORDSIZE)
#define HID_STACK_SIZE (256 * STACK_WORDSIZE)
#define CDC_STACK_SIZE (1024 * STACK_WORDSIZE)
#define LED_STACK_SIZE (128 * STACK_WORDSIZE)
#define LOG_STACK_SIZE (1024 * STACK_WORDSIZE)
#define CEC_STACK_SIZE (1024 * STACK_WORDSIZE)
#define KEY_STACK_SIZE (1024 * STACK_WORDSIZE)

#define CEC_QUEUE_LENGTH (16)
#define HID_QUEUE_LENGTH (16)

#define LED_TASK_NAME "led"
#define CEC_TASK_NAME "cec"
#define HID_TASK_NAME "hid"
#define USB_TASK_NAME "usb"
#define LOG_TASK_NAME "log"
#define CDC_TASK_NAME "cdc"
#define KEY_TASK_NAME "key"

#define LED_PRIORITY (configMAX_PRIORITIES - configMAX_PRIORITIES + 1)
#define CEC_PRIORITY (configMAX_PRIORITIES - 2)
#define HID_PRIORITY (configMAX_PRIORITIES - 3)
#define USB_PRIORITY (configMAX_PRIORITIES - 4)
#define LOG_PRIORITY (configMAX_PRIORITIES - 5)
#define CDC_PRIORITY (configMAX_PRIORITIES - 6)
#define KEY_PRIORITY (configMAX_PRIORITIES - 7)

#endif  // CONFIG_H
