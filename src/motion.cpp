#include "motion.h"

#include <avr/interrupt.h>

#include "config.h"
#include "driver_control.h"
#include "pedal.h"
#include "state.h"
#include "ui_lcd.h"

const uint16_t TIMER1_PRESCALER = 8;
const uint32_t TIMER1_TICKS_PER_SECOND = F_CPU / TIMER1_PRESCALER;
const uint16_t STEP_PULSE_TICKS = STEP_PULSE_US * (TIMER1_TICKS_PER_SECOND / 1000000UL);

volatile uint16_t stepPeriodTicks = 0;
volatile uint16_t nextStepCompare = 0;
volatile bool timerRunning = false;
volatile uint8_t *stepPort = nullptr;
uint8_t stepMask = 0;
unsigned long lastRampMillis = 0;

uint16_t rpmToPeriodTicks(float rpm)
{
  float stepsPerSecond = (rpm * STEPS_PER_REV) / 60.0;
  uint32_t periodTicks = (uint32_t)((TIMER1_TICKS_PER_SECOND / stepsPerSecond) + 0.5);

  if (periodTicks <= STEP_PULSE_TICKS + 1)
    periodTicks = STEP_PULSE_TICKS + 2;
  if (periodTicks > 65535UL)
    periodTicks = 65535UL;

  return (uint16_t)periodTicks;
}

uint8_t speedBucketForRpm(float rpm)
{
  uint8_t bestIndex = 0;
  float bestDelta = SPEED_PRESETS_RPM[0] > rpm ? SPEED_PRESETS_RPM[0] - rpm : rpm - SPEED_PRESETS_RPM[0];

  for (uint8_t i = 1; i < SPEED_PRESET_COUNT; i++)
  {
    float delta = SPEED_PRESETS_RPM[i] > rpm ? SPEED_PRESETS_RPM[i] - rpm : rpm - SPEED_PRESETS_RPM[i];
    if (delta < bestDelta)
    {
      bestDelta = delta;
      bestIndex = i;
    }
  }

  return bestIndex;
}

void setSpeedBucket(uint8_t nextSpeedIndex)
{
  if (nextSpeedIndex == speedIndex)
    return;

  speedIndex = nextSpeedIndex;
  applyDriverPreset();
  ignoreStallGuardFor(STALL_IGNORE_AFTER_CHANGE_MS);

  if (motorRunning)
  {
    lastRampMillis = millis();
  }
}

void setTargetRpm(float nextTargetRpm, uint8_t nextSpeedIndex)
{
  if (nextTargetRpm < 0.0f)
    nextTargetRpm = 0.0f;
  if (nextTargetRpm > EXPRESSION_MAX_RPM)
    nextTargetRpm = EXPRESSION_MAX_RPM;

  if (directionChangePending)
  {
    directionChangeResumeRpm = nextTargetRpm;
    setSpeedBucket(nextSpeedIndex);
    return;
  }

  targetRpm = nextTargetRpm;
  setSpeedBucket(nextSpeedIndex);

  if (!motorRunning && targetRpm >= EXPRESSION_STOP_RPM)
  {
    activeRpm = targetRpm;
    configureStepRate(activeRpm);
  }
}

void setDriverEnabled(bool enabled)
{
  digitalWrite(EN_PIN, enabled == ENABLE_ACTIVE_LOW ? LOW : HIGH);
}

void configureStepRate(float rpm)
{
  uint16_t nextPeriodTicks = rpmToPeriodTicks(rpm);

  noInterrupts();
  stepPeriodTicks = nextPeriodTicks;
  interrupts();
}

void startStepTimer()
{
  noInterrupts();
  *stepPort &= ~stepMask;
  timerRunning = true;
  TCNT1 = 0;
  nextStepCompare = stepPeriodTicks;
  OCR1A = nextStepCompare;
  TIFR1 = _BV(OCF1A) | _BV(OCF1B);
  TIMSK1 |= _BV(OCIE1A);
  TIMSK1 &= ~_BV(OCIE1B);
  interrupts();
}

void stopStepTimer()
{
  noInterrupts();
  TIMSK1 &= ~(_BV(OCIE1A) | _BV(OCIE1B));
  timerRunning = false;
  interrupts();

  digitalWrite(STEP_PIN, LOW);
}

void setMotorRunning(bool running)
{
  if (running == motorRunning)
    return;

  motorRunning = running;
  if (motorRunning)
  {
    if (targetRpm < EXPRESSION_STOP_RPM)
    {
      motorRunning = false;
      return;
    }

    stallFault = false;
    activeRpm = min(targetRpm, START_RAMP_RPM);
    configureStepRate(activeRpm);
    setDriverEnabled(true);

    // StealthChop autoscale learns its standstill baseline before motion starts.
    delay(STEALTHCHOP_STANDSTILL_TUNE_MS);

    motorStartedMillis = millis();
    lastRampMillis = motorStartedMillis;
    ignoreStallGuardFor(STALL_IGNORE_AFTER_START_MS);
    startStepTimer();
  }
  else
  {
    directionChangePending = false;
    stopStepTimer();
    setDriverEnabled(false);
  }
}

