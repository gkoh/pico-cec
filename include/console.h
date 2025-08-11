#ifndef CONSOLE_H
#define CONSOLE_H

void console_init(void);
void console_input(char c);

void console_output(const char *str);  // used internally by log_task via function pointer

#endif
