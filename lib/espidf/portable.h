#ifndef _PORTABLE_H_
#define _PORTABLE_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define STACK_WORDSIZE 4

typedef unsigned int uint;

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR main.c & debug.c
#define vTaskStartScheduler vTaskStartScheduler_stub
void vTaskStartScheduler_stub(void);

//void blink_init();
void stdio_init_all();
void board_init();
void alarm_pool_init_default();
//void cec_log_init();

//void blink_task(void *param);
//void cec_task(void *param);
//void hid_task(void *param);
//void usb_device_task(void *param);
//void cdc_task(void *param);
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR blink.c
#define PICO_DEFAULT_WS2812_PIN 0
//void ws2812_init(int pin);
//void ws2812_put_rgb(int r, int g, int b);
//void ws2812_put_rgb(int r, int g, int b) {}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR debug.c
#define PICO_DEFAULT_LED_PIN 2
//#define GPIO_OUT true
//void gpio_put(uint gpio, int value);
//void stdio_init_all();
//void alarm_pool_init_default();
//void gpio_init(uint gpio);
//void gpio_set_dir(int pin, int mode);
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR hdmi-cec.c
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

#include <string.h>
#define i2c_default 0
#define IO_IRQ_BANK0 0
#define HID_KEY_NONE 0
#define PICO_ERROR_TIMEOUT 0
#define PICO_DEFAULT_I2C_SDA_PIN 0
#define PICO_DEFAULT_I2C_SCL_PIN 0
#define GPIO_IN 0
#define GPIO_OUT 1
#define GPIO_IRQ_EDGE_RISE 0
#define GPIO_IRQ_EDGE_FALL 0

typedef void (*gpio_irq_callback_t)(uint gpio, uint32_t event_mask);

void irq_set_enabled(int a, int b);
void i2c_init(int a, int b);
void i2c_deinit(int a);

void gpio_init(uint gpio);
void gpio_pull_up(uint gpio);
void gpio_disable_pulls(uint gpio);
// Set a single GPIO to input/output.
// true = out
// 0 = in
void gpio_set_dir(uint gpio, bool out);
void gpio_set_function(uint gpio, enum gpio_function fn);
void gpio_set_irq_enabled(uint gpio, uint32_t event_mask, bool enabled);
void gpio_acknowledge_irq(uint gpio, uint32_t events);
void gpio_set_irq_callback(gpio_irq_callback_t callback);
//gpio_function_t gpio_get_function(uint gpio);  // UNUSED ?
bool gpio_get(uint gpio);
void gpio_put(uint gpio, int value);

uint64_t time_us_64(void);

//typedef int alarm_id_t;
typedef unsigned int uint;
typedef int64_t(*f_ptr)(int,void*);

typedef uint64_t absolute_time_t;
static inline void update_us_since_boot(absolute_time_t *t, uint64_t us_since_boot) { *t = us_since_boot; }
static inline absolute_time_t from_us_since_boot(uint64_t us_since_boot) {
    absolute_time_t t;
    update_us_since_boot(&t, us_since_boot);
    return t;
}

#if 0
struct alarm_pool {
    uint8_t timer_alarm_num;
    uint8_t core_num;
    // this is protected by the lock (threads allocate from it, and the IRQ handler adds back to it)
    int16_t free_head;
    // this is protected by the lock (threads add to it, the IRQ handler removes from it)
    volatile int16_t new_head;
    volatile bool has_pending_cancellations;

    // this is owned by the IRQ handler so doesn't need additional locking
    int16_t ordered_head;
    uint16_t num_entries;
    // alarm_pool_timer_t *timer;
    // spin_lock_t *lock;
    // alarm_pool_entry_t *entries;
};

typedef struct alarm_pool alarm_pool_t;
// // static alarm_pool_t default_alarm_pool = {
// //         .entries = default_alarm_pool_entries,
// // };
// // static inline bool default_alarm_pool_initialized(void) {
// //     return default_alarm_pool.lock != NULL;
// // }
// typedef void alarm_pool_timer_t;
alarm_pool_t *alarm_pool_get_default(void) {
// //    assert(default_alarm_pool_initialized());
// //    return &default_alarm_pool;
 	return NULL;
}

typedef int32_t alarm_id_t; // note this is signed because we use <0 as a meaningful error value
typedef int64_t (*alarm_callback_t)(alarm_id_t id, void *user_data);

alarm_id_t alarm_pool_add_alarm_at(alarm_pool_t *pool, absolute_time_t time, alarm_callback_t callback, void *user_data, bool fire_if_past) {
	alarm_id_t id = 0;
	return id;
}

typedef int64_t (*alarm_callback_t)(alarm_id_t id, void *user_data);
static inline alarm_id_t add_alarm_at(absolute_time_t time, alarm_callback_t callback, void *user_data, bool fire_if_past) {
    return alarm_pool_add_alarm_at(alarm_pool_get_default(), time, callback, user_data, fire_if_past);
}
#endif // 0

typedef int32_t alarm_id_t; // note this is signed because we use <0 as a meaningful error value
typedef int64_t (*alarm_callback_t)(alarm_id_t id, void *user_data);
alarm_id_t add_alarm_at(absolute_time_t time, alarm_callback_t callback, void *user_data, bool fire_if_past);
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR hdmi-ddc.c
#define PICO_ERROR_NONE 0
#define PICO_ERROR_TIMEOUT 0
#define PICO_ERROR_GENERIC 0
#define i2c_default 0
#define PICO_DEFAULT_I2C_SDA_PIN 0
#define PICO_DEFAULT_I2C_SCL_PIN 0
//void i2c_init(int a, int b);
//void i2c_deinit(int a);
int i2c_read_timeout_us(int a, int b, void* c, int d, int e, int f);
int i2c_write_timeout_us(int a, int b, void* c, int d, int e, int f);
//void gpio_set_function(int pin, int func);
//void gpio_pull_up(int pin);
////////////////////////////////////////////////////////////////////////////////

#endif // _PORTABLE_H_
