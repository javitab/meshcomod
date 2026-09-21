#ifdef COMPANION_KISS_TCP
#include "KissTcpMode.h"
#include "KissRadioConfig.h"
#include "RadioModeStore.h"
#include "WiFiConfig.h"
#include "../kiss_modem/KissSocketStream.h"
#include <target.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <cmath>

static bool s_active = false;
static bool s_ready = false;
static bool s_reboot = false;
static RadioModeSwitch s_button_switch;
static bool s_button_error = false;
static uint32_t s_button_error_since = 0;
static const char* s_fault = nullptr;
static DisplayDriver* s_display = nullptr;
static mesh::LocalIdentity s_identity;
static KissSocketStream s_stream;
static KissModem* s_modem = nullptr;
static WiFiServer s_server(KISS_TCP_PORT);
static WiFiClient s_client;
static bool s_has_client = false;
static bool s_listening = false;
static IPAddress s_address;
static RadioConfig s_config;

bool kissTcpIsActive() { return s_active; }

bool companionModeBegin() {
  CompanionRadioMode mode;
  if (!radioModeLoad(mode, s_fault)) {
    Serial.printf("[mode] ERROR: %s; KISS recovery console only\n", s_fault);
    s_active = true;
  } else {
    s_active = mode == CompanionRadioMode::KissTcp;
    Serial.printf("[mode] %s\n", radioModeName(mode));
  }
  return s_active;
}

static bool wifiUsable() {
  return WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0);
}

bool companionModeCanUseKiss() {
  return wifiConfigHasRuntime() && wifiConfigGetRadioEnabled() && wifiUsable();
}

bool companionModeSelect(CompanionRadioMode mode, const char*& error) {
  return radioModeSelect(mode, companionModeCanUseKiss(), error);
}

void companionModeCommand(const char* command, char* reply, size_t size) {
  while (*command == ' ' || *command == '\t') ++command;
  size_t len = strlen(command);
  while (len && (command[len - 1] == ' ' || command[len - 1] == '\t')) --len;
  if (len == 4 && strncasecmp(command, "mode", len) == 0) {
    CompanionRadioMode saved;
    const char* error;
    if (!radioModeLoad(saved, error)) {
      snprintf(reply, size, "error: %s", error);
    } else {
      snprintf(reply, size, "active=%s next_boot=%s; KISS TCP port=%u",
               s_active ? "kiss-tcp" : "companion", radioModeName(saved), KISS_TCP_PORT);
    }
    return;
  }

  CompanionRadioMode mode;
  if (len == 14 && strncasecmp(command, "mode companion", len) == 0) {
    mode = CompanionRadioMode::Companion;
  } else if (len == 13 && strncasecmp(command, "mode kiss-tcp", len) == 0) {
    mode = CompanionRadioMode::KissTcp;
  } else {
    snprintf(reply, size, "error: use mode, mode companion, or mode kiss-tcp");
    return;
  }
  const char* error;
  if (!companionModeSelect(mode, error)) {
    snprintf(reply, size, "error: %s", error);
    return;
  }
  snprintf(reply, size, "saved %s; send reboot to apply (identity/settings retained)", radioModeName(mode));
}

static void fault(const char* message) {
  s_fault = message;
  s_ready = false;
  s_stream.attach(-1);
  Serial.printf("[kiss] ERROR: %s; use USB 'mode companion' then 'reboot'\n", message);
}

static int16_t applyRadio(const RadioConfig& config) {
  radio_driver.onSendFinished();
  int16_t result = radio_driver.setParamsChecked(config.freq_hz / 1000000.0f,
                                                config.bw_hz / 1000.0f, config.sf, config.cr);
  if (result == RADIOLIB_ERR_NONE) {
    result = radio.setOutputPower(static_cast<int8_t>(config.tx_power));
  }
  return result;
}

static bool configureRadio(const RadioConfig& config) {
  if (!kissSx1262ConfigValid(config)) return false;
  int16_t result = applyRadio(config);
  if (result != RADIOLIB_ERR_NONE) {
    Serial.printf("[kiss] radio configuration failed (%d); restoring previous settings\n", result);
    if (applyRadio(s_config) != RADIOLIB_ERR_NONE) {
      fault("radio configuration and rollback failed");
    }
    return false;
  }
  s_config = config;
  return true;
}

static bool returnToCompanion() {
  const char* error;
  if (!radioModeSave(CompanionRadioMode::Companion, error)) {
    Serial.printf("[kiss] ERROR: %s\n", error);
    return false;
  }
  Serial.println("[kiss] returning to companion; rebooting");
  s_reboot = true;
  return true;
}

