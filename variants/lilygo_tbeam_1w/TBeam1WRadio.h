#pragma once

#include <CustomSX1262.h>

class TBeam1WRadio : public CustomSX1262 {
  int16_t _power_status = RADIOLIB_ERR_UNKNOWN;

public:
  explicit TBeam1WRadio(Module* module) : CustomSX1262(module) {}

  using CustomSX1262::startTransmit;

  int16_t setOutputPower(int8_t power) override {
    return setOutputPower(power, true);
  }

  int16_t setOutputPower(int8_t power, bool optimize) {
    _power_status = CustomSX1262::setOutputPower(power, optimize);
    if (_power_status == RADIOLIB_ERR_NONE) {
      // RadioLib resets the ramp to 200 us on every power change. Preserve its
      // optimized PA drive setting while meeting LilyGo's >800 us requirement.
      _power_status = setPaRampTime(RADIOLIB_SX126X_PA_RAMP_1700U);
    }
    if (_power_status != RADIOLIB_ERR_NONE) {
      Serial.printf("ERROR: T-Beam 1W PA configuration failed: %d\n", _power_status);
    }
    return _power_status;
  }

  int16_t startTransmit(const uint8_t* data, size_t len, uint8_t addr = 0) override {
    if (_power_status != RADIOLIB_ERR_NONE) {
      Serial.printf("ERROR: T-Beam 1W TX blocked: PA configuration failed (%d)\n", _power_status);
      return _power_status;
    }
    return CustomSX1262::startTransmit(data, len, addr);
  }
};
