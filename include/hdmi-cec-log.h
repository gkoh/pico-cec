#ifndef HDMI_LOG_H
#define HDMI_LOG_H

#include <stdbool.h>

typedef struct hdmi_frame_t hdmi_frame_t;

void log_cec_frame(hdmi_frame_t *frame, bool recv);

#endif
