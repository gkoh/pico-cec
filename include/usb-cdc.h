#ifndef USB_CDC_H
#define USB_CDC_H

#define _CDC_BR "\r\n"

/** Print formatted string to USB-CDC output. */
__attribute__((format(printf, 1, 2))) void cdc_printf(const char *fmt, ...);

/** Line print formatted string to USB-CDC output. */
__attribute__((format(printf, 1, 2))) void cdc_printfln(const char *fmt, ...);

void cdc_log(const char *str);
void cdc_task(void *param);

#endif
