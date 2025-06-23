#include <stdio.h>
#include <string.h>

#include "cec-config.h"
#include "cec-frame.h"
#include "cec-log.h"
#include "hdmi-cec-id.h"
#include "hdmi-cec-log.h"
#include "hdmi-cec.h"
#include "usb-cdc.h"

const char *cec_message[] = {
    [CEC_ID_FEATURE_ABORT] = "Feature Abort",
    [CEC_ID_IMAGE_VIEW_ON] = "Image View On",
    [CEC_ID_TEXT_VIEW_ON] = "Text View On",
    [CEC_ID_STANDBY] = "Standby",
    [CEC_ID_USER_CONTROL_PRESSED] = "User Control Pressed",
    [CEC_ID_USER_CONTROL_RELEASED] = "User Control Released",
    [CEC_ID_GIVE_OSD_NAME] = "Give OSD Name",
    [CEC_ID_SET_OSD_NAME] = "Set OSD Name",
    [CEC_ID_SYSTEM_AUDIO_MODE_REQUEST] = "System Audio Mode Request",
    [CEC_ID_GIVE_AUDIO_STATUS] = "Give Audio Status",
    [CEC_ID_SET_SYSTEM_AUDIO_MODE] = "Set System Audio Mode",
    [CEC_ID_GIVE_SYSTEM_AUDIO_MODE_STATUS] = "Give System Audio Mode",
    [CEC_ID_SYSTEM_AUDIO_MODE_STATUS] = "System Audio Mode Status",
    [CEC_ID_REPORT_AUDIO_STATUS] = "Report Audio Status",
    [CEC_ID_ROUTING_CHANGE] = "Routing Change",
    [CEC_ID_ACTIVE_SOURCE] = "Active Source",
    [CEC_ID_GIVE_PHYSICAL_ADDRESS] = "Give Physical Address",
    [CEC_ID_REPORT_PHYSICAL_ADDRESS] = "Report Physical Address",
    [CEC_ID_REQUEST_ACTIVE_SOURCE] = "Request Active Source",
    [CEC_ID_SET_STREAM_PATH] = "Set Stream Path",
    [CEC_ID_DEVICE_VENDOR_ID] = "Device Vendor ID",
    [CEC_ID_GIVE_DEVICE_VENDOR_ID] = "Give Device Vendor ID",
    [CEC_ID_MENU_STATUS] = "Menu Status",
    [CEC_ID_GIVE_DEVICE_POWER_STATUS] = "Give Device Power Status",
    [CEC_ID_REPORT_POWER_STATUS] = "Report Power Status",
    [CEC_ID_GET_MENU_LANGUAGE] = "Get Menu Language",
    [CEC_ID_INACTIVE_SOURCE] = "Inactive Source",
    [CEC_ID_CEC_VERSION] = "CEC Version",
    [CEC_ID_GET_CEC_VERSION] = "Get CEC Version",
    [CEC_ID_VENDOR_COMMAND_WITH_ID] = "Vendor Command With ID",
    [CEC_ID_REQUEST_ARC_INITIATION] = "Request ARC Initiation",
    [CEC_ID_ABORT] = "Abort",
};

const char *cec_feature_abort_reason[] = {
    [CEC_ABORT_UNRECOGNIZED] = "Unrecognized opcode",
    [CEC_ABORT_INCORRECT_MODE] = "Not in correct mode to respond",
    [CEC_ABORT_NO_SOURCE] = "Cannot provide source",
    [CEC_ABORT_INVALID] = "Invalid operand",
    [CEC_ABORT_REFUSED] = "Refused",
    [CEC_ABORT_UNDETERMINED] = "Undetermined",
};

/**
 * Log a timestamped formatted message.
 */
