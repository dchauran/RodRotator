#include "state.h"

unsigned long motorStartedMillis = 0;
unsigned long stallIgnoreUntilMillis = 0;
unsigned long stallDiagHighSinceMillis = 0;

uint8_t speedIndex = DEFAULT_SPEED_INDEX;
uint8_t selectedSpeedIndex = DEFAULT_SPEED_INDEX;
float activeRpm = SPEED_PRESETS_RPM[DEFAULT_SPEED_INDEX];
float targetRpm = SPEED_PRESETS_RPM[DEFAULT_SPEED_INDEX];
bool motorRunning = false;
bool directionForward = true;
bool directionChangePending = false;
bool pendingDirectionForward = true;
float directionChangeResumeRpm = SPEED_PRESETS_RPM[DEFAULT_SPEED_INDEX];
bool stallFault = false;
bool driverOnline = false;
bool uartWarningAcknowledged = false;
bool manualRunRequested = false;
bool pedalSwitchPressed = false;
PedalMode pedalMode = PEDAL_NONE;

void ignoreStallGuardFor(unsigned long durationMs)
{
  stallIgnoreUntilMillis = millis() + durationMs;
  stallDiagHighSinceMillis = 0;
}

void clearStallFault()
{
  stallFault = false;
  ignoreStallGuardFor(STALL_IGNORE_AFTER_CHANGE_MS);
}
