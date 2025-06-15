#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_log.h"
static const char *TAG = "port";

#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/gptimer.h"

#include "pico/stdlib.h"
#include "portable.h"

// For timer support:
// CONFIG_ESP_TIMER_SUPPORTS_ISR_DISPATCH_METHOD=y  // sdkconfig

#define UART_TXD (17)
#define UART_RXD (16)
#define UART_RTS (UART_PIN_NO_CHANGE)
#define UART_CTS (UART_PIN_NO_CHANGE)

#define UART_PORT_NUM  (2)
#define UART_BAUD_RATE (115200)
#define UART_BUF_SIZE  (128)

static void uart_setup() {
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
// TaskHandle_t xCECTask;
// TaskHandle_t xBlinkTask;
// TaskHandle_t xHIDTask;
// TaskHandle_t xCDCTask;
// TaskHandle_t xUSBDTask;
//void blink_task(void *param) {}
//void cec_task(void *param) {}
//void hid_task(void *param) {}
//void usb_device_task(void *param) {}
//void cdc_task(void *param) {}
//void blink_init() {}

void vTaskStartScheduler_stub(void) {
    ESP_LOGI(TAG, "vTaskStartScheduler_stub() invoked");
}

void gpio_setup(void); // currently only in development branch

void stdio_init_all() {}
void board_init() {
//	gpio_setup();
    uart_setup();
}
//void alarm_pool_init_default() {}
//void cec_log_init() {}
////////////////////////////////////////////////////////////////////////////////

#define GPIO_OUTPUT_IO_0    CONFIG_GPIO_OUTPUT_0
#define GPIO_OUTPUT_IO_1    CONFIG_GPIO_OUTPUT_1
#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<GPIO_OUTPUT_IO_0) | (1ULL<<GPIO_OUTPUT_IO_1))

#define GPIO_INPUT_IO_0     CONFIG_GPIO_INPUT_0
#define GPIO_INPUT_IO_1     CONFIG_GPIO_INPUT_1
#define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_INPUT_IO_0) | (1ULL<<GPIO_INPUT_IO_1))

#define ESP_INTR_FLAG_DEFAULT 0

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR debug.c
// #define PICO_DEFAULT_LED_PIN 2
// #define GPIO_OUT 0
//void stdio_init_all() {}
//void alarm_pool_init_default() {}
//void gpio_put(int pin, int state) {}
//void gpio_init(int pin) {}
//void gpio_set_dir(int pin, int dir) {}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR hdmi-cec.c
// #include <string.h>
// #define i2c_default 0
// #define IO_IRQ_BANK0 0
// #define HID_KEY_NONE 0
// #define PICO_ERROR_TIMEOUT 0
// #define PICO_DEFAULT_I2C_SDA_PIN 0
// #define PICO_DEFAULT_I2C_SCL_PIN 0
// #define GPIO_IN 0
// #define GPIO_OUT 0
// #define GPIO_FUNC_I2C 0
// #define GPIO_IRQ_EDGE_RISE 0
// #define GPIO_IRQ_EDGE_FALL 0
void irq_set_enabled(int a, int b) {}
void i2c_init(int a, int b) {}
void i2c_deinit(int a) {}

//   gpio_init(PICO_DEFAULT_LED_PIN);
//   gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
//     gpio_put(PICO_DEFAULT_LED_PIN, state);

// We will need to be careful with how we handle this function as it looks
// like the gpio_config function is designed to initialise all pins at once
// so any consequitive calls may overwrite the previous ones - TODO:
void gpio_init(uint gpio) {
    // //zero-initialize the config structure.
    // gpio_config_t io_conf = {};
    // //disable interrupt
    // io_conf.intr_type = GPIO_INTR_DISABLE;
    // //set as output mode
    // io_conf.mode = GPIO_MODE_OUTPUT;
    // //bit mask of the pins that you want to set,e.g.GPIO18/19
    // io_conf.pin_bit_mask = gpio;
    // //disable pull-down mode
    // io_conf.pull_down_en = 0;
    // //disable pull-up mode
    // io_conf.pull_up_en = 0;
    // //configure GPIO with the given settings
    // gpio_config(&io_conf);
}
void gpio_pull_up(uint gpio) {
//  gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
//  gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);
	gpio_pullup_en(gpio);
}
void gpio_disable_pulls(uint gpio) {
//  gpio_disable_pulls(CEC_PIN);
	gpio_pullup_dis(gpio);
}
void gpio_set_dir(uint gpio, bool out) {
//  gpio_set_dir(CEC_PIN, GPIO_IN);
//  gpio_set_dir(CEC_PIN, GPIO_OUT);
//	gpio_reset_pin(gpio);
	gpio_set_direction(gpio, out == GPIO_OUT ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT);
}
//void gpio_set_function(int pin, int func) {
void gpio_set_function(uint gpio, enum gpio_function fn) {
//  gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
//  gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
}
//void gpio_set_irq_enabled(uint gpio, uint32_t event_mask, bool enabled);
void gpio_set_irq_enabled(uint gpio, uint32_t event_mask, bool enabled) {
//  gpio_set_irq_enabled(CEC_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
//  gpio_set_irq_enabled(CEC_PIN, GPIO_IRQ_EDGE_RISE, true);
//  gpio_set_irq_enabled(CEC_PIN, GPIO_IRQ_EDGE_FALL, true);

// typedef enum {
//     GPIO_INTR_DISABLE = 0,     /*!< Disable GPIO interrupt                             */
//     GPIO_INTR_POSEDGE = 1,     /*!< GPIO interrupt type : rising edge                  */
//     GPIO_INTR_NEGEDGE = 2,     /*!< GPIO interrupt type : falling edge                 */
//     GPIO_INTR_ANYEDGE = 3,     /*!< GPIO interrupt type : both rising and falling edge */
//     GPIO_INTR_LOW_LEVEL = 4,   /*!< GPIO interrupt type : input low level trigger      */
//     GPIO_INTR_HIGH_LEVEL = 5,  /*!< GPIO interrupt type : input high level trigger     */
//     GPIO_INTR_MAX,
// } gpio_int_type_t;
	if (enabled) {
		if (event_mask == (GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL)) gpio_set_intr_type(gpio, GPIO_INTR_ANYEDGE);
		if (event_mask == (GPIO_IRQ_EDGE_RISE)) gpio_set_intr_type(gpio, GPIO_INTR_POSEDGE);
		if (event_mask == (GPIO_IRQ_EDGE_FALL)) gpio_set_intr_type(gpio, GPIO_INTR_NEGEDGE);
	} else {
		gpio_set_intr_type(gpio, GPIO_INTR_DISABLE);
	}
}
void gpio_acknowledge_irq(uint gpio, uint32_t events) {}
void gpio_set_irq_callback(gpio_irq_callback_t callback) {
//   gpio_set_irq_callback(&hdmi_rx_frame_isr);
//   irq_set_enabled(IO_IRQ_BANK0, true);
//   gpio_set_irq_enabled(CEC_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);

    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(GPIO_INPUT_IO_0, (void*)callback, (void*) GPIO_INPUT_IO_0);
}
//gpio_function_t gpio_get_function(uint gpio) { return NULL; }
bool gpio_get(uint gpio) {
    return gpio_get_level(gpio);
}
void gpio_put(uint gpio, int value) {
    gpio_set_level(gpio, value);
}

// // Define a custom timer interrupt handler
// void IRAM_ATTR alarm_isr(void *arg) {
//   // Handle the alarm interrupt (e.g., set a flag, trigger another function)
// }

typedef struct {
    uint64_t event_count;
} example_queue_element_t;

static bool IRAM_ATTR example_timer_on_alarm_cb_v1(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_data)
{
    BaseType_t high_task_awoken = pdFALSE;
    QueueHandle_t queue = (QueueHandle_t)user_data;
    // stop timer immediately
    gptimer_stop(timer);
    // Retrieve count value and send to queue
    example_queue_element_t ele = {
        .event_count = edata->count_value
    };
    xQueueSendFromISR(queue, &ele, &high_task_awoken);
    // return whether we need to yield at the end of ISR
    return (high_task_awoken == pdTRUE);
}

#define ALARM_QUEUE_LENGTH (16)

static StaticQueue_t xStaticAlarmQueue;
static uint8_t storageAlarmQueue[ALARM_QUEUE_LENGTH * sizeof(uint8_t)];

gptimer_event_callbacks_t cbs = {
    .on_alarm = example_timer_on_alarm_cb_v1,
//    .on_alarm = alarm_isr,
};

void alarm_pool_init_default() {
    example_queue_element_t ele;
//    QueueHandle_t queue = xQueueCreate(10, sizeof(example_queue_element_t));
    QueueHandle_t queue = xQueueCreateStatic(10, sizeof(example_queue_element_t), &storageAlarmQueue[0], &xStaticAlarmQueue);
    if (!queue) {
        ESP_LOGE(TAG, "Creating queue failed");
        return;
    }

    ESP_LOGI(TAG, "Create timer handle");
    gptimer_handle_t gptimer = NULL;
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000, // 1MHz, 1 tick=1us
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));

