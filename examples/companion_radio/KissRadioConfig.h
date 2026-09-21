#pragma once

#include "../kiss_modem/KissModem.h"

inline bool kissSx1262ConfigValid(const RadioConfig& config) {
  int8_t power = static_cast<int8_t>(config.tx_power);
  if (config.freq_hz < 150000000 || config.freq_hz > 960000000 ||
      config.sf < 5 || config.sf > 12 || config.cr < 5 || config.cr > 8 ||
      power < -9 || power > 22) return false;
  const uint32_t bandwidths[] = {7800, 10400, 15600, 20800, 31250, 41700,
                                62500, 125000, 250000, 500000};
  for (uint32_t bw : bandwidths) {
    if (config.bw_hz == bw) return true;
  }
  return false;
}
