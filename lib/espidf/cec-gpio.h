#ifndef _GPIO_H_
#define _GPIO_H_

#include "sdkconfig.h"
#include "portable.h"

// TODO: shall we respect sdkconfig mapping of pins, or hardcode assignments?
//       or utilise the current cmake build method, aka 'set(PICO_BOARD ..'
//       however this seems to be woven into the pico-sdk architecture
//       and not a part of the local CMakeFile.txt system
//
// README.md
// 55:* PICO_BOARD: specify variant of Pico board, defaults to Seeed XIAO RP2040
// 63:$ cmake -DPICO_BOARD=pico -DCEC_PIN=11 ..
//
#define GPIO_OUTPUT_IO_0    CONFIG_GPIO_OUTPUT_0
#define GPIO_OUTPUT_IO_1    CONFIG_GPIO_OUTPUT_1
#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<GPIO_OUTPUT_IO_0) | (1ULL<<GPIO_OUTPUT_IO_1))

#define GPIO_INPUT_IO_0     CONFIG_GPIO_INPUT_0
#define GPIO_INPUT_IO_1     CONFIG_GPIO_INPUT_1
#define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_INPUT_IO_0) | (1ULL<<GPIO_INPUT_IO_1))

// typedef enum gpio_function_rp2350 {
//     GPIO_FUNC_HSTX = 0, ///< Select HSTX as GPIO pin function
//     GPIO_FUNC_SPI = 1, ///< Select SPI as GPIO pin function
//     GPIO_FUNC_UART = 2, ///< Select UART as GPIO pin function
//     GPIO_FUNC_I2C = 3, ///< Select I2C as GPIO pin function
//     GPIO_FUNC_PWM = 4, ///< Select PWM as GPIO pin function
//     GPIO_FUNC_SIO = 5, ///< Select SIO as GPIO pin function
//     GPIO_FUNC_PIO0 = 6, ///< Select PIO0 as GPIO pin function
//     GPIO_FUNC_PIO1 = 7, ///< Select PIO1 as GPIO pin function
//     GPIO_FUNC_PIO2 = 8, ///< Select PIO2 as GPIO pin function
//     GPIO_FUNC_GPCK = 9, ///< Select GPCK as GPIO pin function
//     GPIO_FUNC_XIP_CS1 = 9, ///< Select XIP CS1 as GPIO pin function
//     GPIO_FUNC_CORESIGHT_TRACE = 9, ///< Select CORESIGHT TRACE as GPIO pin function
//     GPIO_FUNC_USB = 10, ///< Select USB as GPIO pin function
//     GPIO_FUNC_UART_AUX = 11, ///< Select UART_AUX as GPIO pin function
//     GPIO_FUNC_NULL = 0x1f, ///< Select NULL as GPIO pin function
// } gpio_function_t;

enum gpio_function {
    GPIO_FUNC_XIP = 0,
    GPIO_FUNC_SPI = 1,
    GPIO_FUNC_UART = 2,
    GPIO_FUNC_I2C = 3,
    GPIO_FUNC_PWM = 4,
    GPIO_FUNC_SIO = 5,
    GPIO_FUNC_PIO0 = 6,
    GPIO_FUNC_PIO1 = 7,
    GPIO_FUNC_GPCK = 8,
    GPIO_FUNC_USB = 9,
    GPIO_FUNC_NULL = 0xf,
};

#define GPIO_IN 0
#define GPIO_OUT 1

void gpio_init_all(void); // currently only in development branch
void gpio_init(uint gpio);
void gpio_set_function(uint gpio, enum gpio_function fn);
void gpio_pull_up(uint gpio);
void gpio_disable_pulls(uint gpio);
// Set a single GPIO to input/output.
// true = out
// 0 = in
void gpio_set_dir(uint gpio, bool out);
bool gpio_get(uint gpio);
void gpio_put(uint gpio, int value);

#endif // _GPIO_H_

