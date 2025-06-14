#define KEYMAP_DEFAULT_KODI 1
//#define KEYMAP_DEFAULT_MISTER 1


//// #error "Unknown default keymap."
//#pragma message "Unknown default keymap."
//  config->keymap_type = CEC_CONFIG_KEYMAP_CUSTOM;



typedef enum {
    HID_REPORT_TYPE_RESERVED = 0,
    HID_REPORT_TYPE_INPUT,
    HID_REPORT_TYPE_OUTPUT,
    HID_REPORT_TYPE_FEATURE
} hid_report_type_t;

// RHPort number used for device can be defined by board.mk, default to port 0
#ifndef BOARD_TUD_RHPORT
#define BOARD_TUD_RHPORT 0
#endif

void tud_init(int);
void tud_task();

void tud_hid_keyboard_report(int a, int b, void* c);
int tud_suspended();
int tud_hid_ready();

void tud_remote_wakeup();
void tud_cdc_write_str(const char* str);

int tud_cdc_connected();
int tud_cdc_available();
uint8_t tud_cdc_read_char();
void tud_cdc_write_flush();
  
  
#define KEYBOARD_LED_CAPSLOCK 0


