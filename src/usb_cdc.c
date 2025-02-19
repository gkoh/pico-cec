#include <hardware/watchdog.h>
#include <pico/bootrom.h>
#include <stdlib.h>
#include <tusb.h>

#include "hdmi-cec.h"
#include "nvs.h"
#include "tclie.h"

#ifndef PICO_CEC_VERSION
#define PICO_CEC_VERSION "unknown"
#endif

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define _ENDLINE_SEQ "\r\n"

// Copy of configuration
static cec_config_t config = {0x0};

/** Print string to CDC output. */
static void print(void *arg, const char *str) {
  tud_cdc_write_str(str);
  vTaskDelay(pdMS_TO_TICKS(1));  // needed to minimise deadlocking
}

/** Print formatted string with variadic parameter list. */
static void cdc_vprintf(void *arg, const char *fmt, va_list ap) {
  char buffer[128] = {0x00};
  vsnprintf(buffer, 128, fmt, ap);
  print(arg, buffer);
}

#if 0
/** Print formatted string. */
__attribute__ ((format (printf, 2, 3))) static void cdc_printf(void *arg, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  cdc_vprintf(arg, fmt, ap);
  va_end(ap);
}
#endif

/** Print formatted string with newline. */
__attribute__((format(printf, 2, 3))) static void cdc_printfln(void *arg, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  cdc_vprintf(arg, fmt, ap);
  va_end(ap);
  print(arg, _ENDLINE_SEQ);
}

static int show_version(void *arg) {
  print(arg, PICO_CEC_VERSION ""_ENDLINE_SEQ);
  return 0;
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

static void print_edid_delay(void *arg) {
  cdc_printfln(arg, "%-17s: %lu ms", "EDID delay", config.edid_delay_ms);
}

static void print_physical_address(void *arg, uint16_t address) {
  cdc_printfln(arg, "%-17s: 0x%04x", "Physical address", address);
}

static int show_config(void *arg) {
  // UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
  // cdc_printfln(arg, "StackHighWaterMark = %lu", uxHighWaterMark);

  print_edid_delay(arg);
  print_physical_address(arg, config.physical_address);

  return 0;
}

static int show_stats_cec(void *arg) {
  hdmi_cec_stats_t stats = {0x0};
  cec_get_stats(&stats);
  cdc_printfln(arg, "%-13s: %lu frames", "CEC rx", stats.rx_frames);
  cdc_printfln(arg, "%-13s: %lu frames", "CEC tx", stats.tx_frames);
  cdc_printfln(arg, "%-13s: %lu frames", "CEC rx abort", stats.rx_abort_frames);
  cdc_printfln(arg, "%-13s: %lu frames", "CEC tx noack", stats.tx_noack_frames);

  return 0;
}

static int show_stats_cpu(void *arg) {
  UBaseType_t count = uxTaskGetNumberOfTasks();
  TaskStatus_t status[count];
  unsigned long total_run_time = 0;

  UBaseType_t n = uxTaskGetSystemState(status, count, &total_run_time);

  uint64_t uptime = cec_get_uptime_ms() / 1000;
  uint64_t seconds = uptime % 60;
  uptime /= 60;
  uint64_t minutes = uptime % 60;
  uptime /= 60;
  uint64_t hours = uptime % 24;
  uint64_t days = uptime / 24;

  cdc_printfln(arg, "%-13s: %llud %lluh %llum %llus", "Uptime", days, hours, minutes, seconds);

  for (UBaseType_t i = 0; i < n; i++) {
    cdc_printfln(arg, "%-13s: %7.3f %%", status[i].pcTaskName,
                 (100.0f * status[i].ulRunTimeCounter) / total_run_time);
  }

  return 0;
}

static int exec_show(void *arg, int argc, const char **argv) {
  if (argc == 2) {
    if (strcmp(argv[1], "config") == 0) {
      return show_config(arg);
    } else if (strcmp(argv[1], "keymap") == 0) {
      for (uint8_t n = 0; n < UINT8_MAX; n++) {
        if (config.keymap[n].name != NULL) {
          cdc_printfln(arg, " 0x%02x : %02u : %s", n, config.keymap[n].key, config.keymap[n].name);
        }
      }
    } else if (strcmp(argv[1], "cec") == 0) {
      print_physical_address(arg, cec_get_physical_address());
      cdc_printfln(arg, "%-17s: 0x%02x", "Logical address", cec_get_logical_address());
    } else if (strcmp(argv[1], "version") == 0) {
      return show_version(arg);
    }
  } else if (argc == 3) {
    if (strcmp(argv[1], "stats") == 0) {
      if (strcmp(argv[2], "cec") == 0) {
        return show_stats_cec(arg);
      } else if (strcmp(argv[2], "cpu") == 0) {
        return show_stats_cpu(arg);
      }
    }
  }

  return -1;
}

static int exec_save(void *arg, int argc, const char **argv) {
  // UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
  // cdc_printfln(arg, "StackHighWaterMark = %lu", uxHighWaterMark);

  bool r = nvs_save_config(&config);

  // cdc_printfln(arg, "r = %u", r);

  return r ? 0 : -1;
}

static int exec_set(void *arg, int argc, const char **argv) {
  if (argc == 4) {
    if (strcmp(argv[1], "config") == 0) {
      if (strcmp(argv[2], "edid_delay_ms") == 0) {
        config.edid_delay_ms = atoi(argv[3]);
        print_edid_delay(arg);
        return 0;
      } else if (strcmp(argv[2], "physical_address") == 0) {
        if (sscanf(argv[3], "%4hx", &config.physical_address) == 1) {
          print_physical_address(arg, config.physical_address);
          return 0;
        } else {
          cdc_printfln(arg, "Error parsing physical address");
          return -1;
        }
      }
    }
  } else if (argc == 3) {
    if (strcmp(argv[1], "keymap") == 0) {
      if (strcmp(argv[2], "kodi") == 0) {
        cec_config_set_keymap(CEC_CONFIG_DEFAULT_KODI, &config);
        return 0;
      } else {
        cdc_printfln(arg, "Unknown keymap '%s'", argv[2]);
        return -1;
      }
    }
  }

  return -1;
}

static const tclie_cmd_t cmds[] = {
    {"save", exec_save, "Save configuration.", "save"},
    {"set", exec_set, "Set configuration parameters.",
     "set {(config edid_delay_ms|physical_address <value>)|(keymap <value>)}"},
    {"show", exec_show, "Show information.", "show {cec|config|keymap|(stats {cec|cpu})|version}"},
    {"reboot", exec_reboot, "Reboot system.", "reboot [bootsel]"},
};

void cdc_task(void *params) {
  (void)params;

  nvs_load_config(&config);

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
      vTaskDelay(pdMS_TO_TICKS(1));
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
    tud_cdc_write_str("Connected"_ENDLINE_SEQ);
  } else {
    // Terminal disconnected
    tud_cdc_write_str("Disconnected"_ENDLINE_SEQ);
  }
}
