#include <Arduino.h>
#include <LiquidCrystal.h>
#include <SoftwareSerial.h>
#include <TMCStepper.h>
#include <avr/interrupt.h>

// ---------------------------------------------------------------------------
// User configuration
// ---------------------------------------------------------------------------

// Motor and speed table. MICROSTEPS is written to the TMC2209 over UART and
// also used for RPM math, so it must match the actual driver microstep mode.
// Keep the current/stall/debounce arrays aligned with SPEED_PRESETS_RPM: each
// entry in a column applies to the same speed preset.
const int     MOTOR_STEPS_PER_REV              = 200;
const int     MICROSTEPS                       = 8;
const uint8_t DEFAULT_SPEED_INDEX              = 1;  // 0-based; 1 = 10 RPM

// Preset column:                                0     1      2      3      4       5       6
const float         SPEED_PRESETS_RPM[]      = { 5.0,  10.0,  50.0,  75.0,  100.0,  150.0,  300.0 };
const uint16_t      RUN_CURRENT_MA[]         = { 750,  650,   550,   650,   700,    825,    1000 };
const uint8_t       STALL_GUARD_THRESHOLDS[] = { 13,   21,    75,    120,   140,    170,    190 };
const unsigned long STALL_DIAG_DEBOUNCE_MS[] = { 25,   25,    25,    25,    20,     15,     8 };

// Motion tuning. START_RAMP_RPM is the initial speed used when starting from
// rest; SPEED_RAMP_RPM_PER_SECOND controls speed changes and direction changes.
const float         START_RAMP_RPM             = 10.0;
const float         SPEED_RAMP_RPM_PER_SECOND  = 80.0;
const unsigned long SPEED_RAMP_INTERVAL_MS     = 50;

// TMC2209 setup. Set USE_UART_CURRENT_CONTROL false to use the driver's Vref
// potentiometer for current while still using UART for other driver settings.
const bool    ENABLE_ACTIVE_LOW                = true;
const bool    USE_UART_CURRENT_CONTROL         = true;
const uint8_t TMC_DRIVER_ADDRESS               = 0b00;
const float   TMC_R_SENSE                      = 0.11f;
const float   TMC_HOLD_CURRENT_MULTIPLIER      = 1.0f;

// StallGuard blanking and debounce. Higher SGTHRS values are more sensitive;
// the ignore windows prevent start/ramp transients from latching as stalls.
const unsigned long STALL_IGNORE_AFTER_START_MS  = 1000;
const unsigned long STALL_IGNORE_AFTER_CHANGE_MS = 500;
const unsigned long STALL_IGNORE_DURING_RAMP_MS  = 200;

// Pedal detection and expression calibration. Press SELECT while a pedal is
// connected to print raw/filtered ADC values for calibration.
const int           PEDAL_DISCONNECTED_ADC           = 950;
const int           PEDAL_SWITCH_SLEEVE_ADC          = 120;
const int           PEDAL_SWITCH_TIP_LOW_ADC         = 120;
const int           PEDAL_SWITCH_TIP_HIGH_ADC        = 900;
const int           EXPRESSION_HEEL_ADC              = 72;
const int           EXPRESSION_TOE_ADC               = 332;
const float         EXPRESSION_STOP_RPM              = 5.0f;
const float         EXPRESSION_MAX_RPM               = 300.0f;
const float         EXPRESSION_LOW_SPEED_POSITION    = 0.25f;
const float         EXPRESSION_LOW_SPEED_RPM         = 15.0f;
const float         EXPRESSION_TARGET_HYSTERESIS_RPM = 1.0f;

// Input timing. Button polling is intentionally fast; motor timing is handled
// by Timer1 interrupts, so these values should not affect step pulse cadence.
const unsigned long BUTTON_POLL_INTERVAL_MS      = 15;
const unsigned long BUTTON_DEBOUNCE_MS           = 35;
const unsigned long PEDAL_POLL_INTERVAL_MS       = 25;
const unsigned long PEDAL_MODE_DEBOUNCE_MS       = 200;
const unsigned long PEDAL_SWITCH_DEBOUNCE_MS     = 35;

