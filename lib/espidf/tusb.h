#include <inttypes.h>
#include <stdbool.h>

#define KEYMAP_DEFAULT_KODI 1
//#define KEYMAP_DEFAULT_MISTER 1

typedef enum {
    HID_REPORT_TYPE_RESERVED = 0,
    HID_REPORT_TYPE_INPUT,
    HID_REPORT_TYPE_OUTPUT,
    HID_REPORT_TYPE_FEATURE
} hid_report_type_t;

#define KEYBOARD_LED_CAPSLOCK 0

// RHPort number used for device can be defined by board.mk, default to port 0
#ifndef BOARD_TUD_RHPORT
#define BOARD_TUD_RHPORT 0
#endif

bool tud_init(uint8_t rhport);
void tud_task(void);

bool tud_hid_keyboard_report(uint8_t report_id, uint8_t modifier, const uint8_t keycode[6]);

bool tud_suspended(void);
bool tud_hid_ready(void);

bool tud_remote_wakeup(void);
uint32_t tud_cdc_write_str(const char* str);

bool tud_cdc_connected(void);
uint32_t tud_cdc_available(void);
int32_t tud_cdc_read_char(void);
uint32_t tud_cdc_write_flush(void);

