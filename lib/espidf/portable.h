#ifndef __PORTABLE_H__
#define __PORTABLE_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "sdkconfig.h"

////////////////////////////////////////////////////////////////////////////////
// The published interface of the port made visible to the application
//   everything in here should represent a replacement for pico-sdk stuff
//

typedef unsigned int uint;

#include "esp_log.h"

// https://gcc.gnu.org/onlinedocs/gcc/Diagnostic-Pragmas.html
// clang-format off
#define DECLARE_TAG() \
  _Pragma("GCC diagnostic push") \
  _Pragma("GCC diagnostic ignored \"-Wunused-variable\"") \
  static const char *TAG = __FILE_NAME__; \
  _Pragma("GCC diagnostic pop")
// clang-format on

#define IO_IRQ_BANK0 0
//#define GPIO_IRQ_EDGE_RISE 1
//#define GPIO_IRQ_EDGE_FALL 2

/*! \brief  GPIO Interrupt level definitions (GPIO events)
 *  \ingroup hardware_gpio
 *  \brief GPIO Interrupt levels
 *
 * An interrupt can be generated for every GPIO pin in 4 scenarios:
 *
 * * Level High: the GPIO pin is a logical 1
 * * Level Low: the GPIO pin is a logical 0
 * * Edge High: the GPIO has transitioned from a logical 0 to a logical 1
 * * Edge Low: the GPIO has transitioned from a logical 1 to a logical 0
 *
 * The level interrupts are not latched. This means that if the pin is a logical 1 and the level
 * high interrupt is active, it will become inactive as soon as the pin changes to a logical 0. The
 * edge interrupts are stored in the INTR register and can be cleared by writing to the INTR
 * register.
 */
enum gpio_irq_level {
  GPIO_IRQ_LEVEL_LOW = 0x1u,   ///< IRQ when the GPIO pin is a logical 0
  GPIO_IRQ_LEVEL_HIGH = 0x2u,  ///< IRQ when the GPIO pin is a logical 1
  GPIO_IRQ_EDGE_FALL =
      0x4u,  ///< IRQ when the GPIO has transitioned from a logical 1 to a logical 0
  GPIO_IRQ_EDGE_RISE =
      0x8u,  ///< IRQ when the GPIO has transitioned from a logical 0 to a logical 1
};

// uint64_t time_us_64(void);  // normally declared in freertos
//#define time_us_64 esp_timer_get_time

int64_t esp_timer_get_time(void);  // from esp_timer.h

static inline uint64_t time_us_64(void) {
  return esp_timer_get_time();
}

typedef void (*gpio_irq_callback_t)(uint64_t edge_time);

void irq_set_enabled(int a, int b);

////////////////////////////////////////////////////////////////////////////////
// pico-sdk gpio primatives/functions
//
void gpio_set_irq_enabled(uint gpio, uint32_t event_mask, bool enabled);
void gpio_acknowledge_irq(uint gpio, uint32_t events);

void esp_cec_rx_init(uint gpio, gpio_irq_callback_t edge_time);

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

////////////////////////////////////////////////////////////////////////////////
// Implement some of the pico-sdk timer primatives/functions
//
typedef uint64_t absolute_time_t;

// /*
//  update_us_since_boot(): update an absolute_time_t value to represent a given number of
//  microseconds since boot
//     static void update_us_since_boot(absolute_time_t *t, uint64_t us_since_boot)
//       t              the absolute time value to update
//       us_since_boot  the number of microseconds since boot to represent. Note this should be
//       representable as a signed 64 bit integer
// */
// static inline void update_us_since_boot(absolute_time_t *t, uint64_t us_since_boot) {
// //    *t = esp_timer_get_time() + us_since_boot;
//     *t = time_us_64() + us_since_boot;
// }

// /*
//  from_us_since_boot(): convert a number of microseconds since boot to an absolute_time_t
//     static absolute_time_t from_us_since_boot(uint64_t us_since_boot)
//         us_since_boot	number of microseconds since boot
//     Returns an absolute time equivalent to us_since_boot
// */
// // Added from_us_since_boot() function to convert a uint64_t timestamp to an absolute_time_t.
// static inline absolute_time_t from_us_since_boot(uint64_t us_since_boot) {
//     absolute_time_t t;
// //    update_us_since_boot(&t, us_since_boot);
// //    t = time_us_64() + us_since_boot;
//     t = us_since_boot;
//     return t; // returns an absolute time equivalent to us_since_boot
// }

#define from_us_since_boot(t) ((absolute_time_t)(t))

////////////////////////////////////////////////////////////////////////////////
// Some pico-sdk alarm stuff
//
void alarm_pool_init_default();

typedef int32_t alarm_id_t;  // note this is signed because we use <0 as a meaningful error value
typedef int64_t (*alarm_callback_t)(alarm_id_t id, void *user_data);
alarm_id_t add_alarm_at(absolute_time_t time,
                        alarm_callback_t callback,
                        void *user_data,
                        bool fire_if_past);
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR main.c & debug.c
#define vTaskStartScheduler vTaskStartScheduler_stub
void vTaskStartScheduler_stub(void);
void stdio_init_all();
void board_init();
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR blink.c
#define PICO_DEFAULT_WS2812_PIN 0
// void ws2812_init(int pin);
// void ws2812_put_rgb(int r, int g, int b);
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR debug.c
#define PICO_DEFAULT_LED_PIN 2
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR hdmi-cec.c
#define HID_KEY_NONE 0
////////////////////////////////////////////////////////////////////////////////

