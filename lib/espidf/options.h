#ifndef __OPTIONS_H__
#define __OPTIONS_H__

////////////////////////////////////////////////////////////////////////////////
// Everything in this file is intended for development purposes only and should
// be removed throughout the codebase before final merge to master repository
//

// #define USE_GPIO_TASK_HANDLER // enable this to get the tx frame handling out of the interrupt
// #define USE_GPIO_TASK_QUEUE
#define USE_ESPIDF_I2C_DRIVER_V2

// Perhaps implement the equivalent of ESP_LOGx for the pico build:

// #define ESP_LOGD(tag, fmt, ...) do {} while (0)

// #ifdef DEBUG_MODE
//     #define DEBUG_PRINTF(fmt, ...) printf(fmt, ##__VA_ARGS__)
// #else
//     #define DEBUG_PRINTF(fmt, ...) do {} while (0)
// #endif

#endif  // __OPTIONS_H__
