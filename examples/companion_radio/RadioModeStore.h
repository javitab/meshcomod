#pragma once

#include <stdint.h>

enum class CompanionRadioMode : uint8_t { Companion = 0, KissTcp = 1 };

const char* radioModeName(CompanionRadioMode mode);
bool radioModeLoad(CompanionRadioMode& mode, const char*& error);
bool radioModeSave(CompanionRadioMode mode, const char*& error);
bool radioModeSelect(CompanionRadioMode mode, bool wifi_ready, const char*& error);

class RadioModeSwitch {
  bool _pending = false;

public:
  bool request(CompanionRadioMode mode, bool wifi_ready, const char*& error);
  bool pending() const { return _pending; }
  bool readyToReboot(bool button_pressed) const { return _pending && !button_pressed; }
};
