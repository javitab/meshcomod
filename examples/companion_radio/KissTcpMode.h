#pragma once

#ifdef COMPANION_KISS_TCP
#include "DataStore.h"
#include <helpers/ui/DisplayDriver.h>

bool companionModeBegin();
bool kissTcpIsActive();
void companionModeCommand(const char* command, char* reply, size_t size);
void kissTcpBegin(DataStore& store, const NodePrefs& defaults, mesh::RNG& rng, DisplayDriver* display);
void kissTcpLoop();
#endif
