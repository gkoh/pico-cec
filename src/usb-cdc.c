#include <stdlib.h>
#include <string.h>
#include <tusb.h>

#include <hardware/watchdog.h>
#include <pico/bootrom.h>

#include "pico-cec/config.h"
#include "pico-cec/util.h"

#include "cec-frame.h"
#include "cec-log.h"
#include "cec-task.h"
#include "ddc.h"
#include "nvs.h"
#include "tclie.h"
#include "usb-cdc.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

// Copy of configuration
static cec_config_t config = {0x0};

static tclie_t tclie;

/** Print string to CDC output. */
static void print(const char *str) {
  tud_cdc_write_str(str);
  vTaskDelay(pdMS_TO_TICKS(1));  // needed to avoid garbled output
}

/** tcli print callback function. */
static void tcli_print(void *arg, const char *str) {
  print(str);
}

/** Print formatted string with variadic parameter list. */
static void cdc_vprintf(const char *fmt, va_list ap) {
  char buffer[128] = {0x00};
  vsnprintf(buffer, 128, fmt, ap);
  print(buffer);
}

void cdc_log(const char *str) {
  tclie_log(&tclie, str);
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

static int show_version(void *arg) {
  cdc_printfln("%s", PICO_CEC_VERSION);
  return 0;
}

static int exec_debug(void *arg, int argc, const char *argv[]) {
  if (argc == 2) {
    if (strcmp(argv[1], "on") == 0) {
      cec_log_enable();
      return 0;
    } else if (strcmp(argv[1], "off") == 0) {
      cec_log_disable();
      return 0;
    }
  }

  return -1;
}

static int exec_reboot(void *arg, int argc, const char **argv) {
  if ((argc == 2) && (strcmp(argv[1], "bootsel") == 0)) {
    // reboot into USB bootloader
#ifdef PICO_DEFAULT_LED_PIN
    uint32_t activity_mask = PICO_DEFAULT_LED_PIN;
#else
    uint32_t activity_mask = 0;
#endif
    reset_usb_boot(activity_mask, 0);
  } else {
    // normal reboot
    watchdog_reboot(0, 0, 0);
  }

  return -1;
}

static void print_edid_delay(uint32_t delay) {
  cdc_printfln("%-17s: %lu ms", "EDID delay", delay);
}

static void print_physical_address(uint16_t address) {
  cdc_printfln("%-17s: 0x%04x", "Physical address", address);
}

static void print_logical_address(uint8_t address) {
  cdc_printfln("%-17s: 0x%02x", "Logical address", address);
}

static int show_config(cec_config_t *config) {
  // UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
  // cdc_printfln("StackHighWaterMark = %lu", uxHighWaterMark);

  print_edid_delay(config->edid_delay_ms);
  print_physical_address(config->physical_address);
  print_logical_address(config->logical_address);
  const char *type = "unknown";
  switch ((cec_config_device_type_t)config->device_type) {
    case CEC_CONFIG_DEVICE_TYPE_TV:
      type = "TV";
      break;
    case CEC_CONFIG_DEVICE_TYPE_RECORDING:
      type = "recording";
      break;
    case CEC_CONFIG_DEVICE_TYPE_RESERVED:
      type = "reserved";
      break;
    case CEC_CONFIG_DEVICE_TYPE_PLAYBACK:
      type = "playback";
      break;
    case CEC_CONFIG_DEVICE_TYPE_TUNER:
      type = "tuner";
      break;
    case CEC_CONFIG_DEVICE_TYPE_AUDIO_SYSTEM:
      type = "audio";
      break;
  }
  cdc_printfln("%-17s: %s", "Device type", type);

  const char *keymap = "unknown";
  switch (config->keymap_type) {
    case CEC_CONFIG_KEYMAP_CUSTOM:
      keymap = "custom";
      break;
    case CEC_CONFIG_KEYMAP_KODI:
      keymap = "Kodi";
      break;
    case CEC_CONFIG_KEYMAP_MISTER:
      keymap = "MiSTer";
      break;
  }
  cdc_printfln("%-17s: %s", "Keymap", keymap);

  return 0;
}

static int show_stats_cec(void) {
  cec_frame_stats_t stats = {0x0};
  cec_frame_get_stats(&stats);
  cdc_printfln("%-13s: %lu frames", "CEC rx", stats.rx_frames);
  cdc_printfln("%-13s: %lu frames", "CEC tx", stats.tx_frames);
  cdc_printfln("%-13s: %lu frames", "CEC rx abort", stats.rx_abort_frames);
  cdc_printfln("%-13s: %lu frames", "CEC tx noack", stats.tx_noack_frames);

  return 0;
}

static int show_stats_cpu(void) {
  UBaseType_t count = uxTaskGetNumberOfTasks();
  TaskStatus_t status[count];
  unsigned long total_run_time = 0;

  UBaseType_t n = uxTaskGetSystemState(status, count, &total_run_time);

  uint64_t uptime = util_uptime_ms() / 1000;
  uint64_t seconds = uptime % 60;
  uptime /= 60;
  uint64_t minutes = uptime % 60;
  uptime /= 60;
  uint64_t hours = uptime % 24;
  uint64_t days = uptime / 24;

  cdc_printfln("%-13s: %llud %lluh %llum %llus", "Uptime", days, hours, minutes, seconds);

  for (UBaseType_t i = 0; i < n; i++) {
    cdc_printfln("%-13s: %7.3f %%", status[i].pcTaskName,
                 (100.0f * status[i].ulRunTimeCounter) / total_run_time);
  }

  return 0;
}

static int show_stats_tasks(void) {
  UBaseType_t count = uxTaskGetNumberOfTasks();
  TaskStatus_t status[count];

  UBaseType_t n = uxTaskGetSystemState(status, count, NULL);

  cdc_printfln("%-13s | %-10s | %-10s", "task", "priority", "stack (min)");

  for (UBaseType_t i = 0; i < n; i++) {
    cdc_printfln("%-13s | %-10lu | %-10lu", status[i].pcTaskName,
                 (uint32_t)status[i].uxCurrentPriority, (uint32_t)status[i].usStackHighWaterMark);
  }

  return 0;
}

static int exec_show(void *arg, int argc, const char **argv) {
  if (argc == 2) {
    if (strcmp(argv[1], "config") == 0) {
      return show_config(&config);
    } else if (strcmp(argv[1], "keymap") == 0) {
      for (uint8_t n = 0; n < UINT8_MAX; n++) {
        if (config.keymap[n].name != NULL) {
          cdc_printfln(" 0x%02x : %02u : %s", n, config.keymap[n].key, config.keymap[n].name);
        }
      }
    } else if (strcmp(argv[1], "cec") == 0) {
      print_physical_address(cec_get_physical_address());
      print_logical_address(cec_get_logical_address());
    } else if (strcmp(argv[1], "version") == 0) {
      return show_version(arg);
    } else if (strcmp(argv[1], "nvs") == 0) {
      cec_config_t nvs_config;
      if (nvs_read_config(&nvs_config)) {
        return show_config(&nvs_config);
      } else {
        cdc_printfln("Failed to read configuration from NVS.");
        return -1;
      }
    }
  } else if (argc == 3) {
    if (strcmp(argv[1], "stats") == 0) {
      if (strcmp(argv[2], "cec") == 0) {
        return show_stats_cec();
      } else if (strcmp(argv[2], "cpu") == 0) {
        return show_stats_cpu();
      } else if (strcmp(argv[2], "tasks") == 0) {
        return show_stats_tasks();
      }
    }
  }

  return -1;
}

static int exec_query(void *arg, int argc, const char **argv) {
  if (argc == 2) {
    if (strcmp(argv[1], "edid") == 0) {
      print_physical_address(ddc_get_physical_address());
      return -1;
    }
  }

  return 0;
}

static int exec_save(void *arg, int argc, const char **argv) {
  // UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
  // cdc_printfln("StackHighWaterMark = %lu", uxHighWaterMark);

  bool r = nvs_save_config(&config);

  // cdc_printfln("r = %u", r);

  return r ? 0 : -1;
}

static int exec_set(void *arg, int argc, const char **argv) {
  if (argc == 4) {
    if (strcmp(argv[1], "config") == 0) {
      if (strcmp(argv[2], "edid_delay_ms") == 0) {
        config.edid_delay_ms = atoi(argv[3]);
        print_edid_delay(config.edid_delay_ms);
        return 0;
      } else if (strcmp(argv[2], "physical_address") == 0) {
        if (sscanf(argv[3], "%4hx", &config.physical_address) == 1) {
          print_physical_address(config.physical_address);
          return 0;
        } else {
          cdc_printfln("Error parsing physical address");
          return -1;
        }
      } else if (strcmp(argv[2], "logical_address") == 0) {
        if (sscanf(argv[3], "%hhx", &config.logical_address) == 1) {
          config.logical_address &= 0x0f;  // valid is 00 to 0f
          print_logical_address(config.logical_address);
          return 0;
        } else {
          cdc_printfln("Error parsing logical address");
          return -1;
        }
      } else if (strcmp(argv[2], "device_type") == 0) {
        if (strcmp(argv[3], "playback") == 0) {
          config.device_type = CEC_CONFIG_DEVICE_TYPE_PLAYBACK;
          return 0;
        } else if (strcmp(argv[3], "recording") == 0) {
          config.device_type = CEC_CONFIG_DEVICE_TYPE_RECORDING;
          return 0;
        } else {
          cdc_printfln("Unknown device type \'%s\'", argv[3]);
          return -1;
        }
      }
    }
  } else if (argc == 3) {
    if (strcmp(argv[1], "keymap") == 0) {
      if (strcmp(argv[2], "kodi") == 0) {
        config.keymap_type = CEC_CONFIG_KEYMAP_KODI;
        cec_config_set_keymap(&config);
        return 0;
      } else if (strcmp(argv[2], "mister") == 0) {
        config.keymap_type = CEC_CONFIG_KEYMAP_MISTER;
        cec_config_set_keymap(&config);
        return 0;
      } else {
        cdc_printfln("Unknown keymap '%s'", argv[2]);
        return -1;
      }
    }
  }

  return -1;
}

static const tclie_cmd_t cmds[] = {
    {"debug", exec_debug, "Control debug output.", "debug {on|off}"},
    {"query", exec_query, "Query information.", "query {edid}"},
    {"save", exec_save, "Save configuration.", "save"},
    {"set", exec_set, "Set configuration parameters.",
     "set {(config (edid_delay_ms|logical_address|physical_address <value>)|(device_type "
     "{playback|recording}))|(keymap <value>)}"},
    {"show", exec_show, "Show information.",
     "show {cec|config|keymap|nvs|(stats {cec|cpu|tasks})|version}"},
    {"reboot", exec_reboot, "Reboot system.", "reboot [bootsel]"},
};

void cdc_task(void *params) {
  (void)params;

  nvs_load_config(&config);

  tclie_init(&tclie, tcli_print, NULL);
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
    tud_cdc_write_str("Connected"_CDC_BR);
  } else {
    // Terminal disconnected
    tud_cdc_write_str("Disconnected"_CDC_BR);
  }
}
