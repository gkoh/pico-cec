#ifndef CEC_UTIL_H
#define CEC_UTIL_H

typedef uint32_t (*write_str_ptr_t)(const char *);

void *cec_frame_capture(void *ptr);
void cec_frame_dump(write_str_ptr_t);
void cec_frame_rxint(void);  // for diagnosing rx failure (force rx interrupt on)

#endif
