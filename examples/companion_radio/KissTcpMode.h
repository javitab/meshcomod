#pragma once

#ifdef COMPANION_KISS_TCP
#include "DataStore.h"
#include "RadioModeStore.h"
#include <helpers/ui/DisplayDriver.h>

#ifndef KISS_TCP_PORT
#define KISS_TCP_PORT 8001
#endif

bool companionModeBegin();
bool kissTcpIsActive();
bool companionModeCanUseKiss();
bool companionModeSelect(CompanionRadioMode mode, const char*& error);
void companionModeCommand(const char* command, char* reply, size_t size);
void kissTcpBegin(DataStore& store, const NodePrefs& defaults, mesh::RNG& rng, DisplayDriver* display);
void kissTcpLoop();
#endif
