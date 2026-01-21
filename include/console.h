#ifndef CONSOLE_H
#define CONSOLE_H

void console_init(void);
void console_get(char c);

void console_put(const char *str);  // used internally by log_task via function pointer

#endif
