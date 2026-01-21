#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <driver/gpio.h>
#include <driver/i2c_master.h>  // new driver model
#include <driver/uart.h>
#include <esp_flash.h>
#include <esp_system.h>

#include "esp-idf.h"
DECLARE_TAG()

#include "prefs.h"

#ifdef USE_RGB_LED
#include "led_strip.h"
#endif

/* Use project configuration menu (idf.py menuconfig) to choose the GPIO to blink,
   or you can edit the following line and set a number here.
*/
// #define BLINK_GPIO CONFIG_BLINK_GPIO
#define BLINK_GPIO 48
#define CONFIG_BLINK_LED_STRIP_BACKEND_RMT 1

#define EDID_I2C_ADDR (0x50)

void gpio_init(uint gpio) {}
void gpio_disable_pulls(uint gpio) {
  //  gpio_pullup_dis(gpio);
  gpio_pullup_en(gpio);
  gpio_pulldown_dis(gpio);
}
void gpio_put(uint gpio, int value) {
  gpio_set_level(gpio, value);
}
void gpio_pull_up(uint gpio) {
  //  gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
  //  gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);
  gpio_pullup_en(gpio);
  gpio_pulldown_dis(gpio);
}
void gpio_set_function(uint gpio, enum gpio_function fn) {
  //  gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
  //  gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
}

void usb_init(void);

#if (CONFIG_FREERTOS_HZ != 1000)
#pragma error
#endif

#if (CONFIG_FREERTOS_TASK_NOTIFICATION_ARRAY_ENTRIES < 2)
#pragma error
#endif

#if (CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS != 1)
#pragma error
#endif

#define UART_TXD (CONFIG_UART_TXD)
#define UART_RXD (CONFIG_UART_RXD)
#define UART_RTS (UART_PIN_NO_CHANGE)
#define UART_CTS (UART_PIN_NO_CHANGE)

#define UART_BAUD_RATE (CONFIG_UART_BAUD_RATE)
#define UART_BUF_SIZE (1024)

void uart_init(void) {
  uart_config_t uart_config = {
      .baud_rate = UART_BAUD_RATE,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_SCLK_DEFAULT,
  };
  int intr_alloc_flags = 0;
#if CONFIG_UART_ISR_IN_IRAM
  intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif
  ESP_ERROR_CHECK(
      uart_driver_install(UART_PORT_NUM, UART_BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
  ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
  ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, UART_TXD, UART_RXD, UART_RTS, UART_CTS));
}

////////////////////////////////////////////////////////////////////////////////
//
// bool gpio_get(unsigned int gpio) {
//   return gpio_get_level(gpio);
// }
void IRAM_ATTR gpio_set_dir(unsigned int gpio, bool out) {
  //	gpio_reset_pin(gpio);
  gpio_set_direction(gpio, out == GPIO_OUT ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT);
}

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR main.c
void vTaskStartScheduler_stub(void) {
  // Vanilla FreeRTOS never returns, but with esp32 we wouldn't even call this so don't block
  // ESP_LOGI(TAG, "vTaskStartScheduler_stub() invoked");
}
void stdio_init_all() {}
void board_init() {
  // log levels: None, Error, Warning, Info, Debug, Verbose
  esp_log_level_set("*", ESP_LOG_DEBUG);
  prefs_init();
// #ifdef USE_USB_CDC
#if (CONFIG_TINYUSB_CDC_ENABLED == 1)
  usb_init();
#else
  uart_init();
#endif
}
void alarm_pool_init_default() {
  // timer_init();
}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR cec-frame.c
// void irq_set_enabled(int a, int b) {}  // pico only
// void gpio_acknowledge_irq(uint gpio, uint32_t events) {}  // pico only
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR cec-task.c
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR usb-cdc.c
void reset_usb_boot(uint32_t usb_activity_gpio_pin_mask, uint32_t disable_interface_mask) {}
void watchdog_reboot(uint32_t pc, uint32_t sp, uint32_t delay_ms) {
  esp_restart();
}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR ddc.c
static i2c_master_bus_handle_t bus_handle;
static i2c_master_dev_handle_t dev_handle;

