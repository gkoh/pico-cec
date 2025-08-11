#ifndef CEC_HAL_H
#define CEC_HAL_H

#include <stdbool.h>

#include "FreeRTOS.h"
#include "queue.h"  // required for pico build, but not for esp
#include "task.h"

#if defined(__XTENSA__) || defined(__riscv)
#include "../esp-port.h"
#else
#include "../rp-port.h"
#endif  // __XTENSA__

typedef void (*cec_frame_rx_callback_t)(uint32_t edge_time);
typedef uint32_t (*cec_frame_tx_callback_t)(void *user_data);

void cec_hal_bus_high(void);
void cec_hal_bus_low(void);
bool cec_hal_bus_get(void);

#define GPIO_IRQ_EDGE_FALL 0x4u
#define GPIO_IRQ_EDGE_RISE 0x8u

#define CEC_HAL_RX_IRQ_NONE 0x0u
#define CEC_HAL_RX_IRQ_FALL 0x4u
#define CEC_HAL_RX_IRQ_RISE 0x8u
void cec_hal_rx_irq(uint32_t event_mask, bool enabled);

void cec_hal_init(unsigned int gpio, cec_frame_rx_callback_t callback);
void cec_hal_frame_tx(uint32_t time, cec_frame_tx_callback_t callback, void *user_data);

void *cec_hal_swap_rx_isr(void *);  // for cec-util.c

void cec_hal_YIELD_FROM_ISR(BaseType_t);

#endif
