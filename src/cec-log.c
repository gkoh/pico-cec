#include <stdarg.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "message_buffer.h"
#include "task.h"

#include "cec-log.h"
#include "usb-cdc.h"

#define LOG_TASK_STACK_SIZE (2048)
#define LOG_LINE_LENGTH (64)
#define LOG_QUEUE_LENGTH (16)
#define LOG_MB_SIZE (LOG_LINE_LENGTH * LOG_QUEUE_LENGTH)

static StaticTask_t log_task_static;
static StackType_t log_stack[LOG_TASK_STACK_SIZE];

static StaticMessageBuffer_t log_mb_static;
static MessageBufferHandle_t log_mb;
static uint8_t log_mb_storage[LOG_MB_SIZE];

static volatile bool enabled = false;

static void cec_log_task(void *param) {
  while (true) {
    char buffer[LOG_LINE_LENGTH];

    size_t bytes = xMessageBufferReceive(log_mb, buffer, sizeof(buffer), pdMS_TO_TICKS(10));
    if (bytes > 0) {
      cdc_log(buffer);
    }
  }
}

void cec_log_init(void) {
  log_mb = xMessageBufferCreateStatic(LOG_MB_SIZE, &log_mb_storage[0], &log_mb_static);
  enabled = false;

  xTaskCreateStatic(cec_log_task, "log", LOG_TASK_STACK_SIZE, NULL, configMAX_PRIORITIES - 4,
                    &log_stack[0], &log_task_static);
}

bool cec_log_enabled(void) {
  return enabled;
}

void cec_log_enable(void) {
  enabled = true;
}

void cec_log_disable(void) {
  enabled = false;
}

void cec_log_vsubmitf(const char *fmt, va_list ap) {
  if (enabled) {
    char buffer[LOG_LINE_LENGTH];

    int bytes = vsnprintf(buffer, sizeof(buffer), fmt, ap);
    if (bytes < sizeof(buffer)) {
      xMessageBufferSend(log_mb, buffer, bytes + 1, pdMS_TO_TICKS(20));
    }
  }
}

void cec_log_submitf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  cec_log_vsubmitf(fmt, ap);
  va_end(ap);
}
