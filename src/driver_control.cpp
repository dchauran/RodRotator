#include "driver_control.h"

#include <Arduino.h>
#include <TMCStepper.h>

#include "config.h"
#include "motion.h"
#include "state.h"
#include "ui_lcd.h"

#if TMC_DRIVER_TYPE == TMC_DRIVER_TMC2208
TMC2208Stepper driver(TMC_RX_PIN, TMC_TX_PIN, TMC_R_SENSE);
const char *TMC_DRIVER_NAME = "TMC2208";
#elif TMC_DRIVER_TYPE == TMC_DRIVER_TMC2209
TMC2209Stepper driver(TMC_RX_PIN, TMC_TX_PIN, TMC_R_SENSE, TMC_DRIVER_ADDRESS);
const char *TMC_DRIVER_NAME = "TMC2209";
#else
#error "Unsupported CONFIG_TMC_DRIVER_TYPE"
#endif

void applyDriverPreset()
{
  if (!USE_TMC_UART || !driverOnline)
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
#if TMC_DRIVER_TYPE == TMC_DRIVER_TMC2209
  if (USE_STALL_GUARD)
  {
    driver.SGTHRS(STALL_GUARD_THRESHOLDS[speedIndex]);
  }
#endif
}

void printDriverPreset()
{
  if (!USE_TMC_UART)
  {
    Serial.println("driver: STEP/DIR fallback, UART disabled");
    return;
  }

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
#if TMC_DRIVER_TYPE == TMC_DRIVER_TMC2209
  if (USE_STALL_GUARD)
  {
    Serial.print(" SGTHRS: ");
    Serial.println(STALL_GUARD_THRESHOLDS[speedIndex]);
  }
  else
  {
    Serial.println(" StallGuard disabled");
  }
#else
  Serial.println(" StallGuard unavailable on TMC2208");
#endif
}

void setupDriver()
{
  pinMode(DIAG_PIN, INPUT);
  pinMode(TMC_RX_PIN, INPUT);

  if (!USE_TMC_UART)
  {
    pinMode(TMC_TX_PIN, INPUT);
    driverOnline = true;
    uartWarningAcknowledged = true;
    Serial.print(TMC_DRIVER_NAME);
    Serial.println(" UART: disabled, STEP/DIR fallback");
    printDriverPreset();
    return;
  }

  pinMode(TMC_TX_PIN, OUTPUT);

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
#if TMC_DRIVER_TYPE == TMC_DRIVER_TMC2209
  driver.semin(0);
#endif
  driver.en_spreadCycle(false);
#if TMC_DRIVER_TYPE == TMC_DRIVER_TMC2209
  driver.TCOOLTHRS(0xFFFFF);
#endif
  driver.pwm_autoscale(true);
  driver.pwm_autograd(true);
  driver.pwm_reg(8);
  driver.pwm_lim(12);
  driver.TPWMTHRS(0); // keep StealthChop-only while testing

  driverOnline = driver.test_connection() == 0;
  uartWarningAcknowledged = driverOnline;
  applyDriverPreset();

  Serial.print(TMC_DRIVER_NAME);
  Serial.print(" UART: ");
  Serial.println(driverOnline ? "OK" : "not detected");
  if (driverOnline)
  {
    printDriverPreset();
  }
}

void pollStallGuard()
{
#if TMC_DRIVER_TYPE != TMC_DRIVER_TMC2209
  return;
#else
  if (!USE_TMC_UART || !USE_STALL_GUARD)
    return;

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
#endif
}