//     gptimer_event_callbacks_t cbs = {
//         .on_alarm = example_timer_on_alarm_cb_v1,
// //        .on_alarm = alarm_isr,
//     };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, queue));

    ESP_LOGI(TAG, "Enable timer");
    ESP_ERROR_CHECK(gptimer_enable(gptimer));
}

//typedef int alarm_id_t;
//typedef unsigned int uint;
//typedef int64_t(*f_ptr)(int,void*);
//void add_alarm_at(int a, f_ptr fp, void* p, int b) {}
alarm_id_t add_alarm_at(absolute_time_t time, alarm_callback_t callback, void *user_data, bool fire_if_past) {



    return 0;
}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR hdmi-ddc.c
// #define PICO_ERROR_NONE 0
// #define PICO_ERROR_TIMEOUT 0
// #define PICO_ERROR_GENERIC 0
// #define i2c_default 0
// #define PICO_DEFAULT_I2C_SDA_PIN 0
// #define PICO_DEFAULT_I2C_SCL_PIN 0
// #define GPIO_FUNC_I2C 0
//void i2c_init(int a, int b) {}
//void i2c_deinit(int a) {}
int i2c_read_timeout_us(int a, int b, void* c, int d, int e, int f) { return 0; }
int i2c_write_timeout_us(int a, int b, void* c, int d, int e, int f) { return 0; }
//void gpio_set_function(int pin, int func) {}
//void gpio_pull_up(int pin) {}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR nvs.c
void restore_interrupts(uint32_t a) {}
uint32_t save_and_disable_interrupts() { return 0; }
void flash_range_erase(int address, int size) {}
void flash_range_program(int address, uint8_t * a, int size) {}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR blink.c
// #define PICO_DEFAULT_WS2812_PIN 0
void ws2812_init(unsigned int pin) {}
void ws2812_put_rgb(uint8_t red, uint8_t green, uint8_t blue) {}
void ws2812_put_pixel(uint32_t pixel_grb) {}
////////////////////////////////////////////////////////////////////////////////


