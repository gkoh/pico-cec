#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "prefs.h"

static const char *TAG = "prefs";

// static uint32_t _handle;
static nvs_handle_t _handle;
static bool _started;
static bool _readOnly;

static const char *nvs_errors[] = {"OTHER",         "NOT_INITIALIZED", "NOT_FOUND",
                                   "TYPE_MISMATCH", "READ_ONLY",       "NOT_ENOUGH_SPACE",
                                   "INVALID_NAME",  "INVALID_HANDLE",  "REMOVE_FAILED",
                                   "KEY_TOO_LONG",  "PAGE_FULL",       "INVALID_STATE",
                                   "INVALID_LENGTH"};
#define nvs_error(e) \
  (((e) > ESP_ERR_NVS_BASE) ? nvs_errors[(e) & ~(ESP_ERR_NVS_BASE)] : nvs_errors[0])

bool prefs_init(void) {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // NVS partition was truncated and needs to be erased
    // Retry nvs_flash_init
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);
  return (err == ESP_OK);
}

bool prefs_begin(const char *name, bool readOnly, const char *partition_label) {
  if (_started) {
    return false;
  }
  _readOnly = readOnly;
  esp_err_t err = ESP_OK;
  if (partition_label != NULL) {
    err = nvs_flash_init_partition(partition_label);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "nvs_flash_init_partition failed: %s", nvs_error(err));
      return false;
    }
    err = nvs_open_from_partition(partition_label, name, readOnly ? NVS_READONLY : NVS_READWRITE,
                                  &_handle);
  } else {
    err = nvs_open(name, readOnly ? NVS_READONLY : NVS_READWRITE, &_handle);
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nvs_open failed: %s", nvs_error(err));
    return false;
  }
  _started = true;
  return true;
}

void prefs_end(void) {
  if (!_started) {
    return;
  }
  nvs_close(_handle);
  _started = false;
}

size_t prefs_putBytes(const char *key, const void *value, size_t len) {
  if (!_started || !key || !value || !len || _readOnly) {
    return 0;
  }
  esp_err_t err = nvs_set_blob(_handle, key, value, len);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nvs_set_blob fail: %s %s", key, nvs_error(err));
    return 0;
  }
  err = nvs_commit(_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nvs_commit fail: %s %s", key, nvs_error(err));
    return 0;
  }
  return len;
}

static size_t prefs_getBytesLength(const char *key) {
  size_t len = 0;
  if (!_started || !key) {
    return 0;
  }
  esp_err_t err = nvs_get_blob(_handle, key, NULL, &len);
  if (err != ESP_OK) {
    ESP_LOGD(TAG, "nvs_get_blob length fail: '%s' %s", key, nvs_error(err));
    return 0;
  }
  return len;
}

size_t prefs_getBytes(const char *key, void *buf, size_t maxLen) {
  size_t len = prefs_getBytesLength(key);
  if (!len || !buf || !maxLen) {
    return len;
  }
  if (len > maxLen) {
    ESP_LOGE(TAG, "not enough space in buffer: %u < %u", maxLen, len);
    return 0;
  }
  esp_err_t err = nvs_get_blob(_handle, key, buf, &len);
  if (err != ESP_OK) {
    ESP_LOGD(TAG, "nvs_get_blob fail: '%s' %s", key, nvs_error(err));
    return 0;
  }
  return len;
}

bool prefs_clear(void) {
  if (!_started || _readOnly) {
    return false;
  }
  esp_err_t err = nvs_erase_all(_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nvs_erase_all fail: %s", nvs_error(err));
    return false;
  }
  err = nvs_commit(_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nvs_commit fail: %s", nvs_error(err));
    return false;
  }
  return true;
}
