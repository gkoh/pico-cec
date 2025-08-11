#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "portable.h"
DECLARE_TAG()

#include "cec-config.h"
#include "config-keymap.h"
#include "config.h"

#include "blink.h"
#include "cec-cmd.h"
#include "cec-frame.h"
#include "cec-log.h"
#include "cec-task.h"
#include "cec-util.h"
#include "ddc.h"
#include "nvs.h"
#include "tclie.h"
#include "usb-cdc.h"

#include "console.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

// Copy of configuration
static config_t config = {0x0};

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
// static void cdc_vprintf(const char *fmt, va_list ap) {
//   char buffer[128] = {0x00};
//   vsnprintf(buffer, 128, fmt, ap);
//   print(buffer);
// }

static int show_version(void *arg) {
  cdc_printfln("%s", PICO_CEC_VERSION);
#if defined(__XTENSA__) || defined(__riscv)
  cdc_printfln("Project: %s", PROJECT_NAME);
  cdc_printfln("Version: %s", PROJECT_VERSION);
  cdc_printfln("SDK_VER: %s", IDF_VER);
  cdc_printfln("Built: %s", BUILD_TIMESTAMP);
  cdc_printfln("Git: %s (%s)%s", GIT_COMMIT_HASH, GIT_BRANCH, GIT_DIRTY);
#endif
  return 0;
}

void save_logical_address(uint8_t addr);

static int debug_test(const char *arg) {
  if (strcmp(arg, "help") == 0) {
    cdc_printfln("frame|start|stop|dump|clear|rxint|erase");
  } else if (strcmp(arg, "frame") == 0) {
    cdc_printfln("Not implemented");
  } else if (strcmp(arg, "clear") == 0) {
    cec_frame_clear_stats();
  } else if (strcmp(arg, "rxint") == 0) {
    cec_frame_rxint();
  } else if (strcmp(arg, "erase") == 0) {
    cec_config_set_default(&config.cec);
    nvs_save_config(&config);
#if defined(__XTENSA__) || defined(__riscv)
    save_logical_address(0xFF);
#endif
  } else {
    cdc_printfln("Not implemented");
    return -1;
  }
  return 0;
}

static int capture(const char *arg) {
  int capture_bit_count = atoi(arg);
  cdc_printfln("capturing rx bits, timeout in %d seconds", capture_bit_count);
  // cec_frame_capture(true);
  void *ptr = cec_frame_capture(NULL);
  vTaskDelay(pdMS_TO_TICKS(1000 * capture_bit_count));
  // cec_frame_capture(false);
  cec_frame_capture(ptr);
  cec_frame_dump(tud_cdc_write_str);
  return 0;
}

int echo_destination_addr = 0;
int echo_rate = 50;

static int echo(const char *arg, const char *arg2) {
  echo_destination_addr = atoi(arg);
  if (arg2 != NULL) {
    echo_rate = atoi(arg2);
  }
  cdc_printfln("send echoes to %d at %d", echo_destination_addr, echo_rate);
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
    } else if (strcmp(argv[1], "mask") == 0) {
      cec_log_mask_toggle();
      return 0;
    }
  }
  if (argc >= 3) {
    if (strcmp(argv[1], "echo") == 0) {
      if (argc == 3) {
        echo(argv[2], NULL);
      } else {
        echo(argv[2], argv[3]);
      }
      return 0;
    }
    if (strcmp(argv[1], "capture") == 0) {
      capture(argv[2]);
      return 0;
    }
    if (strcmp(argv[1], "test") == 0) {
      debug_test(argv[2]);
      return 0;
    }
    if (strcmp(argv[1], "log") == 0) {
#if defined(__XTENSA__) || defined(__riscv)
      int level = atoi(argv[2]);
      esp_log_level_set("*", level);
#else
      // Not implemented
#endif
      return 0;
    }
  }

  return -1;
}

