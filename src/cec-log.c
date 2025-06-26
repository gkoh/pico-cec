#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "message_buffer.h"
#include "task.h"

#include "portable.h"
DECLARE_TAG()

#include "cec-log.h"
#include "config.h"
#include "usb-cdc.h"

#define LOG_LINE_LENGTH (64)
#define LOG_QUEUE_LENGTH (16)
#define LOG_MB_SIZE (LOG_LINE_LENGTH * LOG_QUEUE_LENGTH)

static StaticTask_t log_task_static;
static StackType_t log_stack[LOG_STACK_SIZE];

static StaticMessageBuffer_t log_mb_static;
static MessageBufferHandle_t log_mb;
static uint8_t log_mb_storage[LOG_MB_SIZE];

static volatile bool enabled = false;

static void cec_log_task(void *param) {
  MessageBufferHandle_t *mb = (MessageBufferHandle_t *)param;  // yes, this is already a local just
                                                               // above, but for design consistency

  while (true) {
    char buffer[LOG_LINE_LENGTH];

    size_t bytes = xMessageBufferReceive(*mb, buffer, sizeof(buffer) - 2, pdMS_TO_TICKS(100));
    if (bytes > 0) {
      ESP_LOGI("log", "%s", buffer);

      strcat(buffer, "\r\n");
      cdc_log(buffer);
    }
  }
}

void cec_log_init(void) {
  log_mb = xMessageBufferCreateStatic(LOG_MB_SIZE, &log_mb_storage[0], &log_mb_static);
  enabled = false;

  xTaskCreateStatic(cec_log_task, "log", LOG_STACK_SIZE, &log_mb, LOG_PRIORITY, &log_stack[0],
                    &log_task_static);
  ESP_LOGI(TAG, "cec_log_init()");
}

bool cec_log_enabled(void) {
  return enabled;
}

void cec_log_enable(void) {
  enabled = true;
  ESP_LOGI(TAG, "cec_log_enable()");
}

void cec_log_disable(void) {
  enabled = false;
  ESP_LOGI(TAG, "cec_log_disable()");
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