// LCD keypad ADC thresholds. Change these if a different shield reports
// noticeably different analog values for the buttons.
const uint8_t LCD_SHIELD_VERSION              = 1;  // 1 = older V1.0 ladder, 2 = newer V1.1/V2-style ladder
const int LCD_BUTTON_NONE_MIN_ADC             = 1000;
const int LCD_BUTTON_RIGHT_MAX_ADC            = 50;
const int LCD_BUTTON_UP_MAX_ADC               = LCD_SHIELD_VERSION == 1 ? 195 : 250;
const int LCD_BUTTON_DOWN_MAX_ADC             = LCD_SHIELD_VERSION == 1 ? 380 : 450;
const int LCD_BUTTON_LEFT_MAX_ADC             = LCD_SHIELD_VERSION == 1 ? 555 : 650;
const int LCD_BUTTON_SELECT_MAX_ADC           = LCD_SHIELD_VERSION == 1 ? 790 : 850;

// Board pinout. The LCD keypad shield owns D4-D9 and A0.
const int STEP_PIN                            = 2;   // TMC2209 STEP
const int DIR_PIN                             = 3;   // TMC2209 DIR
const int EN_PIN                              = 11;  // TMC2209 EN, usually active LOW
const int BUTTON_PIN                          = A0;  // LCD keypad analog buttons
const int DIAG_PIN                            = A1;  // TMC2209 DIAG / StallGuard output
const int TMC_RX_PIN                          = A2;  // Arduino RX, tied to PDN_UART node
const int PEDAL_SLEEVE_PIN                    = A3;  // TRS sleeve sense, INPUT_PULLUP
const int PEDAL_TIP_PIN                       = A4;  // TRS tip sense, INPUT_PULLUP
const int TMC_TX_PIN                          = 12;  // Arduino TX through 1k to PDN_UART

// Advanced timing. Most builds should leave this alone.
const unsigned long STEALTHCHOP_STANDSTILL_TUNE_MS = 150;
const uint8_t       STEP_PULSE_US                  = 20;

// ---------------------------------------------------------------------------
// Internal constants and state
// ---------------------------------------------------------------------------

const int STEPS_PER_REV = MOTOR_STEPS_PER_REV * MICROSTEPS;
const size_t SPEED_PRESET_COUNT = sizeof(SPEED_PRESETS_RPM) / sizeof(SPEED_PRESETS_RPM[0]);

enum PedalMode
{
  PEDAL_NONE,
  PEDAL_SWITCH,
  PEDAL_EXPRESSION
};

SoftwareSerial tmcSerial(TMC_RX_PIN, TMC_TX_PIN);
TMC2209Stepper driver(&tmcSerial, TMC_R_SENSE, TMC_DRIVER_ADDRESS);

// Select the pins used on the LCD panel
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

// Define some values used by the panel and buttons
int adc_key_in = 0;
#define btnRIGHT 0
#define btnUP 1
#define btnDOWN 2
#define btnLEFT 3
#define btnSELECT 4
#define btnNONE 5

const uint16_t TIMER1_PRESCALER = 8;
const uint32_t TIMER1_TICKS_PER_SECOND = F_CPU / TIMER1_PRESCALER;
const uint16_t STEP_PULSE_TICKS = STEP_PULSE_US * (TIMER1_TICKS_PER_SECOND / 1000000UL);

volatile uint16_t stepPeriodTicks = 0;
volatile uint16_t nextStepCompare = 0;
volatile bool timerRunning = false;
volatile uint8_t *stepPort = nullptr;
uint8_t stepMask = 0;

unsigned long lastLcdMillis = 0;
unsigned long lastButtonPollMillis = 0;
unsigned long buttonChangedMillis = 0;
unsigned long lastPedalPollMillis = 0;
unsigned long pedalModeChangedMillis = 0;
unsigned long pedalSwitchChangedMillis = 0;
unsigned long motorStartedMillis = 0;
unsigned long stallIgnoreUntilMillis = 0;
unsigned long stallDiagHighSinceMillis = 0;
unsigned long lastRampMillis = 0;
int rawButton = btnNONE;
int stableButton = btnNONE;
int pedalSleeveAdc = 0;
int pedalTipAdc = 0;
int rawPedalSleeveAdc = 0;
int rawPedalTipAdc = 0;
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
bool manualRunRequested = false;
bool rawPedalSwitchPressed = false;
bool pedalSwitchPressed = false;
bool pedalAdcInitialized = false;
PedalMode rawPedalMode = PEDAL_NONE;
PedalMode pedalMode = PEDAL_NONE;

