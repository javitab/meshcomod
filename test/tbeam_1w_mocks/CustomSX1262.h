#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

constexpr int16_t RADIOLIB_ERR_NONE = 0;
constexpr int16_t RADIOLIB_ERR_UNKNOWN = -1;
constexpr uint8_t RADIOLIB_SX126X_PA_RAMP_1700U = 0x06;

struct Module {};

struct MockSerial {
  int errors = 0;
  template<typename... Args>
  void printf(const char*, Args...) { ++errors; }
};
inline MockSerial Serial;

class CustomSX1262 {
public:
  std::vector<char> calls;
  int16_t power_result = 0;
  int16_t ramp_result = 0;
  int16_t tx_result = 0;
  int8_t drive = 0;
  uint8_t ramp = 0;
  bool optimized = false;

  explicit CustomSX1262(Module*) {}
  virtual ~CustomSX1262() = default;

  virtual int16_t setOutputPower(int8_t power) {
    return setOutputPower(power, true);
  }

  int16_t setOutputPower(int8_t power, bool optimize) {
    calls.push_back('P');
    optimized = optimize;
    // Model RadioLib's PA optimization and its reset of the ramp on power writes.
    drive = optimize ? 22 : power;
    ramp = 0x04;
    return power_result;
  }

  int16_t setPaRampTime(uint8_t value) {
    calls.push_back('R');
    ramp = value;
    return ramp_result;
  }

  virtual int16_t startTransmit(const uint8_t*, size_t, uint8_t = 0) {
    calls.push_back('T');
    return tx_result;
  }
};
