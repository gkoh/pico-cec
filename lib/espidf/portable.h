#ifndef _PORTABLE_H_
#define _PORTABLE_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define STACK_WORDSIZE 4

typedef unsigned int uint;

#include "esp_log.h"
extern const char *TAG;

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
//void ws2812_init(int pin);
//void ws2812_put_rgb(int r, int g, int b);
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

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR hdmi-cec.c
//#include <string.h>
#define HID_KEY_NONE 0
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR hdmi-ddc.c
#define PICO_ERROR_NONE 0
#define PICO_ERROR_TIMEOUT 0
#define PICO_ERROR_GENERIC 0
#define i2c_default 0
#define PICO_DEFAULT_I2C_SDA_PIN 0
#define PICO_DEFAULT_I2C_SCL_PIN 0
//void i2c_init(void* a, uint b);
//void i2c_deinit(void* a);
//int i2c_read_timeout_us(void* a, int b, void* c, int d, int e, int f);
//int i2c_write_timeout_us(void* a, int b, void* c, int d, int e, int f);
struct i2c_inst {
    void *hw;
    bool restart_on_next;
};
typedef struct i2c_inst i2c_inst_t;
uint i2c_init (i2c_inst_t *i2c, uint baudrate);
void i2c_deinit (i2c_inst_t *i2c);
int i2c_read_timeout_us(i2c_inst_t * i2c, uint8_t addr, uint8_t * dst, size_t len, bool nostop, uint timeout_us);
int i2c_write_timeout_us(i2c_inst_t * i2c, uint8_t addr, const uint8_t * src, size_t len, bool nostop, uint timeout_us);
////////////////////////////////////////////////////////////////////////////////

#endif // _PORTABLE_H_
