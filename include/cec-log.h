#ifndef CEC_LOG_H
#define CEC_LOG_H

#include <stdarg.h>
#include <stdbool.h>

#include "usb-cdc.h"

#define _LOG_BR _CDC_BR

typedef struct cec_frame_t cec_frame_t;
typedef void (*log_callback_t)(const char *str);

void cec_log_init(log_callback_t log);
bool cec_log_enabled();
void cec_log_enable(void);
void cec_log_disable(void);
void cec_log_frame(cec_frame_t *frame, bool recv);
void cec_log_vsubmitf(const char *fmt, va_list ap);
__attribute__((format(printf, 1, 2))) void cec_log_submitf(const char *fmt, ...);

uint64_t cec_log_uptime_ms(void);

#endif