void kissTcpBegin(DataStore& store, const NodePrefs& defaults, mesh::RNG& rng, DisplayDriver* display) {
  s_display = display;
  board.setInhibitSleep(true);
  pinMode(PIN_USER_BTN, INPUT_PULLUP);
  Serial.println("[kiss] USB commands: mode, mode companion, reboot, help");
  if (s_fault) return;
  // Never format storage, create an identity, or migrate companion files in KISS mode.
  if (!SPIFFS.begin(false)) {
    fault("cannot mount companion storage without formatting");
    return;
  }
  if (!store.loadMainIdentity(s_identity)) {
    fault("cannot load existing companion identity");
    return;
  }
  NodePrefs prefs = defaults;
  double lat = 0, lon = 0;
  store.loadPrefs(prefs, lat, lon, false);
  if (!std::isfinite(prefs.freq) || !std::isfinite(prefs.bw) ||
      prefs.freq < 150 || prefs.freq > 960 || prefs.bw < 7 || prefs.bw > 500) {
    fault("invalid saved radio frequency/bandwidth");
    return;
  }
  s_config = {static_cast<uint32_t>(lroundf(prefs.freq * 1000000.0f)),
              static_cast<uint32_t>(lroundf(prefs.bw * 1000.0f)),
              prefs.sf, prefs.cr, static_cast<uint8_t>(prefs.tx_power_dbm)};
  radio_driver.begin();
  radio_driver.resetStats();
  if (!kissSx1262ConfigValid(s_config) || applyRadio(s_config) != RADIOLIB_ERR_NONE ||
      !radio_driver.setRxBoostedGainMode(prefs.rx_boosted_gain != 0)) {
    fault("cannot apply saved radio settings");
    return;
  }
  sensors.begin();
  s_modem = new KissModem(s_stream, s_identity, rng, radio_driver, board, sensors);
  s_modem->setInitialRadioConfig(s_config);
  s_modem->setConfigureRadioCallback(configureRadio);
  s_modem->setExitCallback(returnToCompanion);
  s_modem->setGetCurrentRssiCallback([]() { return radio_driver.getCurrentRSSI(); });
  s_modem->setGetStatsCallback([](uint32_t* rx, uint32_t* tx, uint32_t* errors) {
    *rx = radio_driver.getPacketsRecv();
    *tx = radio_driver.getPacketsSent();
    *errors = radio_driver.getPacketsRecvErrors();
  });
  s_modem->begin();
  wifiConfigBegin();
  if (!wifiConfigHasRuntime() || !wifiConfigGetRadioEnabled()) {
    fault("KISS TCP needs saved Wi-Fi credentials and Wi-Fi enabled");
    return;
  }
  WiFi.persistent(false);
  if (!WiFi.mode(WIFI_STA)) {
    fault("cannot start Wi-Fi");
    return;
  }
  WiFi.setAutoReconnect(false);
  s_ready = true;
}

static void closeClient() {
  if (s_modem) s_modem->resetSession();
  s_stream.attach(-1);
  s_client.stop();
  s_has_client = false;
}

static void pollConsole() {
  static char line[64];
  static size_t len = 0;
  static bool overflow = false;
  for (unsigned i = 0; i < sizeof(line) && Serial.available(); ++i) {
    int ch = Serial.read();
    if (ch == '\r' || ch == '\n') {
      if (overflow) {
        Serial.println("error: command too long");
      } else if (len) {
        line[len] = '\0';
        if (strcasecmp(line, "reboot") == 0) {
          Serial.println("rebooting...");
          s_reboot = true;
        } else if (strcasecmp(line, "help") == 0) {
          Serial.println("mode | mode companion | reboot\nHold user button 3s to return to companion.");
        } else {
          char reply[160];
          companionModeCommand(line, reply, sizeof(reply));
          Serial.println(reply);
        }
      }
      len = 0;
      overflow = false;
    } else if (ch >= 0 && !overflow) {
      if (len + 1 < sizeof(line)) line[len++] = static_cast<char>(ch);
      else overflow = true;
    }
  }
}

static void pollRecoveryButton() {
  static bool held = false;
  static uint32_t since = 0;
  if (digitalRead(PIN_USER_BTN) != LOW) {
    held = false;
    if (s_button_switch.readyToReboot(false)) s_reboot = true;
  } else if (!held) {
    held = true;
    since = millis();
  } else if (!s_button_switch.pending() && millis() - since >= 3000) {
    since = millis();
    const char* error;
    if (s_button_switch.request(CompanionRadioMode::Companion, false, error)) {
      s_button_error = false;
      Serial.println("[kiss] companion saved; release button to reboot");
    } else {
      Serial.printf("[kiss] ERROR: %s\n", error);
      s_button_error = true;
      s_button_error_since = millis();
    }
  }
}