#define SCL_IO_PIN CONFIG_I2C_MASTER_SCL
#define SDA_IO_PIN CONFIG_I2C_MASTER_SDA
#define MASTER_FREQUENCY CONFIG_I2C_MASTER_FREQUENCY
#define I2C_PORT 0  // == I2C_NUM_0
////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR hdmi-ddc.c
#define PICO_ERROR_NONE 0
#define PICO_ERROR_GENERIC -1
#define PICO_ERROR_TIMEOUT -2

#define i2c_default I2C_PORT
#define PICO_DEFAULT_I2C_SDA_PIN SDA_IO_PIN
#define PICO_DEFAULT_I2C_SCL_PIN SCL_IO_PIN

struct i2c_inst {
  void *hw;
  bool restart_on_next;
};

//#define USE_NEW_I2C_DRIVER

typedef struct i2c_inst i2c_inst_t;
#ifdef USE_NEW_I2C_DRIVER
uint i2c_init(i2c_inst_t *i2c,
              uint i2c_frequency,
              uint16_t chip_addr);  // modified for compatibility with esp-idf
#else
uint i2c_init(i2c_inst_t *i2c, uint baudrate);  // pico-sdk prototype
#endif
void i2c_deinit(i2c_inst_t *i2c);
int i2c_read_timeout_us(i2c_inst_t *i2c,
                        uint8_t addr,
                        uint8_t *dst,
                        size_t len,
                        bool nostop,
                        uint timeout_us);
int i2c_write_timeout_us(i2c_inst_t *i2c,
                         uint8_t addr,
                         const uint8_t *src,
                         size_t len,
                         bool nostop,
                         uint timeout_us);
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR usb_cdc.c
void reset_usb_boot(uint32_t usb_activity_gpio_pin_mask, uint32_t disable_interface_mask);
void watchdog_reboot(uint32_t pc, uint32_t sp, uint32_t delay_ms);
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR usb_hid.c
void board_led_write(int state);

#define KEYMAP_DEFAULT_KODI 1
//#define KEYMAP_DEFAULT_MISTER 1

typedef enum {
  HID_REPORT_TYPE_RESERVED = 0,
  HID_REPORT_TYPE_INPUT,
  HID_REPORT_TYPE_OUTPUT,
  HID_REPORT_TYPE_FEATURE
} hid_report_type_t;

#define KEYBOARD_LED_CAPSLOCK 0

// RHPort number used for device can be defined by board.mk, default to port 0
#ifndef BOARD_TUD_RHPORT
#define BOARD_TUD_RHPORT 0
#endif

bool tud_init(uint8_t rhport);
void tud_task(void);

bool tud_hid_keyboard_report(uint8_t report_id, uint8_t modifier, const uint8_t keycode[6]);

bool tud_suspended(void);
bool tud_hid_ready(void);

bool tud_remote_wakeup(void);
uint32_t tud_cdc_write_str(const char *str);

bool tud_cdc_connected(void);
uint32_t tud_cdc_available(void);
uint8_t tud_cdc_read_char(void);
uint32_t tud_cdc_write_flush(void);
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR nvs.c
#define XIP_BASE 0x100
#define FLASH_PAGE_SIZE 16
#define FLASH_SECTOR_SIZE 512

void flash_range_erase(int address, int size);
void flash_range_program(int address, uint8_t *a, int size);

void restore_interrupts(uint32_t a);
uint32_t save_and_disable_interrupts();
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR freertos_hook.c
#define TU_ASSERT(a, b)  // tu_assert(a)
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR cec-config.c
#ifndef NULL
#define NULL 0
#endif
#define HID_KEY_ENTER 0
#define HID_KEY_ARROW_UP 0
#define HID_KEY_ARROW_DOWN 0
#define HID_KEY_ARROW_LEFT 0
#define HID_KEY_ARROW_RIGHT 0
#define HID_KEY_C 0
#define HID_KEY_BACKSPACE 0
#define HID_KEY_0 0
#define HID_KEY_1 0
#define HID_KEY_2 0
#define HID_KEY_3 0
#define HID_KEY_4 0
#define HID_KEY_5 0
#define HID_KEY_6 0
#define HID_KEY_7 0
#define HID_KEY_8 0
#define HID_KEY_9 0
#define HID_KEY_I 0
#define HID_KEY_P 0
#define HID_KEY_X 0
#define HID_KEY_SPACE 0
#define HID_KEY_R 0
#define HID_KEY_F 0
#define HID_KEY_L 0
#define HID_KEY_F12 0

//#define KEYBOARD_LED_CAPSLOCK 0
//#define HID_REPORT_TYPE_OUTPUT 0
////////////////////////////////////////////////////////////////////////////////

// #define ESP_LOGD(tag, fmt, ...) do {} while (0)

// #ifdef DEBUG_MODE
//     #define DEBUG_PRINTF(fmt, ...) printf(fmt, ##__VA_ARGS__)
// #else
//     #define DEBUG_PRINTF(fmt, ...) do {} while (0)
// #endif

#endif  // __PORTABLE_H__
