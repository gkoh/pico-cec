#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_log.h"

#include "driver/gpio.h"
#include "driver/uart.h"

#include "pico/stdlib.h"
#include "portable.h"
#include "cec-gpio.h"

const char *TAG = "nil";  // global tag for reference from multiple modules

#define UART_TXD (17)
#define UART_RXD (16)
#define UART_RTS (UART_PIN_NO_CHANGE)
#define UART_CTS (UART_PIN_NO_CHANGE)

#define UART_PORT_NUM  (2)
#define UART_BAUD_RATE (115200)
#define UART_BUF_SIZE  (128)

static void uart_init() {
    uart_config_t uart_config = {
        .baud_rate  = UART_BAUD_RATE,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    int intr_alloc_flags = 0;
#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, UART_BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, UART_TXD, UART_RXD, UART_RTS, UART_CTS));
}

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR main.c
void vTaskStartScheduler_stub(void) {
    // Vanilla FreeRTOS never returns, but with esp32 we wouldn't even call this so don't block
    ESP_LOGI(TAG, "vTaskStartScheduler_stub() invoked");
}
void stdio_init_all() {}
void board_init() {
//	gpio_init_all();
    uart_init();
}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR hdmi-cec.c
// #define GPIO_IN 0
// #define GPIO_OUT 0
void reset_usb_boot(uint32_t usb_activity_gpio_pin_mask, uint32_t disable_interface_mask) {}
void watchdog_reboot(uint32_t pc, uint32_t sp, uint32_t delay_ms) {}

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR hdmi-ddc.c
// #define PICO_ERROR_NONE 0
// #define PICO_ERROR_TIMEOUT 0
// #define PICO_ERROR_GENERIC 0
// #define i2c_default 0
// #define PICO_DEFAULT_I2C_SDA_PIN 0
// #define PICO_DEFAULT_I2C_SCL_PIN 0
uint i2c_init (i2c_inst_t *i2c, uint baudrate) {
    return baudrate; // Returns: Actual set baudrate
}
void i2c_deinit (i2c_inst_t *i2c) {}
int i2c_read_timeout_us(i2c_inst_t * i2c, uint8_t addr, uint8_t * dst, size_t len, bool nostop, uint timeout_us) {
    return 0; // Returns: Number of bytes read, or PICO_ERROR_GENERIC if address not acknowledged, no device present, or PICO_ERROR_TIMEOUT if a timeout occurred.
}
int i2c_write_timeout_us(i2c_inst_t * i2c, uint8_t addr, const uint8_t * src, size_t len, bool nostop, uint timeout_us) {
    return 0; // Returns: Number of bytes written, or PICO_ERROR_GENERIC if address not acknowledged, no device present, or PICO_ERROR_TIMEOUT if a timeout occurred.
}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR nvs.c
uint32_t CEC_NVS_BASE_ADDR[] = { 0 };
uint32_t __CEC_NVS_LEN[] = { 0 };
void restore_interrupts(uint32_t a) {} // declared in <hardware/sync.h>
uint32_t save_and_disable_interrupts() { return 0; } // declared in <hardware/sync.h>
void flash_range_erase(int address, int size) {} // declared in <hardware/flash.h>
void flash_range_program(int address, uint8_t * a, int size) {} // declared in <hardware/flash.h>
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR blink.c
// #define PICO_DEFAULT_WS2812_PIN 0
void ws2812_init(unsigned int pin) {}
void ws2812_put_rgb(uint8_t red, uint8_t green, uint8_t blue) {}
void ws2812_put_pixel(uint32_t pixel_grb) {}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR usb_hid.c
bool tud_init(uint8_t rhport) { return 0; }
void tud_task(void) {
  static uint32_t _delay = 100;
  vTaskDelay(pdMS_TO_TICKS(_delay));
}
bool tud_hid_keyboard_report(uint8_t report_id, uint8_t modifier, const uint8_t keycode[6]) { return 0; }
bool tud_suspended(void) { return 0; }
bool tud_hid_ready(void) { return 0; }
void tud_remote_wakeup(void) {}
uint32_t tud_cdc_write_str(const char* str) {
    uart_write_bytes(UART_PORT_NUM, str, strlen(str));
    return 0;
}
bool tud_cdc_connected(void) {
    return 1;
}
uint32_t tud_cdc_available(void) {
    size_t size;
    uart_get_buffered_data_len(UART_PORT_NUM, &size);
    return size;
}
int8_t tud_cdc_read_char(void) {
    uint8_t data;
    uart_read_bytes(UART_PORT_NUM, &data, 1, 20 / portTICK_PERIOD_MS); // (20 / portTICK_PERIOD_MS) = 2
    return data;
}
uint32_t tud_cdc_write_flush(void) {
    vTaskDelay(pdMS_TO_TICKS(10));  // needs to be at least ten?
//    uart_flush(UART_PORT_NUM);  // TODO: seems to be a problem when calling this
    return 0;
}
////////////////////////////////////////////////////////////////////////////////

