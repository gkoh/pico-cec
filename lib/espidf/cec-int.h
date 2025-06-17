#ifndef _CEC_INT_H_
#define _CEC_INT_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "sdkconfig.h"
#include "portable.h"
//typedef unsigned int uint;


// For timer support:
// CONFIG_ESP_TIMER_SUPPORTS_ISR_DISPATCH_METHOD=y  // sdkconfig

#define IO_IRQ_BANK0 0
#define GPIO_IRQ_EDGE_RISE 0
#define GPIO_IRQ_EDGE_FALL 0

uint64_t time_us_64(void);

typedef void (*gpio_irq_callback_t)(uint gpio, uint32_t event_mask);

void irq_set_enabled(int a, int b);
void gpio_set_irq_enabled(uint gpio, uint32_t event_mask, bool enabled);
void gpio_acknowledge_irq(uint gpio, uint32_t events);
void gpio_set_irq_callback(gpio_irq_callback_t callback);


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

void alarm_pool_init_default();

typedef int32_t alarm_id_t; // note this is signed because we use <0 as a meaningful error value
typedef int64_t (*alarm_callback_t)(alarm_id_t id, void *user_data);
alarm_id_t add_alarm_at(absolute_time_t time, alarm_callback_t callback, void *user_data, bool fire_if_past);

#endif // _CEC_INT_H_
