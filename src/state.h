#pragma once

#include <Arduino.h>
#include "config.h"

enum PedalMode
{
  PEDAL_NONE,
  PEDAL_SWITCH,
  PEDAL_EXPRESSION
};

extern unsigned long motorStartedMillis;
extern unsigned long stallIgnoreUntilMillis;
extern unsigned long stallDiagHighSinceMillis;

extern uint8_t speedIndex;
extern uint8_t selectedSpeedIndex;
extern float activeRpm;
extern float targetRpm;
extern bool motorRunning;
extern bool directionForward;
extern bool directionChangePending;
extern bool pendingDirectionForward;
extern float directionChangeResumeRpm;
extern bool stallFault;
extern bool driverOnline;
extern bool uartWarningAcknowledged;
extern bool manualRunRequested;
extern bool pedalSwitchPressed;
extern PedalMode pedalMode;

void ignoreStallGuardFor(unsigned long durationMs);
void clearStallFault();
