#ifndef HDMI_EDID_H
#define HDMI_EDID_H

#include <inttypes.h>
#include <stdint.h>

void edid_bus_init(void);
uint16_t edid_get_physical_address(void);

#endif