uint i2c_init(i2c_inst_t *i2c, uint i2c_frequency) {
  (void)i2c;
  esp_err_t ret;
  i2c_master_bus_config_t i2c_bus_config = {
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .i2c_port = I2C_NUM_0,
      .scl_io_num = SCL_IO_PIN,
      .sda_io_num = SDA_IO_PIN,
      .glitch_ignore_cnt = 7,
  };
  //  ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &bus_handle));
  ret = i2c_new_master_bus(&i2c_bus_config, &bus_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "i2c_master_bus_add_device(..) failed %u", ret);
  }
  i2c_device_config_t i2c_dev_conf = {
      .scl_speed_hz = i2c_frequency,
      .device_address = EDID_I2C_ADDR,
  };
  ret = i2c_master_bus_add_device(bus_handle, &i2c_dev_conf, &dev_handle);
  //  *      - ESP_OK: Create I2C master device successfully.
  //  *      - ESP_ERR_INVALID_ARG: I2C bus initialization failed because of invalid argument.
  //  *      - ESP_ERR_NO_MEM: Create I2C bus failed because of out of memory.
  // if (ret != ESP_OK) {
  //   ESP_LOGE(TAG, "i2c_master_bus_add_device(..) failed %u", ret);
  // }
  switch (ret) {
    case ESP_OK:
      break;
    case ESP_ERR_INVALID_ARG:
      ESP_LOGE(TAG, "I2C bus initialization failed because of invalid argument");
      break;
    case ESP_ERR_NO_MEM:
      ESP_LOGE(TAG, "Create I2C bus failed because of out of memory");
      break;
    default:
      ESP_LOGE(TAG, "i2c_master_bus_add_device(..) failed %u", ret);
      break;
  }
  //    if (i2c_new_master_bus(&i2c_bus_config, &tool_bus_handle) != ESP_OK) {
  //        return 1;
  //    }
  return i2c_frequency;  // unused
}
void i2c_deinit(i2c_inst_t *i2c) {
  (void)i2c;
  esp_err_t ret;
  ret = i2c_master_bus_rm_device(dev_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "i2c_deinit(..) failed %u", ret);
  } else
    dev_handle = 0;
  ret = i2c_del_master_bus(bus_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "i2c_del_master_bus(..) failed %u", ret);
  } else
    bus_handle = 0;
}
//  int ret = i2c_read_timeout_us(i2c_default, EDID_I2C_ADDR, edid, len, false,
//  EDID_I2C_TIMEOUT_US);
int i2c_read_timeout_us(i2c_inst_t *i2c,
                        uint8_t addr,
                        uint8_t *dst,
                        size_t len,
                        bool nostop,
                        uint timeout_us) {
  (void)i2c;

  assert(EDID_I2C_ADDR == addr);

  // esp_err_t i2c_master_receive(i2c_master_dev_handle_t i2c_dev, uint8_t *read_buffer, size_t
  // read_size, int xfer_timeout_ms);
  esp_err_t ret = i2c_master_receive(dev_handle, dst, len, timeout_us / 1000);
  // *      - ESP_OK: I2C master receive success
  // *      - ESP_ERR_INVALID_ARG: I2C master receive parameter invalid.
  // *      - ESP_ERR_TIMEOUT: Operation timeout(larger than xfer_timeout_ms) because the bus is
  // busy or hardware crash. if (ret != ESP_OK) {
  //   ESP_LOGE(TAG, "i2c_master_receive(..) failed %u", ret);
  // }
  switch (ret) {
    case ESP_OK:
      ret = len;
      break;
    case ESP_ERR_INVALID_ARG:
      ret = PICO_ERROR_GENERIC;
      break;
    case ESP_ERR_TIMEOUT:
      ret = PICO_ERROR_TIMEOUT;
      break;
    default:
      ESP_LOGE(TAG, "i2c_master_receive(..) failed %u", ret);
      break;
  }
  return ret;  // Returns: Number of bytes read, or PICO_ERROR_GENERIC if address not acknowledged,
               // no device present, or PICO_ERROR_TIMEOUT if a timeout occurred.
}

