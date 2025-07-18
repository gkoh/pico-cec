#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <driver/gpio.h>
#include <driver/i2c.h>         // legacy driver
#include <driver/i2c_master.h>  // new driver model
#include <driver/uart.h>
#include <esp_flash.h>
#include <esp_system.h>

#include "esp-port.h"
#include "portable.h"
DECLARE_TAG()

#include "sdkconfig.h"
#include "prefs.h"

void gpio_init(uint gpio) {}
void gpio_disable_pulls(uint gpio) {
  //  gpio_pullup_dis(gpio);
  gpio_pullup_en(gpio);
  gpio_pulldown_dis(gpio);
}
void IRAM_ATTR gpio_set_dir(uint gpio, bool out) {
  //	gpio_reset_pin(gpio);
  gpio_set_direction(gpio, out == GPIO_OUT ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT);
}
bool gpio_get(uint gpio) {
  return gpio_get_level(gpio);
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

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR main.c
void vTaskStartScheduler_stub(void) {
  // Vanilla FreeRTOS never returns, but with esp32 we wouldn't even call this so don't block
  // ESP_LOGI(TAG, "vTaskStartScheduler_stub() invoked");
}
void stdio_init_all() {}
void board_init() {
  esp_log_level_set("*", ESP_LOG_DEBUG);
  prefs_init();
  uart_init();
}
void alarm_pool_init_default() {
  timer_init();
}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR cec-frame.c
void irq_set_enabled(int a, int b) {}
void gpio_acknowledge_irq(uint gpio, uint32_t events) {}
void IRAM_ATTR gpio_set_irq_enabled(uint gpio, uint32_t event_mask, bool enabled) {
  if (enabled) {
    if (event_mask == (GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL)) {
      gpio_set_intr_type(gpio, GPIO_INTR_ANYEDGE);
    }
    if (event_mask == (GPIO_IRQ_EDGE_RISE)) {
      gpio_set_intr_type(gpio, GPIO_INTR_POSEDGE);
    }
    if (event_mask == (GPIO_IRQ_EDGE_FALL)) {
      gpio_set_intr_type(gpio, GPIO_INTR_NEGEDGE);
    }
  } else {
    gpio_set_intr_type(gpio, GPIO_INTR_DISABLE);
  }
}
void esp_cec_rx_init(uint gpio, cec_frame_rx_isr_callback_t callback) {  // esp32 port only
  gpio_isr_init(gpio, callback);
}
void IRAM_ATTR timer_start(uint64_t time, timer_callback_t callback, void *user_data);
alarm_id_t IRAM_ATTR add_alarm_at(absolute_time_t time,
                                  alarm_callback_t callback,
                                  void *user_data,
                                  bool fire_if_past) {
  timer_start(time, callback, user_data);
  return 0;
}
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
#ifdef USE_ESPIDF_I2C_DRIVER_V2
// https://docs.espressif.com/projects/esp-idf/en/stable/esp32/migration-guides/release-5.x/5.2/peripherals.html
i2c_master_bus_handle_t bus_handle;
i2c_master_dev_handle_t dev_handle;

uint esp_i2c_init(uint i2c_frequency, uint16_t chip_addr) {
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
      .device_address = chip_addr,
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

#else

uint i2c_init(i2c_inst_t *i2c, uint i2c_frequency) {
  (void)i2c;
  esp_err_t ret;
  i2c_config_t i2c_config = {
      .mode = I2C_MODE_MASTER,
      .scl_io_num = SCL_IO_PIN,
      .sda_io_num = SDA_IO_PIN,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_pullup_en = GPIO_PULLUP_ENABLE,
      .master.clk_speed = i2c_frequency,
      // .clk_flags = I2C_SCLK_SRC_FLAG_FOR_NOMAL
  };
  //  esp_err_t i2c_param_config(i2c_port_t i2c_num, const i2c_config_t *i2c_conf);
  ret = i2c_param_config(I2C_PORT, &i2c_config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "i2c_param_config(..) failed %d", ret);
  }
  // esp_err_t i2c_driver_install(i2c_port_t i2c_num, i2c_mode_t mode, size_t slv_rx_buf_len, size_t
  // slv_tx_buf_len, int intr_alloc_flags);
  ret = i2c_driver_install(I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
  //  *     - ESP_OK   Success
  //  *     - ESP_ERR_INVALID_ARG Parameter error
  //  *     - ESP_FAIL Driver installation error
  // if (ret != ESP_OK) {
  //     ESP_LOGE(TAG, "i2c_driver_install(..) failed %d", ret);
  // }
  switch (ret) {
    case ESP_OK:
      break;
    case ESP_ERR_INVALID_ARG:
      ESP_LOGE(TAG, "i2c_init(..) Parameter error");
      break;
    case ESP_FAIL:
      ESP_LOGE(TAG, "i2c_init(..) Driver installation error");
      break;
    default:
      ESP_LOGE(TAG, "i2c_driver_install(..) failed %d", ret);
      break;
  }
  return i2c_frequency;  // unused
}
void i2c_deinit(i2c_inst_t *i2c) {
  (void)i2c;
  // esp_err_t i2c_driver_delete(i2c_port_t i2c_num);
  esp_err_t ret = i2c_driver_delete(I2C_PORT);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "i2c_driver_delete(..) failed %d", ret);
  }
}
int i2c_read_timeout_us(i2c_inst_t *i2c,
                        uint8_t addr,
                        uint8_t *dst,
                        size_t len,
                        bool nostop,
                        uint timeout_us) {
  (void)i2c;
  //  esp_err_t ret = i2c_master_read_from_device(I2C_PORT, addr, dst, len, timeout_us);
  esp_err_t ret = i2c_master_read_from_device(I2C_PORT, addr, dst, len, timeout_us / 1000);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "i2c_master_read_from_device(..) failed %d", ret);
  }
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
      ret = PICO_ERROR_GENERIC;
      ESP_LOGE(TAG, "i2c_master_read_from_device(..) failed %d", ret);
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
  //  esp_err_t ret = i2c_master_write_to_device(I2C_PORT, addr, src, len, timeout_us / 1000);
  esp_err_t ret = i2c_master_write_to_device(I2C_PORT, addr, src, len, timeout_us);
  //  *     - ESP_OK Success
  //  *     - ESP_ERR_INVALID_ARG Parameter error
  //  *     - ESP_FAIL Sending command error, slave hasn't ACK the transfer.
  //  *     - ESP_ERR_INVALID_STATE I2C driver not installed or not in master mode.
  //  *     - ESP_ERR_TIMEOUT Operation timeout because the bus is busy.
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "i2c_master_write_to_device(..) failed %d", ret);
  }
  switch (ret) {
    case ESP_OK:
      ret = len;
      break;
    case ESP_ERR_INVALID_ARG:
      ESP_LOGE(TAG, "i2c_master_write_to_device(..) Parameter error");
      ret = PICO_ERROR_GENERIC;
      break;
    case ESP_FAIL:
      ESP_LOGE(
          TAG,
          "i2c_master_write_to_device(..) Sending command error, slave hasn't ACK the transfer");
      ret = PICO_ERROR_GENERIC;
      break;
    case ESP_ERR_INVALID_STATE:
      ESP_LOGE(TAG,
               "i2c_master_write_to_device(..) I2C driver not installed or not in master mode");
      ret = PICO_ERROR_GENERIC;
      break;
    case ESP_ERR_TIMEOUT:
      ESP_LOGE(TAG, "i2c_master_write_to_device(..) Operation timeout because the bus is busy");
      ret = PICO_ERROR_TIMEOUT;
      break;
    default:
      ret = PICO_ERROR_GENERIC;
      ESP_LOGE(TAG, "i2c_master_write_to_device(..) failed %d", ret);
      break;
  }
  return ret;  // Returns: Number of bytes written, or PICO_ERROR_GENERIC if address not
               // acknowledged, no device present, or PICO_ERROR_TIMEOUT if a timeout occurred.
}
#endif  // USE_ESPIDF_I2C_DRIVER_V2
////////////////////////////////////////////////////////////////////////////////

