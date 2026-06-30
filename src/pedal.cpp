#include "pedal.h"

#include "config.h"
#include "motion.h"
#include "state.h"
#include "ui_lcd.h"

unsigned long lastPedalPollMillis = 0;
unsigned long pedalModeChangedMillis = 0;
unsigned long pedalSwitchChangedMillis = 0;
int pedalSleeveAdc = 0;
int pedalTipAdc = 0;
int rawPedalSleeveAdc = 0;
int rawPedalTipAdc = 0;
bool rawPedalSwitchPressed = false;
bool pedalAdcInitialized = false;
PedalMode rawPedalMode = PEDAL_NONE;

PedalMode detectPedalMode(int sleeveAdc, int tipAdc)
{
  if (sleeveAdc > PEDAL_DISCONNECTED_ADC && tipAdc > PEDAL_DISCONNECTED_ADC)
    return PEDAL_NONE;

  if (sleeveAdc < PEDAL_SWITCH_SLEEVE_ADC &&
      (tipAdc < PEDAL_SWITCH_TIP_LOW_ADC || tipAdc > PEDAL_SWITCH_TIP_HIGH_ADC))
    return PEDAL_SWITCH;

  return PEDAL_EXPRESSION;
}

const char *pedalModeName(PedalMode mode)
{
  switch (mode)
  {
  case PEDAL_SWITCH:
    return "switch";
  case PEDAL_EXPRESSION:
    return "expression";
  default:
    return "none";
  }
}

void printPedalDebug()
{
  Serial.print("Pedal ADC raw S/A3: ");
  Serial.print(rawPedalSleeveAdc);
  Serial.print(" T/A4: ");
  Serial.print(rawPedalTipAdc);
  Serial.print(" filt S/A3: ");
  Serial.print(pedalSleeveAdc);
  Serial.print(" T/A4: ");
  Serial.print(pedalTipAdc);
  Serial.print(" mode: ");
  Serial.print(pedalModeName(pedalMode));
  if (pedalMode == PEDAL_EXPRESSION)
  {
    Serial.print(" pos: ");
    Serial.print(expressionPositionFromAdc(pedalSleeveAdc), 3);
    Serial.print(" target RPM: ");
    Serial.print(targetRpm);
  }
  else if (pedalMode == PEDAL_SWITCH)
  {
    Serial.print(" pressed: ");
    Serial.print(pedalSwitchPressed ? "yes" : "no");
  }
  Serial.println();
}

float expressionPositionFromAdc(int sleeveAdc)
{
  float position = (float)(sleeveAdc - EXPRESSION_HEEL_ADC) / (float)(EXPRESSION_TOE_ADC - EXPRESSION_HEEL_ADC);

  if (position < 0.0f)
    return 0.0f;
  if (position > 1.0f)
    return 1.0f;

  return position;
}

float expressionRpmFromPosition(float position)
{
  if (position <= 0.0f)
    return 0.0f;

  if (position <= EXPRESSION_LOW_SPEED_POSITION)
  {
    float lowSpeedRatio = position / EXPRESSION_LOW_SPEED_POSITION;
    return EXPRESSION_STOP_RPM +
           ((EXPRESSION_LOW_SPEED_RPM - EXPRESSION_STOP_RPM) * lowSpeedRatio);
  }

  float highPosition = (position - EXPRESSION_LOW_SPEED_POSITION) / (1.0f - EXPRESSION_LOW_SPEED_POSITION);
  return EXPRESSION_LOW_SPEED_RPM +
         ((EXPRESSION_MAX_RPM - EXPRESSION_LOW_SPEED_RPM) * highPosition * highPosition);
}

void applyPedalControl()
{
  if (pedalMode == PEDAL_NONE)
    return;

  if (stallFault)
  {
    setMotorRunning(false);
    return;
  }

  if (pedalMode == PEDAL_SWITCH)
  {
    setTargetRpm(SPEED_PRESETS_RPM[selectedSpeedIndex], selectedSpeedIndex);
    setMotorRunning(pedalSwitchPressed);
    return;
  }

  float position = expressionPositionFromAdc(pedalSleeveAdc);
  float nextTargetRpm = expressionRpmFromPosition(position);

  if (nextTargetRpm < EXPRESSION_STOP_RPM)
  {
    if (directionChangePending)
    {
      directionChangeResumeRpm = 0.0f;
      return;
    }

    targetRpm = 0.0f;
    return;
  }

  float targetDelta = nextTargetRpm > targetRpm ? nextTargetRpm - targetRpm : targetRpm - nextTargetRpm;
  if (targetDelta < EXPRESSION_TARGET_HYSTERESIS_RPM)
    return;

  setTargetRpm(nextTargetRpm, speedBucketForRpm(nextTargetRpm));
  setMotorRunning(true);
}

void pollPedal()
{
  unsigned long now = millis();

  if (now - lastPedalPollMillis < PEDAL_POLL_INTERVAL_MS)
    return;
  lastPedalPollMillis = now;

  rawPedalSleeveAdc = analogRead(PEDAL_SLEEVE_PIN);
  rawPedalTipAdc = analogRead(PEDAL_TIP_PIN);

  if (!pedalAdcInitialized)
  {
    pedalSleeveAdc = rawPedalSleeveAdc;
    pedalTipAdc = rawPedalTipAdc;
    pedalAdcInitialized = true;
  }
  else
  {
    pedalSleeveAdc = ((pedalSleeveAdc * 3) + rawPedalSleeveAdc) / 4;
    pedalTipAdc = ((pedalTipAdc * 3) + rawPedalTipAdc) / 4;
  }

  PedalMode detectedMode = detectPedalMode(pedalSleeveAdc, pedalTipAdc);
  if (detectedMode != rawPedalMode)
  {
    rawPedalMode = detectedMode;
    pedalModeChangedMillis = now;
  }

  if (rawPedalMode != pedalMode && now - pedalModeChangedMillis >= PEDAL_MODE_DEBOUNCE_MS)
  {
    pedalMode = rawPedalMode;
    pedalSwitchPressed = false;
    rawPedalSwitchPressed = false;
    pedalSwitchChangedMillis = now;

    Serial.print("Pedal mode: ");
    Serial.println(pedalModeName(pedalMode));

    if (pedalMode == PEDAL_NONE)
    {
      setTargetRpm(SPEED_PRESETS_RPM[selectedSpeedIndex], selectedSpeedIndex);
      setMotorRunning(manualRunRequested);
    }
    else
    {
      manualRunRequested = false;
      applyPedalControl();
    }

    updateLcd();
  }

  if (pedalMode == PEDAL_SWITCH)
  {
    bool detectedPressed = pedalTipAdc < PEDAL_SWITCH_TIP_LOW_ADC;
    if (detectedPressed != rawPedalSwitchPressed)
    {
      rawPedalSwitchPressed = detectedPressed;
      pedalSwitchChangedMillis = now;
    }

    if (rawPedalSwitchPressed != pedalSwitchPressed &&
        now - pedalSwitchChangedMillis >= PEDAL_SWITCH_DEBOUNCE_MS)
    {
      pedalSwitchPressed = rawPedalSwitchPressed;
      applyPedalControl();
      updateLcd();
    }
  }
  else if (pedalMode == PEDAL_EXPRESSION)
  {
    applyPedalControl();
  }
}

void setupPedal()
{
  pinMode(PEDAL_SLEEVE_PIN, INPUT_PULLUP);
  pinMode(PEDAL_TIP_PIN, INPUT_PULLUP);
}
