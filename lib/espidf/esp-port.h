#ifndef _ESP_PORT_H_
#define _ESP_PORT_H_

#include <stdint.h>

////////////////////////////////////////////////////////////////////////////////
// Internal workings of the port which is not made directly visible to the application
// This will be more like the stuff we utilise from/for the esp-sdk mostly
//  intended as support for pico-sdk compatibility functions found in portable.c
//

#define UART_PORT_NUM (2)
void uart_init(void);

void timer_init(void);
typedef int64_t (*timer_callback_t)(int32_t id, void *user_data);
// IRAM_ATTR not defined at this point
//void IRAM_ATTR timer_start(uint64_t time, timer_callback_t callback, void *user_data);

typedef void (*gpio_irq_callback_t)(uint64_t edge_time);
void gpio_isr_init(unsigned int gpio, gpio_irq_callback_t callback);

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
