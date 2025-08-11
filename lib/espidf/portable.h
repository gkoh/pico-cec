#ifndef __PORTABLE_H__
#define __PORTABLE_H__

#include <stdbool.h>
#include <stddef.h>

////////////////////////////////////////////////////////////////////////////////
// The published interface of the port made visible to the application
//   everything in here should represent a replacement for pico-sdk stuff
//

typedef unsigned int uint;

#include <esp_log.h>
// https://gcc.gnu.org/onlinedocs/gcc/Diagnostic-Pragmas.html
// clang-format off
#define DECLARE_TAG() \
  _Pragma("GCC diagnostic push") \
  _Pragma("GCC diagnostic ignored \"-Wunused-variable\"") \
  static const char *TAG = __FILE_NAME__; \
  _Pragma("GCC diagnostic pop")
// clang-format on

#if __INTELLISENSE__
#define __FILE_NAME__ __FILE__
#endif

#define IO_IRQ_BANK0 0

#define UART_PORT_NUM (CONFIG_UART_PORT_NUM)  // TODO: won't work, CONFIG_ not defined here
void uart_init(void);

////////////////////////////////////////////////////////////////////////////////
// pico-sdk gpio primatives/functions
//

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

void gpio_set_dir(unsigned int gpio, bool out);

void gpio_init(uint gpio);
void gpio_set_function(uint gpio, enum gpio_function fn);
void gpio_pull_up(uint gpio);
void gpio_disable_pulls(uint gpio);
void gpio_set_dir(uint gpio, bool out);
void gpio_put(uint gpio, int value);

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR main.c & debug.c
#define vTaskStartScheduler vTaskStartScheduler_stub
void vTaskStartScheduler_stub(void);
void alarm_pool_init_default();
void stdio_init_all();
void board_init();
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR blink.c
#define PICO_DEFAULT_WS2812_PIN 48
// void ws2812_init(int pin);
// void ws2812_put_rgb(int r, int g, int b);
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR debug.c
#define PICO_DEFAULT_LED_PIN 2
////////////////////////////////////////////////////////////////////////////////

#define SCL_IO_PIN CONFIG_I2C_MASTER_SCL
#define SDA_IO_PIN CONFIG_I2C_MASTER_SDA
#define MASTER_FREQUENCY CONFIG_I2C_MASTER_FREQUENCY
#define I2C_PORT 0  // == I2C_NUM_0
////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR ddc.c
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

typedef struct i2c_inst i2c_inst_t;
uint i2c_init(i2c_inst_t *i2c, uint baudrate);  // pico-sdk prototype can map to esp-idf v1 driver
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
// REQUIRED FOR usb-cdc.c
void reset_usb_boot(uint32_t usb_activity_gpio_pin_mask, uint32_t disable_interface_mask);
void watchdog_reboot(uint32_t pc, uint32_t sp, uint32_t delay_ms);
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR usb_hid.c
void board_led_write(int state);

#define KEYMAP_DEFAULT_KODI 1
// #define KEYMAP_DEFAULT_MISTER 1

// RHPort number used for device can be defined by board.mk, default to port 0
#ifndef BOARD_TUD_RHPORT
#define BOARD_TUD_RHPORT 0
#endif

#ifndef USE_USB_CDC  // TODO: won't work as config.h not included here

#ifndef KEYBOARD_LED_CAPSLOCK
#define KEYBOARD_LED_CAPSLOCK 0
#endif

typedef enum {
  HID_REPORT_TYPE_RESERVED = 0,
  HID_REPORT_TYPE_INPUT,
  HID_REPORT_TYPE_OUTPUT,
  HID_REPORT_TYPE_FEATURE
} hid_report_type_t;

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

#endif

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR nvs.c
#define XIP_BASE 0x100
#define FLASH_PAGE_SIZE 16
#define FLASH_SECTOR_SIZE 512
void restore_interrupts(uint32_t a);                        // declared in <hardware/sync.h>
uint32_t save_and_disable_interrupts();                     // declared in <hardware/sync.h>
void flash_range_erase(uint32_t flash_offs, size_t count);  // declared in <hardware/flash.h>
void flash_range_program(uint32_t flash_offs,
                         const uint8_t *data,
                         size_t count);  // declared in <hardware/flash.h>

void flash_range_read(uint32_t flash_offs, const uint8_t *data, size_t count);  // esp32 port only
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR freertos_hook.c
#ifndef TU_ASSERT
#define TU_ASSERT(a, b)  // tu_assert(a)
#endif
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR cec-config.c
#ifndef NULL
#define NULL 0
#endif
#define HID_KEY_NONE 0x00
#define HID_KEY_C 0x06
#define HID_KEY_F 0x09
#define HID_KEY_I 0x0C
#define HID_KEY_L 0x0F
#define HID_KEY_P 0x13
#define HID_KEY_R 0x15
#define HID_KEY_X 0x1B
#define HID_KEY_1 0x1E
#define HID_KEY_2 0x1F
#define HID_KEY_3 0x20
#define HID_KEY_4 0x21
#define HID_KEY_5 0x22
#define HID_KEY_6 0x23
#define HID_KEY_7 0x24
#define HID_KEY_8 0x25
#define HID_KEY_9 0x26
#define HID_KEY_0 0x27
#define HID_KEY_ENTER 0x28
#define HID_KEY_BACKSPACE 0x2A
#define HID_KEY_SPACE 0x2C
#define HID_KEY_F12 0x45
#define HID_KEY_ARROW_RIGHT 0x4F
#define HID_KEY_ARROW_LEFT 0x50
#define HID_KEY_ARROW_DOWN 0x51
#define HID_KEY_ARROW_UP 0x52
// #define KEYBOARD_LED_CAPSLOCK 0
// #define HID_REPORT_TYPE_OUTPUT 0
////////////////////////////////////////////////////////////////////////////////

#endif  // __PORTABLE_H__
