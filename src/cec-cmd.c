#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "portable.h"
DECLARE_TAG()

#include "cec-frame.h"
#include "cec-task.h"

#include "usb-cdc.h"  // for cdc_printf

typedef struct cec_tagCmds {
  int address;
  const char *cmdstr;
  const unsigned char frame[4];
  int size;
} cec_cmds_t;

cec_cmds_t ceclist[] = {
    {0xFF, "ping", "\x00", 0},  // BEWARE: special case, data is unused
    {0x5F, "vendor", "\x8C", 1},
    {0x40, "on", "\x04", 1},
    {0x5F, "off", "\x36", 1},  // this only works if the actual source address sent out is 5, which
                               // cannot be garanteed from here
    {0x40, "on2", "\x44\x6D", 2},
    {0x40, "off2", "\x44\x6C", 2},   // this works for the LG TV
    {0x40, "getcec", "\x9F", 1},     // get CEC version
    {0x40, "getphys", "\x83", 1},    // get physical address
    {0x40, "getstate", "\x8F", 1},   // get power state
    {0x40, "getosd", "\x46", 1},     // get OSD name - LG TV appears to ignore this
    {0x40, "getvendor", "\x87", 1},  // get vendor ID
    {0x40, "input1", "\x82\x10\x00", 3},
    {0x40, "input2", "\x82\x20\x00", 3},
    {0x40, "input3", "\x82\x30\x00", 3},
    {0x40, "input4", "\x82\x40\x00", 3},
    {0x40, "standby", "\x44\x36", 2},
    {0x40, "toggle", "\x44\x6B", 2},  // turns tv on, but not off?
    {0x40, "up", "\x44\x41", 2},
    {0x40, "down", "\x44\x42", 2},
    {0x40, "mute", "\x44\x43", 2},     // not working
    {0x40, "unmute", "\x44\x44", 2},   // not working
    {0x40, "volup", "\x44\x41", 2},    // remote control pass through - pressed - volume up
    {0x40, "voldown", "\x44\x42", 2},  // remote control pass through - pressed - volume down
    {0x40, "mute2", "\x44\x42", 2},    // remote control pass through - pressed - mute function
    {0x40, "released", "\x45", 1}      // remote control pass through - user control released
};

extern bool force;

static bool TransmitFrame(int targetAddress, const unsigned char *buffer, int count) {
  uint8_t pld[count + 1];
  uint8_t logical_address = cec_get_logical_address();

  pld[0] = HEADER0(logical_address, targetAddress);
  memcpy(&pld[1], buffer, count);

  ESP_LOGI(TAG, "TransmitFrame(address %02x, cmd %02x, count %d)", pld[0], pld[1], count);

  force = true;
  cec_frame_send(count + 1, pld);
  force = false;
  return true;
}

int send_cmd(const char *cmdstr) {
  bool recognised = false;
  if (!cmdstr)
    return -1;  // If cmdstr is null, do nothing
  if (strcmp(cmdstr, "help") == 0) {
    cdc_printf("Usage:\n");
    for (size_t i = 0; i < (sizeof(ceclist) / sizeof(ceclist[0])); i++) {
      cdc_printf("\t%s\r\n", ceclist[i].cmdstr);
    }
    recognised = true;
  }
  for (size_t i = 0; i < (sizeof(ceclist) / sizeof(ceclist[0])); i++) {
    if (strcmp(ceclist[i].cmdstr, cmdstr) == 0) {
      cdc_printf("Sending command '%s'\n", cmdstr);
      TransmitFrame(ceclist[i].address, ceclist[i].frame, ceclist[i].size);
      recognised = true;
    }
  }
  if (!recognised) {
    cdc_printf("command '%s' unknown\n", cmdstr);
    return -1;
  }
  return 0;
}
