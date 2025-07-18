#include <stdbool.h>
#include <stdio.h>

bool prefs_init(void);
bool prefs_begin(const char *name, bool readOnly, const char *partition_label);
bool prefs_clear(void);
void prefs_end(void);

size_t prefs_putBytes(const char *key, const void *value, size_t len);
size_t prefs_getBytes(const char *key, void *buf, size_t maxLen);
