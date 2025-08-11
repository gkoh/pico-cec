#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "cec-hal.h"
DECLARE_TAG()

#include "cec-id.h"

#define PROTO_STACK_SIZE (4096)
static StackType_t stackProto[PROTO_STACK_SIZE];
static StaticTask_t xProtoTCB;
static TaskHandle_t xProtoTask;

#define PROTO_QUEUE_LENGTH (8)
static StaticQueue_t xStaticProtoQueue;
static QueueHandle_t proto_queue = NULL;
static uint8_t storageProtoQueue[PROTO_QUEUE_LENGTH * sizeof(uint8_t)];

static void cec_id_task(void *param) {
  QueueHandle_t *q = (QueueHandle_t *)param;
  uint8_t id;

  while (true) {
    if (xQueueReceive(*q, &id, portMAX_DELAY)) {
      // ESP_LOGI(TAG, "id = %u", id);
      switch (id) {
        case CEC_ID_IMAGE_VIEW_ON:
          ESP_LOGI(TAG, "CEC_ID_IMAGE_VIEW_ON");
          break;
        case CEC_ID_TEXT_VIEW_ON:
          ESP_LOGI(TAG, "CEC_ID_TEXT_VIEW_ON");
          break;
        case CEC_ID_STANDBY:
          ESP_LOGI(TAG, "CEC_ID_STANDBY");
          break;
        case CEC_ID_SYSTEM_AUDIO_MODE_REQUEST:
          ESP_LOGI(TAG, "CEC_ID_SYSTEM_AUDIO_MODE_REQUEST");
          break;
        case CEC_ID_GIVE_AUDIO_STATUS:
          ESP_LOGI(TAG, "CEC_ID_GIVE_AUDIO_STATUS");
          break;
        case CEC_ID_SET_SYSTEM_AUDIO_MODE:
          ESP_LOGI(TAG, "CEC_ID_SET_SYSTEM_AUDIO_MODE");
          break;
        case CEC_ID_GIVE_SYSTEM_AUDIO_MODE_STATUS:
          ESP_LOGI(TAG, "CEC_ID_GIVE_SYSTEM_AUDIO_MODE_STATUS");
          break;
        case CEC_ID_SYSTEM_AUDIO_MODE_STATUS:
          ESP_LOGI(TAG, "CEC_ID_SYSTEM_AUDIO_MODE_STATUS");
          break;
        case CEC_ID_ROUTING_CHANGE:
          ESP_LOGI(TAG, "CEC_ID_ROUTING_CHANGE");
          break;
        case CEC_ID_ACTIVE_SOURCE:
          ESP_LOGI(TAG, "CEC_ID_ACTIVE_SOURCE");
          break;
        case CEC_ID_REPORT_PHYSICAL_ADDRESS:
          ESP_LOGI(TAG, "CEC_ID_REPORT_PHYSICAL_ADDRESS");
          break;
        case CEC_ID_REQUEST_ACTIVE_SOURCE:
          ESP_LOGI(TAG, "CEC_ID_REQUEST_ACTIVE_SOURCE");
          break;
        case CEC_ID_SET_STREAM_PATH:
          ESP_LOGI(TAG, "CEC_ID_SET_STREAM_PATH");
          break;
        case CEC_ID_DEVICE_VENDOR_ID:
          ESP_LOGD(TAG, "CEC_ID_DEVICE_VENDOR_ID");
          break;
        case CEC_ID_GIVE_DEVICE_VENDOR_ID:
          ESP_LOGI(TAG, "CEC_ID_GIVE_DEVICE_VENDOR_ID");
          break;
        case CEC_ID_MENU_STATUS:
          ESP_LOGI(TAG, "CEC_ID_MENU_STATUS");
          break;
        case CEC_ID_GIVE_DEVICE_POWER_STATUS:
          ESP_LOGI(TAG, "CEC_ID_GIVE_DEVICE_POWER_STATUS");
          break;
        case CEC_ID_REPORT_POWER_STATUS:
          ESP_LOGI(TAG, "CEC_ID_REPORT_POWER_STATUS");
          break;
        case CEC_ID_GET_MENU_LANGUAGE:
          ESP_LOGI(TAG, "CEC_ID_GET_MENU_LANGUAGE");
          break;
        case CEC_ID_INACTIVE_SOURCE:
          ESP_LOGI(TAG, "CEC_ID_INACTIVE_SOURCE");
          break;
        case CEC_ID_CEC_VERSION:
          ESP_LOGI(TAG, "CEC_ID_CEC_VERSION");
          break;
        case CEC_ID_GET_CEC_VERSION:
          ESP_LOGI(TAG, "CEC_ID_GET_CEC_VERSION");
          break;
        case CEC_ID_GIVE_OSD_NAME:
          ESP_LOGI(TAG, "CEC_ID_GIVE_OSD_NAME");
          break;
        case CEC_ID_SET_OSD_NAME:
          ESP_LOGI(TAG, "CEC_ID_SET_OSD_NAME");
          break;
        case CEC_ID_GIVE_PHYSICAL_ADDRESS:
          ESP_LOGD(TAG, "CEC_ID_GIVE_PHYSICAL_ADDRESS");
          break;
        case CEC_ID_USER_CONTROL_PRESSED:
          ESP_LOGI(TAG, "CEC_ID_USER_CONTROL_PRESSED");
          break;
        case CEC_ID_USER_CONTROL_RELEASED:
          ESP_LOGI(TAG, "CEC_ID_USER_CONTROL_RELEASED");
          break;
        case CEC_ID_ABORT:
          ESP_LOGI(TAG, "CEC_ID_ABORT");
          break;
        case CEC_ID_FEATURE_ABORT:
          ESP_LOGI(TAG, "CEC_ID_FEATURE_ABORT");
          break;
        case CEC_ID_VENDOR_COMMAND_WITH_ID:
          ESP_LOGI(TAG, "CEC_ID_VENDOR_COMMAND_WITH_ID");
          break;
        case CEC_ID_ECHO_REQUEST:
          ESP_LOGI(TAG, "CEC_ID_ECHO_REQUEST");
          break;
        case CEC_ID_ECHO_RESPOND:
          ESP_LOGI(TAG, "CEC_ID_ECHO_RESPOND");
          break;
        default:
          ESP_LOGI(TAG, "CEC_ default");
          break;
      }
    }
  }
}

void cec_id_event_log(uint8_t id) {
  if (proto_queue != NULL) {
    if (pdTRUE != xQueueSendToBack(proto_queue, &id, (TickType_t)0)) {
      ESP_LOGE(TAG, "protocol monitor queue full");
    }
  }
}

void cec_id_event_log_start(void) {
  proto_queue = xQueueCreateStatic(PROTO_QUEUE_LENGTH, sizeof(uint8_t), &storageProtoQueue[0],
                                   &xStaticProtoQueue);
  if (!proto_queue) {
    ESP_LOGE(TAG, "Creating protocol monitor queue failed");
    return;
  }
  xProtoTask = xTaskCreateStatic(cec_id_task, "proto", PROTO_STACK_SIZE, &proto_queue,
                                 configMAX_PRIORITIES - 8, &stackProto[0], &xProtoTCB);
  (void)xProtoTask;
}
