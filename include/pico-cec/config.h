#ifndef CONFIG_H
#define CONFIG_H

#ifndef PICO_CEC_VERSION
#define PICO_CEC_VERSION "unknown"
#endif  // PICO_CEC_VERSION

#define LED_STACK_SIZE (128)
#define CEC_STACK_SIZE (1024)
#define HID_STACK_SIZE (256)
#define USB_STACK_SIZE (512)
#define LOG_STACK_SIZE (1024)
#define CDC_STACK_SIZE (1024)

#define CEC_QUEUE_LENGTH (16)

#define LED_TASK_NAME "Blink"
#define CEC_TASK_NAME "cec"
#define HID_TASK_NAME "hid"
#define USB_TASK_NAME "usb"
#define LOG_TASK_NAME "log"
#define CDC_TASK_NAME "cdc"

#define LED_PRIORITY (1)
#define CEC_PRIORITY (configMAX_PRIORITIES - 1)
#define HID_PRIORITY (configMAX_PRIORITIES - 2)
#define USB_PRIORITY (configMAX_PRIORITIES - 3)
#define LOG_PRIORITY (configMAX_PRIORITIES - 4)
#define CDC_PRIORITY (configMAX_PRIORITIES - 5)

#endif  // CONFIG_H