int i2c_write_timeout_us(i2c_inst_t *i2c,
                         uint8_t addr,
                         const uint8_t *src,
                         size_t len,
                         bool nostop,
                         uint timeout_us) {
  (void)i2c;

  assert(EDID_I2C_ADDR == addr);

  // esp_err_t i2c_master_transmit(i2c_master_dev_handle_t i2c_dev, const uint8_t *write_buffer,
  // size_t write_size, int xfer_timeout_ms);
  esp_err_t ret = i2c_master_transmit(dev_handle, src, len, timeout_us / 1000);
  // *      - ESP_OK: I2C master transmit success
  // *      - ESP_ERR_INVALID_ARG: I2C master transmit parameter invalid.
  // *      - ESP_ERR_TIMEOUT: Operation timeout(larger than xfer_timeout_ms) because the bus is
  // busy or hardware crash. if (ret != ESP_OK) {
  //     ESP_LOGE(TAG, "i2c_master_transmit(..) failed %u", ret);
  // }
  switch (ret) {
    case ESP_OK:
      ret = len;
      break;
    case ESP_ERR_INVALID_ARG:
      ret = PICO_ERROR_GENERIC;
      break;
    case ESP_ERR_TIMEOUT:
      ret = PICO_ERROR_TIMEOUT;
      break;
    default:
      ESP_LOGE(TAG, "i2c_master_transmit(..) failed %u", ret);
      break;
  }
  //  ESP_LOGE(TAG, "i2c_master_transmit(..) returning %u", ret);
  return ret;  // Returns: Number of bytes written, or PICO_ERROR_GENERIC if address not
               // acknowledged, no device present, or PICO_ERROR_TIMEOUT if a timeout occurred.
}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR nvs.c
uint32_t CEC_NVS_BASE_ADDR[] = {0};
// uint32_t __CEC_NVS_LEN[] = {0};
// uint32_t CEC_NVS_BASE_ADDR[] = &pico_cec_nvs;
// uint32_t __CEC_NVS_LEN = sizeof(pico_cec_nvs_t);
uint32_t __CEC_NVS_LEN = 2048;
void restore_interrupts(uint32_t a) {}
uint32_t save_and_disable_interrupts() {
  return 0;
}
void flash_range_erase(uint32_t flash_offs, size_t count) {
  prefs_clear();
}
void flash_range_program(uint32_t flash_offs, const uint8_t *data, size_t count) {
  if (prefs_begin("Settings", false, NULL)) {
    prefs_putBytes("Settings", data, count);
    prefs_end();
  }
}
void flash_range_read(uint32_t flash_offs, const uint8_t *data, size_t count) {
  if (prefs_begin("Settings", true, NULL)) {
    prefs_getBytes("Settings", (void *)data, count);
    prefs_end();
  }
}
////////////////////////////////////////////////////////////////////////////////
void save_logical_address(uint8_t addr) {
  if (prefs_begin("cec-laddr", false, NULL)) {
    prefs_putBytes("cec-laddr", (void *)&addr, sizeof(addr));
    prefs_end();
  }
}

uint8_t load_logical_address(void) {
  uint8_t addr;
  if (prefs_begin("cec-laddr", true, NULL)) {
    prefs_getBytes("cec-laddr", (void *)&addr, sizeof(addr));
    prefs_end();
  }
  return addr;
}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR blink.c
// #define PICO_DEFAULT_WS2812_PIN 0
#ifdef USE_RGB_LED
static led_strip_handle_t led_strip;

