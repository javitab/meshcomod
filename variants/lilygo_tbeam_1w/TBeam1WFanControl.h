#pragma once

#include <cmath>
#include <cstdint>

namespace tbeam1w {

inline float ntcTemperatureC(uint32_t millivolts) {
  // LilyGo: 10k NTC to 3.3V, 10k resistor to ground, beta 3950 at 25C.
  // Reject near-rail readings, including open/short circuits and ADC saturation.
  if (millivolts < 100 || millivolts > 3000) return NAN;
  const float resistance_ratio = 3300.0f / millivolts - 1.0f;
  return 1.0f / (std::log(resistance_ratio) / 3950.0f + 1.0f / 298.15f) - 273.15f;
}

class FanControl {
  bool _enabled = true;

public:
  bool update(float temperature_c) {
    if (!std::isfinite(temperature_c) || temperature_c >= 45.0f) {
      _enabled = true;
    } else if (temperature_c < 40.0f) {
      _enabled = false;
    }
    return _enabled;
  }
};

}
