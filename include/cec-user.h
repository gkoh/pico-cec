#ifndef CEC_USER_H
#define CEC_USER_H

#include <stdint.h>

/**
 * CEC user command codes.
 *
 * @note Incomplete, add as needed.
 */
typedef enum {
  CEC_USER_SELECT = 0x00,
  CEC_USER_UP = 0x01,
  CEC_USER_DOWN = 0x02,
  CEC_USER_LEFT = 0x03,
  CEC_USER_RIGHT = 0x04,
  CEC_USER_RIGHT_UP = 0x05,
  CEC_USER_RIGHT_DOWN = 0x06,
  CEC_USER_LEFT_UP = 0x07,
  CEC_USER_LEFT_DOWN = 0x08,
  CEC_USER_OPTIONS = 0x0a,
  CEC_USER_EXIT = 0x0d,
  CEC_USER_0 = 0x20,
  CEC_USER_1 = 0x21,
  CEC_USER_2 = 0x22,
  CEC_USER_3 = 0x23,
  CEC_USER_4 = 0x24,
  CEC_USER_5 = 0x25,
  CEC_USER_6 = 0x26,
  CEC_USER_7 = 0x27,
  CEC_USER_8 = 0x28,
  CEC_USER_9 = 0x29,
  CEC_USER_DISPLAY_INFO = 0x35,
  CEC_USER_VOLUME_UP = 0x41,
  CEC_USER_VOLUME_DOWN = 0x42,
  CEC_USER_PLAY = 0x44,
  CEC_USER_STOP = 0x45,
  CEC_USER_PAUSE = 0x46,
  CEC_USER_REWIND = 0x48,
  CEC_USER_FAST_FWD = 0x49,
  CEC_USER_SUB_PICTURE = 0x51,
  CEC_USER_F1_BLUE = 0x71,
  CEC_USER_F2_RED = 0x72,
  CEC_USER_F3_GREEN = 0x73,
  CEC_USER_F4_YELLOW = 0x74,
  CEC_USER_F5 = 0x75,
} cec_user_t;

/** Mapping from CEC user code to human readable string. */
extern const char *cec_user_control_name[UINT8_MAX];

#endif
