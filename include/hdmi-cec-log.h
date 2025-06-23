#ifndef HDMI_LOG_H
#define HDMI_LOG_H

#include <stdbool.h>
#include "cec-frame.h"

void hdmi_cec_log_frame(cec_frame_t *frame, bool recv);

#endif
