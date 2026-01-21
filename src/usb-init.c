#include "config.h"

#if defined(USE_USB_CDC)  // || defined(USE_USB_HID)

#include "tinyusb.h"
#include "tusb.h"
#include "tusb_cdc_acm.h"

#include "portable.h"
DECLARE_TAG()

extern tinyusb_config_t tusb_cfg;

void usb_init(void) {
  ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
  tinyusb_config_cdcacm_t acm_cfg = {
      .usb_dev = TINYUSB_USBDEV_0,
      .cdc_port = TINYUSB_CDC_ACM_0,
      .rx_unread_buf_sz = 64,
      .callback_rx_wanted_char = NULL,
  };
  ESP_ERROR_CHECK(tusb_cdc_acm_init(&acm_cfg));
  ESP_LOGI(TAG, "USB Composite initialization DONE");
}

#endif  // USE_USB_CDC
