#pragma once

#include <Arduino.h>
#include "state.h"

void setupPedal();
void pollPedal();
void applyPedalControl();
void printPedalDebug();
float expressionPositionFromAdc(int sleeveAdc);
const char *pedalModeName(PedalMode mode);
