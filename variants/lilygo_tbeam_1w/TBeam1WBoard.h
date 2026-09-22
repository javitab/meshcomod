#pragma once

#include <Arduino.h>
#include <helpers/ESP32Board.h>
#include "variant.h"

// LilyGo T-Beam 1W with SX1262 + external PA (XY16P35 module)
//
// Control signals:
//   - LDO_EN (GPIO 40): HIGH powers the radio; keep on across TX/RX transitions
//   - TCXO_EN (DIO3):   voltage configured in platformio.ini
//   - CTL (GPIO 21):    HIGH=RX (LNA on), LOW=TX (LNA off)
//   - DIO2:             AUTO via SX126X_DIO2_AS_RF_SWITCH (TX path)
//
// Power notes:
//   - USB-C input: 3.9-6V; external battery: 7.4V (not charged by this board)
//   - Battery must support 2A+ discharge for high-power TX

class TBeam1WBoard : public ESP32Board {
private:
  bool radio_powered = false;
  portMUX_TYPE fan_mux = portMUX_INITIALIZER_UNLOCKED;
  bool fan_shutdown = false;
  float ntc_temperature = NAN;
  uint32_t ntc_sample_time = 0;

  static void fanTask(void* context);
  void monitorFan();
  bool applyFanState(bool enabled, float temperature);

public:
  void begin();
  void onBeforeTransmit() override;
  void onAfterTransmit() override;
  uint16_t getBattMilliVolts() override;
  const char* getManufacturerName() const override;
  void powerOff() override;

  bool isFanEnabled() const;
  float getNTCTemperature();
};
