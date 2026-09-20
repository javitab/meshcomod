#ifdef COMPANION_KISS_TCP
#include "RadioModeStore.h"
#include <nvs.h>

static const char* kNamespace = "mc-radio";
static const char* kModeKey = "mode";

const char* radioModeName(CompanionRadioMode mode) {
  return mode == CompanionRadioMode::KissTcp ? "kiss-tcp" : "companion";
}

bool radioModeLoad(CompanionRadioMode& mode, const char*& error) {
  error = nullptr;
  nvs_handle_t handle;
  esp_err_t result = nvs_open(kNamespace, NVS_READONLY, &handle);
  if (result == ESP_ERR_NVS_NOT_FOUND) {
    mode = CompanionRadioMode::Companion;
    return true;
  }
  if (result != ESP_OK) {
    error = "cannot open radio-mode NVS";
    return false;
  }
  uint8_t value = 0;
  result = nvs_get_u8(handle, kModeKey, &value);
  nvs_close(handle);
  if (result != ESP_OK && result != ESP_ERR_NVS_NOT_FOUND) {
    error = "cannot read radio mode";
    return false;
  }
  if (value > static_cast<uint8_t>(CompanionRadioMode::KissTcp)) {
    error = "invalid saved radio mode";
    return false;
  }
  mode = static_cast<CompanionRadioMode>(value);
  return true;
}

bool radioModeSave(CompanionRadioMode mode, const char*& error) {
  error = nullptr;
  if (mode != CompanionRadioMode::Companion && mode != CompanionRadioMode::KissTcp) {
    error = "invalid radio mode";
    return false;
  }
  nvs_handle_t handle;
  if (nvs_open(kNamespace, NVS_READWRITE, &handle) != ESP_OK) {
    error = "cannot open radio-mode NVS for writing";
    return false;
  }
  esp_err_t result = nvs_set_u8(handle, kModeKey, static_cast<uint8_t>(mode));
  if (result == ESP_OK) result = nvs_commit(handle);
  nvs_close(handle);
  if (result != ESP_OK) {
    error = "cannot persist radio mode";
    return false;
  }
  return true;
}
#endif
