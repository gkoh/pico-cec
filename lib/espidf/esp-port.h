#ifndef _ESP_PORT_H_
#define _ESP_PORT_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

////////////////////////////////////////////////////////////////////////////////
// Internal workings of the port which is not made visible to the application
//  this will be more like the stuff we utilise from/for the esp-sdk mostly
//  intended as support for pico-sdk compatibility functions found in portable.c
//
#include <esp_timer.h>
#include "sdkconfig.h"

#define UART_PORT_NUM (2)
void uart_init(void);

// For timer support:
// CONFIG_ESP_TIMER_SUPPORTS_ISR_DISPATCH_METHOD=y  // sdkconfig (not needed now that we are not
// using the gptimer?)

// TODO: shall we respect sdkconfig mapping of pins, or hardcode assignments?
//       or utilise the current cmake build method, aka 'set(PICO_BOARD ..'
//       however this seems to be woven into the pico-sdk architecture
//       and not a part of the local CMakeFile.txt system
//
// README.md
// 55:* PICO_BOARD: specify variant of Pico board, defaults to Seeed XIAO RP2040
// 63:$ cmake -DPICO_BOARD=pico -DCEC_PIN=11 ..
//
#define GPIO_OUTPUT_IO_0 CONFIG_GPIO_OUTPUT_0
#define GPIO_OUTPUT_IO_1 CONFIG_GPIO_OUTPUT_1
#define GPIO_OUTPUT_PIN_SEL ((1ULL << GPIO_OUTPUT_IO_0) | (1ULL << GPIO_OUTPUT_IO_1))

#define GPIO_INPUT_IO_0 CONFIG_GPIO_INPUT_0
#define GPIO_INPUT_IO_1 CONFIG_GPIO_INPUT_1
#define GPIO_INPUT_PIN_SEL ((1ULL << GPIO_INPUT_IO_0) | (1ULL << GPIO_INPUT_IO_1))

// // For timer support:
// // CONFIG_ESP_TIMER_SUPPORTS_ISR_DISPATCH_METHOD=y  // sdkconfig

// #define IO_IRQ_BANK0 0
// //#define GPIO_IRQ_EDGE_RISE 1
// //#define GPIO_IRQ_EDGE_FALL 2

// /*! \brief  GPIO Interrupt level definitions (GPIO events)
//  *  \ingroup hardware_gpio
//  *  \brief GPIO Interrupt levels
//  *
//  * An interrupt can be generated for every GPIO pin in 4 scenarios:
//  *
//  * * Level High: the GPIO pin is a logical 1
//  * * Level Low: the GPIO pin is a logical 0
//  * * Edge High: the GPIO has transitioned from a logical 0 to a logical 1
//  * * Edge Low: the GPIO has transitioned from a logical 1 to a logical 0
//  *
//  * The level interrupts are not latched. This means that if the pin is a logical 1 and the level
//  high interrupt is active, it will
//  * become inactive as soon as the pin changes to a logical 0. The edge interrupts are stored in
//  the INTR register and can be
//  * cleared by writing to the INTR register.
//  */
// enum gpio_irq_level {
//     GPIO_IRQ_LEVEL_LOW = 0x1u,  ///< IRQ when the GPIO pin is a logical 0
//     GPIO_IRQ_LEVEL_HIGH = 0x2u, ///< IRQ when the GPIO pin is a logical 1
//     GPIO_IRQ_EDGE_FALL = 0x4u,  ///< IRQ when the GPIO has transitioned from a logical 1 to a
//     logical 0 GPIO_IRQ_EDGE_RISE = 0x8u,  ///< IRQ when the GPIO has transitioned from a logical
//     0 to a logical 1
// };

// //uint64_t time_us_64(void);  // normally declared in freertos
// //#define time_us_64 esp_timer_get_time

// static inline uint64_t time_us_64(void) {
//   return esp_timer_get_time();
// }

/*
time_us_64: Return the current 64 bit timestamp value in microseconds for the default timer
instance. uint64_t time_us_64(void)

 Returns the full 64 bits of the hardware timer. The pico_time and other functions rely on the
 fact that this value monotonically increases from power up. As such it is expected that this
 value counts upwards and never wraps (we apologize for introducing a potential year 5851444 bug).
Returns the 64 bit timestamp
*/
/*
uint64_t time_us_64(): Return the current 64 bit timestamp value in microseconds for the default
timer instance.
*/

#endif  // _ESP_PORT_H_
