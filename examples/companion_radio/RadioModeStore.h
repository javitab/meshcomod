#pragma once

#include <stdint.h>

enum class CompanionRadioMode : uint8_t { Companion = 0, KissTcp = 1 };

const char* radioModeName(CompanionRadioMode mode);
bool radioModeLoad(CompanionRadioMode& mode, const char*& error);
bool radioModeSave(CompanionRadioMode mode, const char*& error);