__attribute__((format(printf, 5, 6))) static void log_printf(uint8_t initiator,
                                                             uint8_t destination,
                                                             bool send,
                                                             bool ack,
                                                             const char *fmt,
                                                             ...) {
  char prefix[64];
  char buffer[64];
  char *arrow = "??";

  if (send) {
    if (ack) {
      arrow = "->";
    } else {
      arrow = "~>";
    }
  } else {
    if (ack) {
      arrow = "<-";
    } else {
      arrow = "<~";
    }
  }

  va_list ap;
  va_start(ap, fmt);
  snprintf(prefix, sizeof(prefix), "[%10llu] %02x %s %02x", cec_get_uptime_ms(),
           send ? initiator : destination, arrow, send ? destination : initiator);
  vsnprintf(buffer, sizeof(buffer), fmt, ap);
  cec_log_submitf("%s: %s"_CDC_BR, prefix, buffer);
  va_end(ap);
}

/**
 * Log a CEC frame.
 *
 * CEC frame logging function, which includes minor protocol decoding for debug
 * purposes.
 */
void hdmi_cec_log_frame(cec_frame_t *frame, bool recv) {
  cec_message_t *msg = frame->message;
  uint8_t initiator = (msg->data[0] & 0xf0) >> 4;
  uint8_t destination = msg->data[0] & 0x0f;

  if (msg->len > 1) {
    uint8_t cmd = msg->data[1];
    switch (cmd) {
      case CEC_ID_FEATURE_ABORT:
        log_printf(initiator, destination, recv, frame->ack, "[%s][%x][%s]", cec_message[cmd],
                   msg->data[2], cec_feature_abort_reason[msg->data[3]]);
        break;
      case CEC_ID_STANDBY:
        log_printf(initiator, destination, recv, frame->ack, "[%s][%s]", cec_message[cmd],
                   "Display OFF");
        break;
      case CEC_ID_ROUTING_CHANGE:
        log_printf(initiator, destination, recv, frame->ack, "[%s][%02x%02x -> %02x%02x]",
                   cec_message[cmd], msg->data[2], msg->data[3], msg->data[4], msg->data[5]);
        break;
      case CEC_ID_ACTIVE_SOURCE:
        log_printf(initiator, destination, recv, frame->ack, "[%s][%02x%02x Display ON]",
                   cec_message[cmd], msg->data[2], msg->data[3]);
        break;
      case CEC_ID_REPORT_PHYSICAL_ADDRESS:
        log_printf(initiator, destination, recv, frame->ack, "[%s] %02x%02x", cec_message[cmd],
                   msg->data[2], msg->data[3]);
        break;
      case CEC_ID_USER_CONTROL_PRESSED: {
        uint8_t key = msg->data[2];
        const char *name = cec_user_control_name[key];
        if (name != NULL) {
          log_printf(initiator, destination, recv, frame->ack, "[%s][%s]", cec_message[cmd], name);
        } else {
          log_printf(initiator, destination, recv, frame->ack, "[%s] Unknown command: 0x%02x",
                     cec_message[cmd], key);
        }
      } break;
      case CEC_ID_VENDOR_COMMAND_WITH_ID:
        log_printf(initiator, destination, recv, frame->ack, "[%s]", cec_message[cmd]);
        for (int i = 0; i < msg->len; i++) {
          cec_log_submitf(" %02x"_CDC_BR, msg->data[i]);
        }
        break;
      case CEC_ID_REPORT_POWER_STATUS:
        const char *status = "unknown";
        switch (msg->data[2]) {
          case 0x00:
            status = "On";
            break;
          case 0x01:
            status = "Standby";
            break;
          case 0x02:
            status = "In transition Standby to On";
            break;
          case 0x03:
            status = "In transition On to Standby";
            break;
        }
        log_printf(initiator, destination, recv, frame->ack, "[%s][%s]", cec_message[cmd], status);
        break;
      default: {
        const char *message = cec_message[cmd];
        if (strlen(message) > 0) {
          log_printf(initiator, destination, recv, frame->ack, "[%s]", cec_message[cmd]);
        } else {
          log_printf(initiator, destination, recv, frame->ack, "[%x] (undecoded)", cmd);
        }
      }
    }
  } else {
    log_printf(initiator, destination, recv, frame->ack, "[%s]", "Polling Message");
  }
}
