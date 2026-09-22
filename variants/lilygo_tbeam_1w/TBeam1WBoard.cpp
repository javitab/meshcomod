#include "TBeam1WBoard.h"
#include "TBeam1WFanControl.h"
#include <driver/adc.h>
#include <esp_adc_cal.h>

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

  // Fail safe until the thermistor has a valid, cool reading.
  digitalWrite(FAN_CTRL_PIN, HIGH);
  pinMode(FAN_CTRL_PIN, OUTPUT);
  if (xTaskCreate(fanTask, "tbeam_fan", 4096, this, 1, nullptr) != pdPASS) {
    Serial.println("ERROR: T-Beam 1W fan monitor could not start; fan stays ON");
  }
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
  portENTER_CRITICAL(&fan_mux);
  fan_shutdown = true;
  ntc_temperature = NAN;
  digitalWrite(FAN_CTRL_PIN, LOW);
  portEXIT_CRITICAL(&fan_mux);

  ESP32Board::powerOff();
}

bool TBeam1WBoard::applyFanState(bool enabled, float temperature) {
  const uint32_t sample_time = millis();
  portENTER_CRITICAL(&fan_mux);
  const bool active = !fan_shutdown;
  if (active) {
    digitalWrite(FAN_CTRL_PIN, enabled ? HIGH : LOW);
    ntc_temperature = temperature;
    ntc_sample_time = sample_time;
  }
  portEXIT_CRITICAL(&fan_mux);
  return active;
}

void TBeam1WBoard::fanTask(void* context) {
  static_cast<TBeam1WBoard*>(context)->monitorFan();
  vTaskDelete(nullptr);
}

void TBeam1WBoard::monitorFan() {
  // GPIO14 is ADC2 channel 3 on ESP32-S3. Check errors explicitly: Wi-Fi can
  // contend for ADC2, and a failed conversion must never look like a cool NTC.
  static_assert(NTC_PIN == 14, "Update the fan ADC channel when changing the NTC pin");
  const esp_err_t setup = adc2_config_channel_atten(ADC2_CHANNEL_3, ADC_ATTEN_DB_11);
  if (setup != ESP_OK) {
    Serial.printf("ERROR: T-Beam 1W NTC ADC setup: %s; fan stays ON\n", esp_err_to_name(setup));
    return;
  }
  esp_adc_cal_characteristics_t calibration = {};
  const auto source = esp_adc_cal_characterize(ADC_UNIT_2, ADC_ATTEN_DB_11,
                                               ADC_WIDTH_BIT_12, 1100, &calibration);
  if (source == ESP_ADC_CAL_VAL_DEFAULT_VREF || source == ESP_ADC_CAL_VAL_NOT_SUPPORTED) {
    Serial.println("ERROR: T-Beam 1W NTC ADC calibration unavailable; fan stays ON");
    return;
  }

  tbeam1w::FanControl control;
  bool first = true;
  bool previous_valid = false;
  bool previous_enabled = true;
  for (;;) {
    uint32_t sum_mv = 0;
    esp_err_t error = ESP_OK;
    for (unsigned i = 0; i < 8; ++i) {
      int raw = 0;
      error = adc2_get_raw(ADC2_CHANNEL_3, ADC_WIDTH_BIT_12, &raw);
      if (error != ESP_OK) break;
      if (raw <= 0 || raw >= 4095) {
        error = ESP_ERR_INVALID_RESPONSE;
        break;
      }
      const uint32_t mv = esp_adc_cal_raw_to_voltage(raw, &calibration);
      if (!std::isfinite(tbeam1w::ntcTemperatureC(mv))) {
        error = ESP_ERR_INVALID_RESPONSE;
        break;
      }
      sum_mv += mv;
    }
    const float temperature = error == ESP_OK ? tbeam1w::ntcTemperatureC(sum_mv / 8) : NAN;
    const bool valid = std::isfinite(temperature);
    const bool enabled = control.update(temperature);
    if (!applyFanState(enabled, temperature)) return;
    if (first || valid != previous_valid || enabled != previous_enabled) {
      if (valid) {
        Serial.printf("[fan] NTC %.1f C; fan %s\n", temperature, enabled ? "ON" : "OFF");
      } else {
        Serial.printf("ERROR: T-Beam 1W NTC reading: %s; fan ON\n", esp_err_to_name(error));
      }
    }
    first = false;
    previous_valid = valid;
    previous_enabled = enabled;
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

bool TBeam1WBoard::isFanEnabled() const {
  return digitalRead(FAN_CTRL_PIN) == HIGH;
}

float TBeam1WBoard::getNTCTemperature() {
  portENTER_CRITICAL(&fan_mux);
  const float temperature = ntc_temperature;
  const uint32_t sample_time = ntc_sample_time;
  portEXIT_CRITICAL(&fan_mux);
  return millis() - sample_time < 3000 ? temperature : NAN;
}
