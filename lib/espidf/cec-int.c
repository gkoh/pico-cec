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
//static const char *TAG = "cec_int";

#include "driver/gpio.h"
#include "driver/gptimer.h"

#include "pico/stdlib.h"
#include "portable.h"
#include "cec-gpio.h"
#include "cec-int.h"

// For timer support:
// CONFIG_ESP_TIMER_SUPPORTS_ISR_DISPATCH_METHOD=y  // sdkconfig

#define ESP_INTR_FLAG_DEFAULT 0

uint64_t time_us_64(void) {
    return esp_timer_get_time();
}

// #define IO_IRQ_BANK0 0
// #define GPIO_IRQ_EDGE_RISE 0
// #define GPIO_IRQ_EDGE_FALL 0
void irq_set_enabled(int a, int b) {}

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

//typedef int alarm_id_t;
//typedef unsigned int uint;
//typedef int64_t(*f_ptr)(int,void*);
//void add_alarm_at(int a, f_ptr fp, void* p, int b) {}
alarm_id_t add_alarm_at(absolute_time_t time, alarm_callback_t callback, void *user_data, bool fire_if_past) {
    ESP_LOGI(TAG, "add_alarm_at %d", 42);

    return 0;
}

