#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cec-cmd.h"
#include "cec-frame.h"
#include "cec-task.h"

typedef struct cec_tagCmds {
  const char *cmdstr;
  const char *frame;
  int size;
} cec_cmds_t;

cec_cmds_t ceclist[] = {
    {"ping", "\xff", 0},  // special case, payload unused
    {"vendor", "\x8C", 1},
    {"on", "\x04", 1},
    {"off", "\x36", 1},  // this only works if the actual source address sent out is 5
    {"on2", "\x44\x6D", 2},
    {"off2", "\x44\x6C", 2},   // this works for the LG TV
    {"getcec", "\x9F", 1},     // get CEC version
    {"getphys", "\x83", 1},    // get physical address
    {"getstate", "\x8F", 1},   // get power state
    {"getosd", "\x46", 1},     // get OSD name - LG TV appears to ignore this
    {"getvendor", "\x87", 1},  // get vendor ID
    {"input1", "\x82\x10\x00", 3},
    {"input2", "\x82\x20\x00", 3},
    {"input3", "\x82\x30\x00", 3},
    {"input4", "\x82\x40\x00", 3},
    {"standby", "\x44\x36", 2},
    {"toggle", "\x44\x6B", 2},  // turns tv on, but not off?
    {"up", "\x44\x41", 2},
    {"down", "\x44\x42", 2},
    {"mute", "\x44\x43", 2},     // not working
    {"unmute", "\x44\x44", 2},   // not working
    {"volup", "\x44\x41", 2},    // remote control pass through - pressed - volume up
    {"voldown", "\x44\x42", 2},  // remote control pass through - pressed - volume down
    {"mute2", "\x44\x42", 2},    // remote control pass through - pressed - mute function
    {"released", "\x45", 1},     // remote control pass through - user control released
    {"echo", "\xfa\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e", 15},  // custom message
    // {"echo", "\xfa\x55\xaa\x55\xaa\x55\xaa\x55\xaa\x55\xaa\x55\xaa\x55\xaa", 15},
};

#define MASKED_HEADER0(iaddr, daddr) ((iaddr << 4) | (daddr & 0x0f))

static bool send_frame(uint8_t address, const unsigned char *frame, int count) {
  uint8_t pld[count + 1];

  pld[0] = address;
  memcpy(&pld[1], frame, count);
  // ESP_LOGI(TAG, "send_frame(address %02x, cmd %02x, count %d)", pld[0], pld[1], count);
  cec_frame_send(count + 1, pld, true);
  return true;
}

static int send_message(const char *cmdstr, int src_addr, int dst_addr) {
  for (size_t i = 0; i < (sizeof(ceclist) / sizeof(ceclist[0])); i++) {
    if (strcmp(ceclist[i].cmdstr, cmdstr) == 0) {
      uint8_t address = MASKED_HEADER0(src_addr, dst_addr);
      send_frame(address, (const unsigned char *)ceclist[i].frame, ceclist[i].size);
      return 0;
    }
  }
  return -1;
}

int send_message_to(const char *cmdstr, int dst_addr) {
  uint8_t src_addr = cec_get_logical_address();
  return send_message(cmdstr, src_addr, dst_addr);
}

int cec_cmd_send(printf_ptr_t _printf, int argc, const char **argv) {
  uint8_t src_addr = cec_get_logical_address();
  uint8_t dst_addr = 0x0;
  if (argc == 2) {
    if (strcmp(argv[1], "help") == 0) {
      _printf("Usage:\n");
      for (size_t i = 0; i < (sizeof(ceclist) / sizeof(ceclist[0])); i++) {
        _printf("\t%s\r\n", ceclist[i].cmdstr);
      }
      return 0;
    } else if (strcmp(argv[1], "ping") == 0) {
      dst_addr = 0xf;
      src_addr = 0xf;
    }
  } else if (argc == 3) {
    dst_addr = atoi(argv[2]);
  } else if (argc == 4) {
    dst_addr = atoi(argv[2]);
    src_addr = atoi(argv[3]);
  } else {
    // ESP_LOGI(TAG, "cec_cmd_send() INVALID PARAMS");
    return -1;
  }
  return send_message(argv[1], src_addr, dst_addr);
}
