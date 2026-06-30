#include <Arduino.h>

#include "driver_control.h"
#include "motion.h"
#include "pedal.h"
#include "ui_lcd.h"

unsigned long lastLcdMillis = 0;

void setup()
{
  setupMotion();

  Serial.begin(115200);
  Serial.println("Rod Rotator ready");

  setupDriver();
  setupPedal();
  setupLcd();
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
