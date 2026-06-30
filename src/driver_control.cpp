#include "driver_control.h"

#include <Arduino.h>
#include <SoftwareSerial.h>
#include <TMCStepper.h>

#include "config.h"
#include "motion.h"
#include "state.h"
#include "ui_lcd.h"

SoftwareSerial tmcSerial(TMC_RX_PIN, TMC_TX_PIN);
TMC2209Stepper driver(&tmcSerial, TMC_R_SENSE, TMC_DRIVER_ADDRESS);

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
  pinMode(DIAG_PIN, INPUT);
  pinMode(TMC_RX_PIN, INPUT);
  pinMode(TMC_TX_PIN, OUTPUT);

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
