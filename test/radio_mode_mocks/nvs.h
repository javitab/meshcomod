#pragma once

#include <stdint.h>
#include <string>
#include <map>

using esp_err_t = int;
using nvs_handle_t = unsigned;
constexpr int ESP_OK = 0;
constexpr int ESP_ERR_NVS_NOT_FOUND = 1;
constexpr int ESP_FAIL = 2;
constexpr int NVS_READONLY = 0;
constexpr int NVS_READWRITE = 1;

inline std::map<std::string, uint8_t> mock_nvs;
inline bool mock_open_failure = false;
inline bool mock_read_failure = false;
inline bool mock_write_failure = false;
inline bool mock_commit_failure = false;
inline bool mock_namespace_exists = false;
inline uint8_t mock_pending_value = 0;

inline esp_err_t nvs_open(const char* ns, int mode, nvs_handle_t* handle) {
  if (mock_open_failure || std::string(ns) != "mc-radio") return ESP_FAIL;
  if (!mock_namespace_exists && mode == NVS_READONLY) return ESP_ERR_NVS_NOT_FOUND;
  mock_namespace_exists = true;
  *handle = 1;
  return ESP_OK;
}
inline esp_err_t nvs_get_u8(nvs_handle_t, const char* key, uint8_t* out) {
  if (mock_read_failure) return ESP_FAIL;
  auto found = mock_nvs.find(key);
  if (found == mock_nvs.end()) return ESP_ERR_NVS_NOT_FOUND;
  *out = found->second;
  return ESP_OK;
}
inline esp_err_t nvs_set_u8(nvs_handle_t, const char* key, uint8_t value) {
  if (mock_write_failure || std::string(key) != "mode") return ESP_FAIL;
  mock_pending_value = value;
  return ESP_OK;
}
inline esp_err_t nvs_commit(nvs_handle_t) {
  if (mock_commit_failure) return ESP_FAIL;
  mock_nvs["mode"] = mock_pending_value;
  return ESP_OK;
}
inline void nvs_close(nvs_handle_t) {}
