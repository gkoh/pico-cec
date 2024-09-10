#include <hardware/watchdog.h>
#include <pico/bootrom.h>
#include <tusb.h>

#include "tclie.h"

#ifndef PICO_CEC_VERSION
#define PICO_CEC_VERSION "unknown"
#endif

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define _ENDLINE_SEQ "\r\n"

static void print(void *arg, const char *str) {
  tud_cdc_write_str(str);
}

static int exec_reboot(void *arg, int argc, const char **argv) {
  if ((argc == 2) && (strcmp(argv[1], "bootsel") == 0)) {
    // reboot into USB bootloader
    reset_usb_boot(PICO_DEFAULT_LED_PIN, 0);
  } else {
    // normal reboot
    watchdog_reboot(0, 0, 0);
  }

  return -1;
}

static int exec_version(void *arg, int argc, const char **argv) {
  print(arg, PICO_CEC_VERSION ""_ENDLINE_SEQ);
  return 0;
}

static const tclie_cmd_t cmds[] = {
    {"version", exec_version, "Display version.", "version"},
    {"reboot", exec_reboot, "Reboot system.", "reboot [bootsel]"},
};

void cdc_task(void *params) {
  (void)params;

  tclie_t tclie;

  tclie_init(&tclie, print, NULL);
  tclie_reg_cmds(&tclie, cmds, ARRAY_SIZE(cmds));

  while (1) {
    // connected() check for DTR bit
    // Most but not all terminal client set this when making connection
    if (tud_cdc_connected()) {
      // There are data available
      while (tud_cdc_available()) {
        uint8_t c = tud_cdc_read_char();
        tclie_input_char(&tclie, c);
      }

      tud_cdc_write_flush();
    }

    vTaskDelay(1);
  }
}

void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
  (void)itf;
  (void)rts;

  if (dtr) {
    // Terminal connected
    tud_cdc_write_str("Connected"_ENDLINE_SEQ);
  } else {
    // Terminal disconnected
    tud_cdc_write_str("Disconnected"_ENDLINE_SEQ);
  }
}