static int exec_monitor(void *arg, int argc, const char *argv[]) {
  if (argc == 2) {
    if (strcmp(argv[1], "on") == 0) {
      cec_frame_set_monitor_mode(0x03);
      return 0;
    } else if (strcmp(argv[1], "off") == 0) {
      cec_frame_set_monitor_mode(0x00);
      return 0;
    } else if (strcmp(argv[1], "raw") == 0) {
      cec_frame_set_monitor_mode(0x01);
      return 0;
    } else if (strcmp(argv[1], "prom") == 0) {
      cec_frame_set_monitor_mode(0x02);
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
    ESP_LOGI(TAG, "exec_reboot()");
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

static void print_monitor_mode(uint8_t monitor_mode) {
  cdc_printfln("%-17s: %s", "Promiscuous mode", monitor_mode ? "on" : "off");
}

static int show_config(config_t *config) {
  // UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
  // cdc_printfln("StackHighWaterMark = %lu", uxHighWaterMark);

  print_edid_delay(config->cec.edid_delay_ms);
  print_monitor_mode(config->cec.monitor_mode);
  print_physical_address(config->cec.physical_address);
  print_logical_address(config->cec.logical_address);

  const char *type = "unknown";
  switch ((cec_config_device_type_t)config->cec.device_type) {
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
  if (stats.rx_frames) {
    cdc_printfln("%-13s: %u %%", "rx abort rate",
                 (unsigned int)(stats.rx_abort_frames * 100 / stats.rx_frames));
  }
  cdc_printfln("%-13s: %lu", "idle timeouts", stats.idle_timeouts);
  cdc_printfln("%-13s: %lu us", "dwell period", stats.dwell_period_max);

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

#if (configTASKLIST_INCLUDE_COREID == 1)
  cdc_printfln("%-13s | %-6s | %-10s | %-10s", "task", "core", "priority", "stack (min*)");
  cdc_printfln("--------------------------------------------------");
#else
  cdc_printfln("%-13s | %-10s | %-10s", "task", "priority", "stack (min*)");
  cdc_printfln("-----------------------------------------");
#endif
  for (UBaseType_t i = 0; i < n; i++) {
#if (configTASKLIST_INCLUDE_COREID == 1)
    cdc_printfln("%-13s | %-6ld | %-10lu | %-10lu", status[i].pcTaskName,
                 (int32_t)status[i].xCoreID, (uint32_t)status[i].uxCurrentPriority,
                 (uint32_t)status[i].usStackHighWaterMark / STACK_WORDSIZE);
#else
    cdc_printfln("%-13s | %-10ld | %-10lu", status[i].pcTaskName,
                 (uint32_t)status[i].uxCurrentPriority,
                 (uint32_t)status[i].usStackHighWaterMark / STACK_WORDSIZE);
#endif
  }
  cdc_printfln("* closer to zero is approaching stack overflow");

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
      print_monitor_mode(cec_frame_get_monitor_mode());
      print_physical_address(cec_get_physical_address());
      print_logical_address(cec_get_logical_address());
    } else if (strcmp(argv[1], "version") == 0) {
      return show_version(arg);
    } else if (strcmp(argv[1], "nvs") == 0) {
      config_t nvs_config;
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
        config.cec.edid_delay_ms = atoi(argv[3]);
        print_edid_delay(config.cec.edid_delay_ms);
        return 0;
      } else if (strcmp(argv[2], "physical_address") == 0) {
        if (sscanf(argv[3], "%4hx", &config.cec.physical_address) == 1) {
          print_physical_address(config.cec.physical_address);
          return 0;
        } else {
          cdc_printfln("Error parsing physical address");
          return -1;
        }
      } else if (strcmp(argv[2], "logical_address") == 0) {
        if (sscanf(argv[3], "%hhx", &config.cec.logical_address) == 1) {
          config.cec.logical_address &= 0x0f;  // valid is 00 to 0f
          print_logical_address(config.cec.logical_address);
          return 0;
        } else {
          cdc_printfln("Error parsing logical address");
          return -1;
        }
      } else if (strcmp(argv[2], "device_type") == 0) {
        if (strcmp(argv[3], "playback") == 0) {
          config.cec.device_type = CEC_CONFIG_DEVICE_TYPE_PLAYBACK;
          return 0;
        } else if (strcmp(argv[3], "recording") == 0) {
          config.cec.device_type = CEC_CONFIG_DEVICE_TYPE_RECORDING;
          return 0;
        } else {
          cdc_printfln("Unknown device type \'%s\'", argv[3]);
          return -1;
        }
      } else if (strcmp(argv[2], "monitor_mode") == 0) {
        if (strcmp(argv[3], "on") == 0) {
          config.cec.monitor_mode = 1;
          print_monitor_mode(config.cec.monitor_mode);
          return 0;
        } else if (strcmp(argv[3], "off") == 0) {
          config.cec.monitor_mode = 0;
          print_monitor_mode(config.cec.monitor_mode);
          return 0;
        } else {
          cdc_printfln("Error parsing monitor mode");
          return -1;
        }
      }
    }
  } else if (argc == 3) {
    if (strcmp(argv[1], "keymap") == 0) {
      if (strcmp(argv[2], "kodi") == 0) {
        config.keymap_type = CEC_CONFIG_KEYMAP_KODI;
        config_keymap_set(&config);
        return 0;
      } else if (strcmp(argv[2], "mister") == 0) {
        config.keymap_type = CEC_CONFIG_KEYMAP_MISTER;
        config_keymap_set(&config);
        return 0;
      } else {
        cdc_printfln("Unknown keymap '%s'", argv[2]);
        return -1;
      }
    } else if (strcmp(argv[1], "led") == 0) {
      uint8_t value = atoi(argv[2]);
      blink_set_intensity(value);
    }
  }

  return -1;
}

static int exec_send(void *arg, int argc, const char **argv) {
  // return cec_cmd_send(cdc_printf, argc, argv);
  int res = cec_cmd_send(cdc_printf, argc, argv);
  if (res < 0) {
    printf("command '%s' unknown\n", argv[1]);
  } else {
    cec_log_submitf("command '%s' sent\n", argv[1]);
  }
  return res;
}

static const tclie_cmd_t cmds[] = {
    {"debug", exec_debug,
     "Developer commands (log levels: None, Error, Warning, Info, Debug, Verbose)",
     "debug {on|off|mask|(echo <address> [<rate>])|(capture <seconds>)|(key <'char'>)|(test "
     "{help|<name>})|(log {0|1|2|3|4|5})}"},
    {"monitor", exec_monitor, "CEC bus monitor mode. (pro=promiscuous)",
     "monitor {on|off|raw|pro}"},
    {"query", exec_query, "Query information.", "query {edid}"},
    {"send", exec_send, "Send CEC command.",
     "send {help|<cec-command> [<destination address>] [<source address>]}"},
    {"save", exec_save, "Save configuration.", "save"},
    {"set", exec_set, "Set configuration parameters.",
     "set {(config (edid_delay_ms|logical_address|physical_address|monitor_mode "
     "<value>)|(device_type "
     "{playback|recording}))|(keymap <value>)|(led <intensity>)}"},
    {"show", exec_show, "Show information.",
     "show {cec|config|keymap|nvs|(stats {cec|cpu|tasks})|version}"},
    {"reboot", exec_reboot, "Reboot system.", "reboot [bootsel]"},
};

void console_output(const char *str) {
  tclie_log(&tclie, str);
}

void console_input(char c) {
  tclie_input_char(&tclie, c);
}

void console_init(void) {
  nvs_load_config(&config);

  tclie_init(&tclie, tcli_print, NULL);
  tclie_reg_cmds(&tclie, cmds, ARRAY_SIZE(cmds));
}
