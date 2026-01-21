#ifndef CONFIG_H
#define CONFIG_H

// Vanilla FreeRTOS specifies stack sizes in number of words whilst
// XTENSA FreeRTOS uses number of bytes
#ifndef STACK_WORDSIZE
#define STACK_WORDSIZE 1
// Needs better name, currently inverted meaning - STACK_BYTES? STACK_WORDBYTES??
// Whatever, it needs to equal one for pico, and four with the esp32 port
// Currently defined in ./main/CMakeLists.txt for the esp32 port
#endif

#define HID_QUEUE_LENGTH (16)

#define LED_STACK_SIZE (128 * STACK_WORDSIZE)
#define HID_STACK_SIZE (256 * STACK_WORDSIZE)
#define USB_STACK_SIZE (512 * STACK_WORDSIZE)
#define CDC_STACK_SIZE (1024 * STACK_WORDSIZE)
#define KEY_STACK_SIZE (1024 * STACK_WORDSIZE)

#define LED_TASK_NAME "led"
#define HID_TASK_NAME "hid"
#define USB_TASK_NAME "usb"
#define CDC_TASK_NAME "cdc"
#define KEY_TASK_NAME "key"

#define LED_PRIORITY (configMAX_PRIORITIES - configMAX_PRIORITIES + 1)
#define HID_PRIORITY (configMAX_PRIORITIES - 3)
#define USB_PRIORITY (configMAX_PRIORITIES - 4)
#define CDC_PRIORITY (configMAX_PRIORITIES - 6)
#define KEY_PRIORITY (configMAX_PRIORITIES - 7)

#endif  // CONFIG_H
