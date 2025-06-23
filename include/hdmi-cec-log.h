#ifndef HDMI_CEC_LOG_H
#define HDMI_CEC_LOG_H

#include <stdbool.h>

typedef struct cec_frame_t cec_frame_t;  // where possible avoid including a local .h in a header

void hdmi_cec_log_frame(cec_frame_t *frame, bool recv);

#endif  // HDMI_CEC_LOG_H
