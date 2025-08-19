#ifndef CEC_CMD_H
#define CEC_CMD_H

#include <stdarg.h>
#include <stdbool.h>

typedef void (*printf_ptr_t)(const char *, ...);

int cec_cmd_send(const char *cmdstr, int dst_addr);  // called from main.c ie. cec_cmd_send("echo", echo_addr);
int cec_cmd_sendv(printf_ptr_t, int argc, const char **argv);  // called from console.c ie. cec_cmd_sendv(cdc_printf, argc, argv);

#endif