void setDirection(bool forward)
{
  if (forward == directionForward)
  {
    if (directionChangePending)
    {
      directionChangePending = false;
      targetRpm = directionChangeResumeRpm;
      lastRampMillis = millis();
      ignoreStallGuardFor(STALL_IGNORE_AFTER_CHANGE_MS);
      updateLcd();
    }

    return;
  }

  if (directionChangePending)
  {
    pendingDirectionForward = forward;
    directionChangeResumeRpm = targetRpm >= EXPRESSION_STOP_RPM ? targetRpm : directionChangeResumeRpm;
    return;
  }

  if (motorRunning && activeRpm >= EXPRESSION_STOP_RPM)
  {
    directionChangePending = true;
    pendingDirectionForward = forward;
    directionChangeResumeRpm = targetRpm;
    targetRpm = 0.0f;
    lastRampMillis = millis();
    ignoreStallGuardFor(STALL_IGNORE_AFTER_CHANGE_MS);
    updateLcd();
    return;
  }

  directionForward = forward;
  digitalWrite(DIR_PIN, directionForward ? HIGH : LOW);
  ignoreStallGuardFor(STALL_IGNORE_AFTER_CHANGE_MS);
}

void setSpeedIndex(uint8_t nextSpeedIndex)
{
  if (nextSpeedIndex == selectedSpeedIndex)
    return;

  selectedSpeedIndex = nextSpeedIndex;
  if (pedalMode != PEDAL_EXPRESSION)
  {
    setTargetRpm(SPEED_PRESETS_RPM[selectedSpeedIndex], selectedSpeedIndex);
  }
  updateLcd();

  if (motorRunning)
  {
    Serial.print("RPM target: ");
    Serial.println(SPEED_PRESETS_RPM[selectedSpeedIndex]);
  }
  else
  {
    Serial.print("RPM: ");
    Serial.print(SPEED_PRESETS_RPM[selectedSpeedIndex]);
    Serial.print(" ");
    printDriverPreset();
  }
}

void finishPendingDirectionChange()
{
  stopStepTimer();
  activeRpm = 0.0f;

  directionForward = pendingDirectionForward;
  digitalWrite(DIR_PIN, directionForward ? HIGH : LOW);
  directionChangePending = false;
  targetRpm = directionChangeResumeRpm;

  if (targetRpm < EXPRESSION_STOP_RPM)
  {
    motorRunning = false;
    setDriverEnabled(false);
    updateLcd();
    return;
  }

  activeRpm = min(targetRpm, START_RAMP_RPM);
  configureStepRate(activeRpm);

  delay(STEALTHCHOP_STANDSTILL_TUNE_MS);

  unsigned long now = millis();
  motorStartedMillis = now;
  lastRampMillis = now;
  ignoreStallGuardFor(STALL_IGNORE_AFTER_START_MS);
  startStepTimer();
  updateLcd();
}

void pollSpeedRamp()
{
  if (!motorRunning)
    return;

  unsigned long now = millis();
  if (now - lastRampMillis < SPEED_RAMP_INTERVAL_MS)
    return;

  if (targetRpm < EXPRESSION_STOP_RPM && activeRpm <= EXPRESSION_STOP_RPM)
  {
    if (directionChangePending)
    {
      finishPendingDirectionChange();
      return;
    }

    activeRpm = 0.0f;
    setMotorRunning(false);
    return;
  }

  float deltaRpm = targetRpm - activeRpm;
  if (deltaRpm > -0.1 && deltaRpm < 0.1)
    return;

  float maxStep = SPEED_RAMP_RPM_PER_SECOND * (now - lastRampMillis) / 1000.0;
  lastRampMillis = now;

  if (deltaRpm > maxStep)
    activeRpm += maxStep;
  else if (deltaRpm < -maxStep)
    activeRpm -= maxStep;
  else
    activeRpm = targetRpm;

  if (targetRpm < EXPRESSION_STOP_RPM && activeRpm <= EXPRESSION_STOP_RPM)
  {
    if (directionChangePending)
    {
      finishPendingDirectionChange();
      return;
    }

    activeRpm = 0.0f;
    setMotorRunning(false);
    return;
  }

  configureStepRate(activeRpm);
  ignoreStallGuardFor(STALL_IGNORE_DURING_RAMP_MS);
}

void setupMotion()
{
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(EN_PIN, OUTPUT);

  stepPort = portOutputRegister(digitalPinToPort(STEP_PIN));
  stepMask = digitalPinToBitMask(STEP_PIN);
  digitalWrite(STEP_PIN, LOW);
  digitalWrite(DIR_PIN, directionForward ? HIGH : LOW);
  setDriverEnabled(false);
  configureStepRate(targetRpm);

  TCCR1A = 0;
  TCCR1B = _BV(CS11);
  TIMSK1 &= ~(_BV(OCIE1A) | _BV(OCIE1B));
}

ISR(TIMER1_COMPA_vect)
{
  if (!timerRunning)
    return;

  *stepPort |= stepMask;

  OCR1B = TCNT1 + STEP_PULSE_TICKS;
  TIFR1 = _BV(OCF1B);
  TIMSK1 |= _BV(OCIE1B);

  nextStepCompare += stepPeriodTicks;
  OCR1A = nextStepCompare;
}

ISR(TIMER1_COMPB_vect)
{
  *stepPort &= ~stepMask;
  TIMSK1 &= ~_BV(OCIE1B);
}