//void cdc_log(const char *str) {}
uint64_t time_us_64(void) {
     return esp_timer_get_time();
}
//uint32_t crc32(unsigned char *, int size) { return 0; }
uint32_t CEC_NVS_BASE_ADDR[] = { 0 };
uint32_t __CEC_NVS_LEN[] = { 0 };


////////////////////////////////////////////////////////////////////////////////
// REQUIRED FOR usb_hid.c
void tud_init(int) {}
void tud_task() {
  static uint32_t _delay = 100;
  // following code only run if tud_task() process at least 1 event
  vTaskDelay(pdMS_TO_TICKS(_delay));
}
void tud_hid_keyboard_report(int a, int b, void* c) {}
int tud_suspended() { return 0; }
int tud_hid_ready() { return 0; }
void tud_remote_wakeup() {}

#include <string.h>
void tud_cdc_write_str(const char* str) {
    uart_write_bytes(UART_PORT_NUM, str, strlen(str));
}
int tud_cdc_connected() {
    return 1;
}
int tud_cdc_available() {
    size_t size;
    uart_get_buffered_data_len(UART_PORT_NUM, &size);
    return size;
}
uint8_t tud_cdc_read_char() {
    uint8_t data;
    uart_read_bytes(UART_PORT_NUM, &data, 1, 20 / portTICK_PERIOD_MS); // (20 / portTICK_PERIOD_MS) = 2
    ESP_LOGI(TAG, "Recv char: %c", data);
    return data;
}
void tud_cdc_write_flush() {
    vTaskDelay(pdMS_TO_TICKS(10));
//    uart_flush(UART_PORT_NUM);
}
////////////////////////////////////////////////////////////////////////////////

void reset_usb_boot(int mask, int unknown) {}
void watchdog_reboot(int a, int b, int c) {}