static void pollNetwork() {
  static uint32_t last_retry = millis() - 10000;
  if (!wifiUsable()) {
    if (s_listening) {
      closeClient();
      s_server.stop();
      s_listening = false;
      Serial.println("[kiss] Wi-Fi lost; TCP session cleared");
    }
    if (millis() - last_retry >= 10000) {
      last_retry = millis();
      char ssid[WIFI_CONFIG_SSID_MAX], pwd[WIFI_CONFIG_PWD_MAX];
      wifiConfigGetSsid(ssid, sizeof(ssid));
      wifiConfigGetPwd(pwd, sizeof(pwd));
      Serial.println("[kiss] connecting Wi-Fi");
      WiFi.disconnect(false, false);
      WiFi.begin(ssid, pwd[0] ? pwd : nullptr);
    }
    return;
  }
  if (s_listening && s_address != WiFi.localIP()) {
    closeClient();
    s_server.stop();
    s_listening = false;
  }
  if (!s_listening) {
    s_server.begin();
    s_listening = static_cast<bool>(s_server);
    if (!s_listening) {
      fault("cannot listen on KISS TCP port");
      return;
    }
    s_address = WiFi.localIP();
    Serial.printf("[kiss] listening on %s:%u (one client)\n", s_address.toString().c_str(), KISS_TCP_PORT);
  }
  if (s_has_client && (!s_client.connected() || s_stream.closed())) {
    Serial.printf("[kiss] client disconnected (socket error=%d)\n", s_stream.error());
    closeClient();
  }
  if (s_server.hasClient()) {
    WiFiClient incoming = s_server.accept();
    if (s_client.connected()) {
      incoming.stop();
      Serial.println("[kiss] rejected second client");
    } else if (incoming) {
      closeClient();
      s_client = incoming;
      s_has_client = true;
      s_client.setNoDelay(true);
      s_stream.attach(s_client.fd());
      Serial.println("[kiss] client connected");
    }
  }
}

static void updateDisplay() {
  static uint32_t last_update = 0;
  if (!s_display || millis() - last_update < 1000) return;
  last_update = millis();
  s_display->startFrame();
  s_display->setTextSize(1);
  s_display->drawTextLeftAlign(0, 0, "KISS TNC: ON");
  char line[32];
  const unsigned battery_mv = board.getBattMilliVolts();
  snprintf(line, sizeof(line), "%u.%02uV", battery_mv / 1000, (battery_mv % 1000) / 10);
  s_display->drawTextRightAlign(s_display->width() - 1, 0, line);
  if (s_button_error && millis() - s_button_error_since < 3000) {
    s_display->drawTextLeftAlign(0, 12, "Mode save failed");
  } else if (s_fault) {
    s_display->drawTextLeftAlign(0, 12, "ERROR: see USB log");
  } else {
    snprintf(line, sizeof(line), "%s:%u", WiFi.localIP().toString().c_str(), KISS_TCP_PORT);
    s_display->drawTextLeftAlign(0, 12, wifiUsable() ? line : "Wi-Fi connecting...");
  }
  s_display->drawTextLeftAlign(0, 24, s_client.connected() ? "Host connected" : "No host");
  snprintf(line, sizeof(line), "RX %lu TX %lu", (unsigned long)radio_driver.getPacketsRecv(),
           (unsigned long)radio_driver.getPacketsSent());
  s_display->drawTextLeftAlign(0, 36, line);
  s_display->drawTextLeftAlign(0, 50, s_button_switch.pending() ? "Release to reboot" : "OFF: hold 3s+release");
  s_display->endFrame();
}

void kissTcpLoop() {
  pollConsole();
  pollRecoveryButton();
  if (s_reboot) {
    closeClient();
    radio_driver.onSendFinished();
    delay(100);
    board.reboot();
    return;
  }
  if (s_ready) {
    pollNetwork();
    if (s_ready && s_has_client) {
      s_stream.beginPoll();
      s_modem->loop();
      static uint32_t stalled_since = 0;
      static uint32_t last_write = 0;
      if (!s_modem->isHostOutputBackedUp() || last_write != s_stream.lastWriteMillis()) {
        stalled_since = millis();
        last_write = s_stream.lastWriteMillis();
      } else if (millis() - stalled_since >= 30000) {
        Serial.println("[kiss] slow host disconnected after 30s without write progress");
        closeClient();
      }
    }
    if (s_ready && !s_reboot && !s_modem->isActuallyTransmitting()) {
      static uint32_t last_agc = 0;
      if (!s_modem->isTxBusy() && millis() - last_agc >= 30000) {
        radio_driver.resetAGC();
        last_agc = millis();
      }
      if (!s_modem->isHostOutputBackedUp()) {
        uint8_t packet[256];
        int len = radio_driver.recvRaw(packet, sizeof(packet));
        if (len > 0 && s_client.connected()) {
          s_modem->onPacketReceived(static_cast<int8_t>(radio_driver.getLastSNR() * 4),
                                   static_cast<int8_t>(radio_driver.getLastRSSI()), packet, len);
        }
      }
    }
    static uint32_t last_noise = 0;
    if (millis() - last_noise >= 2000) {
      radio_driver.triggerNoiseFloorCalibrate(0);
      last_noise = millis();
    }
    radio_driver.loop();
  } else if (s_listening) {
    closeClient();
    s_server.stop();
    s_listening = false;
  }
  updateDisplay();
}
#endif
