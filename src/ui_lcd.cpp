#include "ui_lcd.h"

#include <Arduino.h>
#include <LiquidCrystal.h>

#include "config.h"
#include "driver_control.h"
#include "motion.h"
#include "pedal.h"
#include "state.h"

#define btnRIGHT 0
#define btnUP 1
#define btnDOWN 2
#define btnLEFT 3
#define btnSELECT 4
#define btnNONE 5

LiquidCrystal lcd(LCD_RS_PIN, LCD_ENABLE_PIN, LCD_D4_PIN, LCD_D5_PIN, LCD_D6_PIN, LCD_D7_PIN);

unsigned long lastButtonPollMillis = 0;
unsigned long buttonChangedMillis = 0;
int rawButton = btnNONE;
int stableButton = btnNONE;

int readLcdButtons()
{
  int adcKeyIn = analogRead(BUTTON_PIN);

  if (adcKeyIn > LCD_BUTTON_NONE_MIN_ADC)
    return btnNONE;
  if (adcKeyIn < LCD_BUTTON_RIGHT_MAX_ADC)
    return btnRIGHT;
  if (adcKeyIn < LCD_BUTTON_UP_MAX_ADC)
    return btnUP;
  if (adcKeyIn < LCD_BUTTON_DOWN_MAX_ADC)
    return btnDOWN;
  if (adcKeyIn < LCD_BUTTON_LEFT_MAX_ADC)
    return btnLEFT;
  if (adcKeyIn < LCD_BUTTON_SELECT_MAX_ADC)
    return btnSELECT;

  return btnNONE;
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

void setupLcd()
{
  lcd.begin(16, 2);
  lcd.print("Rod Rotator");
  delay(750);
  updateLcd();
}