void updateLcd();
void applyDriverPreset();
void printDriverPreset();
void applyPedalControl();
void configureStepRate(float rpm);
void printPedalDebug();
float expressionPositionFromAdc(int sleeveAdc);
void finishPendingDirectionChange();

void ignoreStallGuardFor(unsigned long durationMs)
{
  stallIgnoreUntilMillis = millis() + durationMs;
  stallDiagHighSinceMillis = 0;
}

int readLcdButtons()
{
  adc_key_in = analogRead(BUTTON_PIN);

  if (adc_key_in > LCD_BUTTON_NONE_MIN_ADC)
    return btnNONE;
  if (adc_key_in < LCD_BUTTON_RIGHT_MAX_ADC)
    return btnRIGHT;
  if (adc_key_in < LCD_BUTTON_UP_MAX_ADC)
    return btnUP;
  if (adc_key_in < LCD_BUTTON_DOWN_MAX_ADC)
    return btnDOWN;
  if (adc_key_in < LCD_BUTTON_LEFT_MAX_ADC)
    return btnLEFT;
  if (adc_key_in < LCD_BUTTON_SELECT_MAX_ADC)
    return btnSELECT;

  return btnNONE;
}

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

void handleButton(int button)
{
  switch (button)
  {
  case btnUP:
    if (selectedSpeedIndex < SPEED_PRESET_COUNT - 1)
    {
      setSpeedIndex(selectedSpeedIndex + 1);
    }
    break;

  case btnDOWN:
    if (selectedSpeedIndex > 0)
    {
      setSpeedIndex(selectedSpeedIndex - 1);
    }
    break;

  case btnRIGHT:
    setDirection(true);
    break;

  case btnLEFT:
    setDirection(false);
    break;

  case btnSELECT:
    if (stallFault)
    {
      stallFault = false;
      applyPedalControl();
      updateLcd();
    }
    else if (pedalMode == PEDAL_NONE)
    {
      manualRunRequested = !manualRunRequested;
      setMotorRunning(manualRunRequested);
    }
    else
    {
      printPedalDebug();
    }
    break;

  default:
    break;
  }
}

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

void pollButtons()
{
  unsigned long now = millis();

  if (now - lastButtonPollMillis < BUTTON_POLL_INTERVAL_MS)
    return;
  lastButtonPollMillis = now;

  int button = readLcdButtons();

  if (button != rawButton)
  {
    rawButton = button;
    buttonChangedMillis = now;
    return;
  }

  if (button != stableButton && now - buttonChangedMillis >= BUTTON_DEBOUNCE_MS)
  {
    stableButton = button;

    if (stableButton != btnNONE)
    {
      handleButton(stableButton);
    }
  }
}

void updateLcd()
{
  lcd.setCursor(0, 0);
  if (stallFault)
  {
    lcd.print("STAL");
  }
  else
  {
    lcd.print(motorRunning ? "Run " : "Stop");
    lcd.print(driverOnline ? "" : "!");
  }

  lcd.print(directionForward ? " Fwd " : " Rev ");
  lcd.print("RPM ");
  if (pedalMode == PEDAL_EXPRESSION)
  {
    lcd.print((int)(targetRpm + 0.5f));
  }
  else
  {
    lcd.print((int)SPEED_PRESETS_RPM[selectedSpeedIndex]);
  }
  lcd.print("   ");

  lcd.setCursor(0, 1);
  if (stallFault)
  {
    lcd.print("Select to clear ");
  }
  else if (pedalMode == PEDAL_SWITCH)
  {
    lcd.print(pedalSwitchPressed ? "Ped SW on       " : "Ped SW off      ");
  }
  else if (pedalMode == PEDAL_EXPRESSION)
  {
    lcd.print("Ped EXP         ");
  }
  else
  {
    lcd.print("Sel run  Up/Dn  ");
  }
}