// static pico_cec_nvs_t pico_cec_nvs = {0x0};
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

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR blink.c
// #define PICO_DEFAULT_WS2812_PIN 0
void ws2812_init(unsigned int pin) {}
void ws2812_put_rgb(uint8_t red, uint8_t green, uint8_t blue) {}
void ws2812_put_pixel(uint32_t pixel_grb) {}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR usb_hid.c
bool tud_init(uint8_t rhport) {
  return 0;
}
void tud_task(void) {
  static uint32_t _delay = 100;
  vTaskDelay(pdMS_TO_TICKS(_delay));
}
bool tud_hid_keyboard_report(uint8_t report_id, uint8_t modifier, const uint8_t keycode[6]) {
  return 0;
}
bool tud_suspended(void) {
  return 0;
}
bool tud_hid_ready(void) {
  return 0;
}
bool tud_remote_wakeup(void) {
  return false;
}
uint32_t tud_cdc_write_str(const char *str) {
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
uint8_t tud_cdc_read_char(void) {
  uint8_t data;
  uart_read_bytes(UART_PORT_NUM, &data, 1, 20 / portTICK_PERIOD_MS);  // (20/portTICK_PERIOD_MS) = 2
  return data;
}
uint32_t tud_cdc_write_flush(void) {
  // uart_flush(UART_PORT_NUM);  // TODO: seems to be a problem when calling this
  return 0;
}
////////////////////////////////////////////////////////////////////////////////
