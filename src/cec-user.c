#include <stddef.h>

#include "cec-user.h"

/**
 * Official table of CEC User Control codes to human-readable names.
 *
 * From "High-Definition Multimedia Interface Specification Version 1.3, CEC
 * Table 23, User Control Codes, p. 62"
 *
 * @note Incomplete, add as required.
 */
const char *cec_user_control_name[UINT8_MAX] = {
    [CEC_USER_SELECT] = "Select",
    [CEC_USER_UP] = "Up",
    [CEC_USER_DOWN] = "Down",
    [CEC_USER_LEFT] = "Left",
    [CEC_USER_RIGHT] = "Right",
    [CEC_USER_RIGHT_UP] = "Right-Up",
    [CEC_USER_RIGHT_DOWN] = "Right-Down",
    [CEC_USER_LEFT_UP] = "Left-Up",
    [CEC_USER_LEFT_DOWN] = "Left-Down",
    [CEC_USER_OPTIONS] = "Options",
    [CEC_USER_EXIT] = "Exit",
    [CEC_USER_0] = "0",
    [CEC_USER_1] = "1",
    [CEC_USER_2] = "2",
    [CEC_USER_3] = "3",
    [CEC_USER_4] = "4",
    [CEC_USER_5] = "5",
    [CEC_USER_6] = "6",
    [CEC_USER_7] = "7",
    [CEC_USER_8] = "8",
    [CEC_USER_9] = "9",
    [CEC_USER_DISPLAY_INFO] = "Display Information",
    [CEC_USER_VOLUME_UP] = "Volume Up",
    [CEC_USER_VOLUME_DOWN] = "Volume Down",
    [CEC_USER_PLAY] = "Play",
    [CEC_USER_STOP] = "Stop",
    [CEC_USER_PAUSE] = "Pause",
    [CEC_USER_REWIND] = "Rewind",
    [CEC_USER_FAST_FWD] = "Fast Forward",
    [CEC_USER_SUB_PICTURE] = "Sub Picture",
    [CEC_USER_F1_BLUE] = "F1 (Blue)",
    [CEC_USER_F2_RED] = "F2 (Red)",
    [CEC_USER_F3_GREEN] = "F3 (Green)",
    [CEC_USER_F4_YELLOW] = "F4 (Yellow)",
    [CEC_USER_F5] = "F5",
    NULL,
};