void ws2812_init(unsigned int pin) {
  // ESP_LOGI(TAG, "Example configured to blink addressable LED!");
  /* LED strip initialization with the GPIO and pixels number*/
  led_strip_config_t strip_config = {
      .strip_gpio_num = pin,
      .max_leds = 1,  // at least one LED on board
  };
#if CONFIG_BLINK_LED_STRIP_BACKEND_RMT
  led_strip_rmt_config_t rmt_config = {
      .resolution_hz = 10 * 1000 * 1000,  // 10MHz
      .flags.with_dma = false,
  };
  ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
#elif CONFIG_BLINK_LED_STRIP_BACKEND_SPI
  led_strip_spi_config_t spi_config = {
      .spi_bus = SPI2_HOST,
      .flags.with_dma = true,
  };
  ESP_ERROR_CHECK(led_strip_new_spi_device(&strip_config, &spi_config, &led_strip));
#else
#error "unsupported LED strip backend"
#endif
  /* Set all LED off to clear all pixels */
  led_strip_clear(led_strip);
}
void ws2812_put_rgb(uint8_t red, uint8_t green, uint8_t blue) {
  led_strip_set_pixel(led_strip, 0, red, green, blue);
  /* Refresh the strip to send data */
  led_strip_refresh(led_strip);
}
// void ws2812_put_pixel(uint32_t pixel_grb) {}
#else
void ws2812_init(unsigned int pin) {}
void ws2812_put_rgb(uint8_t red, uint8_t green, uint8_t blue) {}
#endif  // USE_RGB_LED

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR usb_hid.c

void board_led_write(int state) {}

// #ifndef USE_USB_CDC

bool __attribute__((weak)) tud_init(uint8_t rhport) {
  return 0;
}
void __attribute__((weak)) tud_task(void) {
  static uint32_t _delay = 100;
  vTaskDelay(pdMS_TO_TICKS(_delay));
}
bool __attribute__((weak)) tud_suspended(void) {
  return 0;
}
bool __attribute__((weak)) tud_remote_wakeup(void) {
  return false;
}

uint32_t __attribute__((weak)) tud_cdc_write_str(const char *str) {
  uart_write_bytes(UART_PORT_NUM, str, strlen(str));
  return 0;
}
bool __attribute__((weak)) tud_cdc_connected(void) {
  return 1;
}
uint32_t __attribute__((weak)) tud_cdc_available(void) {
  size_t size = 0;
  uart_get_buffered_data_len(UART_PORT_NUM, &size);
  return size;
}
uint8_t __attribute__((weak)) tud_cdc_read_char(void) {
  uint8_t data = 0;
  uart_read_bytes(UART_PORT_NUM, &data, 1, 20 / portTICK_PERIOD_MS);  // (20/portTICK_PERIOD_MS) = 2
  return data;
}
uint32_t __attribute__((weak)) tud_cdc_write_flush(void) {
  // uart_flush(UART_PORT_NUM);  // TODO: seems to be a problem when calling this
  return 0;
}

// #endif  // USE_USB_CDC

// #ifndef USE_USB_HID
bool __attribute__((weak)) tud_hid_keyboard_report(uint8_t report_id,
                                                   uint8_t modifier,
                                                   const uint8_t keycode[6]) {
  return 0;
}
bool __attribute__((weak)) tud_hid_ready(void) {
  return 0;
}
// #endif  // USE_USB_HID
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR usb_descriptors.c
// #ifndef USE_USB_CDC
// #if (CONFIG_TINYUSB_CDC_ENABLED != 1)
uint8_t __attribute__((weak)) const *tud_descriptor_configuration_cb(uint8_t index) {
  (void)index;  // for multiple configurations
  // return desc_configuration;
  return 0;
}
uint16_t __attribute__((weak)) const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void)langid;

  return 0;
}
uint8_t __attribute__((weak)) const *tud_descriptor_device_cb(void) {
  //  return (uint8_t const *)&desc_device;
  return 0;
}
// #endif  // USE_USB_CDC

// #ifndef USE_USB_HID
uint8_t __attribute__((weak)) const *tud_hid_descriptor_report_cb(uint8_t instance) {
  (void)instance;
  // return desc_hid_report;
  return 0;
}
// #endif  // USE_USB_HID
////////////////////////////////////////////////////////////////////////////////
