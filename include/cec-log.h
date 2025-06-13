#ifndef CEC_LOG_H
#define CEC_LOG_H

#include <stdarg.h>
#include <stdbool.h>

void cec_log_init(void);
bool cec_log_enabled();
void cec_log_enable(void);
void cec_log_disable(void);
void cec_log_vsubmitf(const char *fmt, va_list ap);
__attribute__((format(printf, 1, 2))) void cec_log_submitf(const char *fmt, ...);

#endif