void applyDriverPreset()
{
  if (!driverOnline)
    return;

  if (USE_UART_CURRENT_CONTROL)
  {
    driver.I_scale_analog(false);
    driver.rms_current(RUN_CURRENT_MA[speedIndex], TMC_HOLD_CURRENT_MULTIPLIER);
  }
  else
  {
    driver.I_scale_analog(true);
    driver.irun(31);
    driver.ihold(31);
  }

  driver.en_spreadCycle(false);
  driver.SGTHRS(STALL_GUARD_THRESHOLDS[speedIndex]);
}

void printDriverPreset()
{
  Serial.print("current: ");
  if (USE_UART_CURRENT_CONTROL)
  {
    Serial.print(RUN_CURRENT_MA[speedIndex]);
    Serial.print("mA UART");
  }
  else
  {
    Serial.print("Vref pot");
  }

  Serial.print(" cs_actual_mA: ");
  Serial.print(driver.cs2rms(driver.cs_actual()));
  Serial.print(" IRUN: ");
  Serial.print(driver.irun());
  Serial.print(" IHOLD: ");
  Serial.print(driver.ihold());
  Serial.print(" SGTHRS: ");
  Serial.println(STALL_GUARD_THRESHOLDS[speedIndex]);
}

void setupDriver()
{
  tmcSerial.begin(115200);

  driver.begin();
  driver.pdn_disable(true);
  driver.I_scale_analog(!USE_UART_CURRENT_CONTROL);
  driver.mstep_reg_select(true);
  driver.toff(4);
  driver.blank_time(24);
  driver.iholddelay(15);
  driver.TPOWERDOWN(255);
  driver.microsteps(MICROSTEPS);
  driver.intpol(true);
  driver.semin(0);
  driver.en_spreadCycle(false);
  driver.TCOOLTHRS(0xFFFFF);
  driver.pwm_autoscale(true);
  driver.pwm_autograd(true);
  driver.pwm_reg(8);
  driver.pwm_lim(12);
  driver.TPWMTHRS(0); // keep StealthChop-only while testing

  driverOnline = driver.test_connection() == 0;
  applyDriverPreset();

  Serial.print("TMC2209 UART: ");
  Serial.println(driverOnline ? "OK" : "not detected");
  if (driverOnline)
  {
    printDriverPreset();
  }
}

void pollStallGuard()
{
  if (!motorRunning || stallFault)
    return;

  unsigned long now = millis();

  if (now - motorStartedMillis < STALL_IGNORE_AFTER_START_MS)
    return;

  if ((long)(now - stallIgnoreUntilMillis) < 0)
    return;

  if (digitalRead(DIAG_PIN) == HIGH)
  {
    if (stallDiagHighSinceMillis == 0)
      stallDiagHighSinceMillis = now;

    if (now - stallDiagHighSinceMillis >= STALL_DIAG_DEBOUNCE_MS[speedIndex])
    {
      stallFault = true;
      setMotorRunning(false);
      Serial.println("StallGuard DIAG asserted");
      updateLcd();
    }
  }
  else
  {
    stallDiagHighSinceMillis = 0;
  }
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

void setup()
{
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(EN_PIN, OUTPUT);
  pinMode(DIAG_PIN, INPUT);
  pinMode(TMC_RX_PIN, INPUT);
  pinMode(PEDAL_SLEEVE_PIN, INPUT_PULLUP);
  pinMode(PEDAL_TIP_PIN, INPUT_PULLUP);
  pinMode(TMC_TX_PIN, OUTPUT);

  stepPort = portOutputRegister(digitalPinToPort(STEP_PIN));
  stepMask = digitalPinToBitMask(STEP_PIN);
  digitalWrite(STEP_PIN, LOW);
  digitalWrite(DIR_PIN, directionForward ? HIGH : LOW);
  setDriverEnabled(false);
  configureStepRate(targetRpm);

  TCCR1A = 0;
  TCCR1B = _BV(CS11);
  TIMSK1 &= ~(_BV(OCIE1A) | _BV(OCIE1B));

  Serial.begin(115200);
  Serial.println("Rod Rotator ready");
  setupDriver();

  lcd.begin(16, 2);
  lcd.print("Rod Rotator");
  delay(750);
  updateLcd();
}

void loop()
{
  pollPedal();
  pollButtons();
  pollSpeedRamp();
  pollStallGuard();

  if (millis() - lastLcdMillis >= 1000)
  {
    lastLcdMillis = millis();
    updateLcd();
  }
}
