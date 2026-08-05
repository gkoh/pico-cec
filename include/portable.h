#ifndef _PORTABLE_H_
#define _PORTABLE_H_

#define DECLARE_TAG()

//
// Duplicate includes are commented out for compilation efficiency
//

// for blink.c
#include "bsp/board.h"
#include "pico/stdlib.h"

// for cec-config.c
#include "class/hid/hid.h"
#include "tusb.h"

// for cec-frame.c
// #include "pico/stdlib.h"

// for debug.c
#include "hardware/timer.h"
// #include "pico/stdlib.h"

// for freertos_hook.c
#include "common/tusb_common.h"

// for cec-task.c
// #include "class/hid/hid.h"
// #include "pico/stdlib.h"
// #include "tusb.h"

// for ddc.c
#include "hardware/i2c.h"
// #include "pico/stdlib.h"

// for main.c
// #include "bsp/board.h"
// #include "hardware/timer.h"
// #include "pico/stdlib.h"

// for nvs.c
#include <hardware/flash.h>
#include <hardware/sync.h>
//// #include "crc/crc32.h"

// for usb-cdc.c
#include <hardware/watchdog.h>
#include <pico/bootrom.h>
// #include <tusb.h>

// for usb_descriptors.c
// #include "tusb.h"

// for usb_hid.c
// #include "bsp/board.h"
// #include "pico/stdlib.h"
// #include "tusb.h"

// for ws2812.c
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "pico/sem.h"
// #include "pico/stdlib.h"

#endif  // _PORTABLE_H_
