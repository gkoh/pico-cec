/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "tinyusb.h"

/**
 * CDC descriptor constants.
 */
#define USBD_STR_CDC (0x04)
#define USBD_CDC_EP_CMD (0x81)
#define USBD_CDC_CMD_MAX_SIZE (8)
#define USBD_CDC_EP_OUT (0x02)
#define USBD_CDC_EP_IN (0x82)
#define USBD_CDC_IN_OUT_MAX_SIZE (64)

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// HID Report Descriptor
//--------------------------------------------------------------------+

uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(HID_ITF_PROTOCOL_KEYBOARD)),
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(HID_ITF_PROTOCOL_MOUSE))};

// Invoked when received GET HID REPORT DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
  // We use only one interface and one HID report descriptor, so we can ignore parameter 'instance'
  (void)instance;
  return desc_hid_report;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+

enum { ITF_NUM_HID, ITF_NUM_CDC, ITF_NUM_CDC_DATA, ITF_NUM_TOTAL };

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN + TUD_CDC_DESC_LEN)

#define EPNUM_HID 0x83

uint8_t const desc_configuration[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1,
                          ITF_NUM_TOTAL,
                          0,
                          CONFIG_TOTAL_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP,
                          200),

    // Interface number, string index, protocol, report descriptor len, EP In address, size &
    // polling interval
    TUD_HID_DESCRIPTOR(ITF_NUM_HID,
                       5,
                       //    HID_ITF_PROTOCOL_NONE,
                       HID_ITF_PROTOCOL_KEYBOARD,
                       sizeof(desc_hid_report),
                       EPNUM_HID,
                       CFG_TUD_HID_EP_BUFSIZE,
                       10),

    // Interface number, string index, EP notification address and size, EP data address (out, in)
    // and size.
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC,
                       USBD_STR_CDC,
                       USBD_CDC_EP_CMD,
                       USBD_CDC_CMD_MAX_SIZE,
                       USBD_CDC_EP_OUT,
                       USBD_CDC_EP_IN,
                       USBD_CDC_IN_OUT_MAX_SIZE)};

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

#define CONFIG_DESC_HID_STRING "HID DESCRIPTOR"

// array of pointer to string descriptors
char const *string_desc_arr[] = {
    (const char[]){0x09, 0x04},               // 0: is supported language is English (0x0409)
    CONFIG_TINYUSB_DESC_MANUFACTURER_STRING,  // 1: Manufacturer
    CONFIG_TINYUSB_DESC_PRODUCT_STRING,       // 2: Product
    CONFIG_TINYUSB_DESC_SERIAL_STRING,        // 3: Serials, should use chip ID
    CONFIG_TINYUSB_DESC_CDC_STRING,           // 4: CDC Interface
    CONFIG_DESC_HID_STRING,                   // 5: HID Interface
};

const tinyusb_config_t tusb_cfg = {
    .device_descriptor = NULL,
    .string_descriptor = string_desc_arr,
    .string_descriptor_count = sizeof(string_desc_arr) / sizeof(string_desc_arr[0]),
    .external_phy = false,
    .configuration_descriptor = desc_configuration,
};
