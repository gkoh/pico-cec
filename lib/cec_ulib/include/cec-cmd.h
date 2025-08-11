#ifndef CEC_CMD_H
#define CEC_CMD_H

#include <stdarg.h>
#include <stdbool.h>

typedef void (*printf_ptr_t)(const char *, ...);

int send_cmd(const char *cmdstr);
int cec_cmd_send(printf_ptr_t, int argc, const char **argv);
int send_message_to(const char *cmdstr, int dst_addr);

#endif
