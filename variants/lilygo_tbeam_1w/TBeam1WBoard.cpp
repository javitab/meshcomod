#include "TBeam1WBoard.h"

void TBeam1WBoard::begin() {
  ESP32Board::begin();

  // The 3:1 divider puts an 8.4V battery at 2.8V on the ADC input.
  analogSetPinAttenuation(BATTERY_PIN, ADC_11db);

  // SD and radio share SPI; keep an inserted card deselected during radio use.
  digitalWrite(SDCARD_CS, HIGH);
  pinMode(SDCARD_CS, OUTPUT);

  // CTRL powers the LNA, not the PA. Start in the safe idle state before LDO EN.
  digitalWrite(SX126X_RXEN, LOW);
  pinMode(SX126X_RXEN, OUTPUT);

  // Power on radio module (must be done before radio init)
  pinMode(SX126X_POWER_EN, OUTPUT);
  digitalWrite(SX126X_POWER_EN, HIGH);
  radio_powered = true;
  delay(10);  // Allow radio to power up

  // RF switch RXEN pin handled by RadioLib via setRfSwitchPins()

  // Initialize LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Initialize fan control (on by default - 1W PA can overheat)
  pinMode(FAN_CTRL_PIN, OUTPUT);
  digitalWrite(FAN_CTRL_PIN, HIGH);
}

void TBeam1WBoard::onBeforeTransmit() {
  // RF switching handled by RadioLib via SX126X_DIO2_AS_RF_SWITCH and setRfSwitchPins()
  digitalWrite(LED_PIN, HIGH);  // TX LED on
}

void TBeam1WBoard::onAfterTransmit() {
  digitalWrite(LED_PIN, LOW);   // TX LED off
}

uint16_t TBeam1WBoard::getBattMilliVolts() {
  analogReadResolution(12);
  uint32_t millivolts = 0;
  for (int i = 0; i < 8; i++) {
    millivolts += analogReadMilliVolts(BATTERY_PIN);
  }
  return static_cast<uint16_t>((millivolts * ADC_MULTIPLIER) / 8);
}

const char* TBeam1WBoard::getManufacturerName() const {
  return "LilyGo T-Beam 1W";
}

void TBeam1WBoard::powerOff() {
  // Turn off radio LNA (CTRL pin must be LOW when not receiving)
  digitalWrite(SX126X_RXEN, LOW);

  // Turn off radio power
  digitalWrite(SX126X_POWER_EN, LOW);
  radio_powered = false;

  // Turn off LED and fan
  digitalWrite(LED_PIN, LOW);
  digitalWrite(FAN_CTRL_PIN, LOW);

  ESP32Board::powerOff();
}

void TBeam1WBoard::setFanEnabled(bool enabled) {
  digitalWrite(FAN_CTRL_PIN, enabled ? HIGH : LOW);
}

bool TBeam1WBoard::isFanEnabled() const {
  return digitalRead(FAN_CTRL_PIN) == HIGH;
}
