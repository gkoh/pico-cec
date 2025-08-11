#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "portable.h"
DECLARE_TAG()

#include "config.h"

#include "cec-cmd.h"
#include "cec-frame.h"
#include "cec-log.h"
#include "cec-task.h"
#include "cec-util.h"
#include "console.h"
#include "ddc.h"
#include "nvs.h"
#include "tclie.h"
#include "usb-cdc.h"

#define _CDC_BR "\r\n"

/** Print string to CDC output. */
static void print(const char *str) {
  tud_cdc_write_str(str);
  vTaskDelay(pdMS_TO_TICKS(1));  // needed to avoid garbled output
}

/** Print formatted string with variadic parameter list. */
static void cdc_vprintf(const char *fmt, va_list ap) {
  char buffer[128] = {0x00};
  vsnprintf(buffer, 128, fmt, ap);
  print(buffer);
}

/** Print formatted string. */
void cdc_printf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  cdc_vprintf(fmt, ap);
  va_end(ap);
}

/** Print formatted string with newline. */
void cdc_printfln(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  cdc_vprintf(fmt, ap);
  va_end(ap);
  print(_CDC_BR);
}

void cdc_task(void *params) {
  (void)params;

  console_init();

  while (1) {
    // connected() check for DTR bit
    // Most but not all terminal client set this when making connection
    if (tud_cdc_connected()) {
      // There are data available
      while (tud_cdc_available()) {
        uint8_t c = tud_cdc_read_char();
        console_input(c);
      }
      tud_cdc_write_flush();
      vTaskDelay(pdMS_TO_TICKS(10));
    } else {
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  }
}

void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
  (void)itf;
  (void)rts;

  if (dtr) {
    // Terminal connected
    tud_cdc_write_str("Connected" _CDC_BR);
  } else {
    // Terminal disconnected
    tud_cdc_write_str("Disconnected" _CDC_BR);
  }
}
