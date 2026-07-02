#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// User configuration
// ---------------------------------------------------------------------------
// Copy config_user.example.h to config_user.h for local, untracked overrides.
// config_user.h is included at the end of this file. Override a default there
// with #undef CONFIG_NAME followed by #define CONFIG_NAME new_value.

// Motor and speed table. CONFIG_MICROSTEPS is used for RPM math and is written
// to the TMC2209 when UART is enabled, so it must match the actual driver
// microstep mode.
// Keep the current/stall/debounce arrays aligned with CONFIG_SPEED_PRESETS_RPM:
// each entry in a column applies to the same speed preset.
#define CONFIG_MOTOR_STEPS_PER_REV 200
#define CONFIG_MICROSTEPS          8
#define CONFIG_DEFAULT_SPEED_INDEX 1  // 0-based; 1 = 10 RPM

// Preset column:                       0      1      2      3       4       5       6
#define CONFIG_SPEED_PRESETS_RPM      { 5.0,  10.0,  50.0,  75.0,  100.0,  150.0,  300.0 }
#define CONFIG_RUN_CURRENT_MA         { 750,  650,   550,   650,   700,    825,    1000 }
#define CONFIG_STALL_GUARD_THRESHOLDS { 13,   21,    75,    120,   140,    170,    190 }
#define CONFIG_STALL_DIAG_DEBOUNCE_MS { 25,   25,    25,    25,    20,     15,     8 }

// Motion tuning. START_RAMP_RPM is the initial speed used when starting from
// rest; SPEED_RAMP_RPM_PER_SECOND controls speed changes and direction changes.
#define CONFIG_START_RAMP_RPM            10.0f
#define CONFIG_SPEED_RAMP_RPM_PER_SECOND 80.0f
#define CONFIG_SPEED_RAMP_INTERVAL_MS    50UL

// TMC driver setup. Set CONFIG_USE_TMC_UART false to use the driver as a plain
// STEP/DIR module with the existing UART/DIAG wires left connected but ignored.
// Set CONFIG_USE_UART_CURRENT_CONTROL false to use the driver's Vref
// potentiometer for current while still using UART for other settings.
#define CONFIG_TMC_DRIVER_TMC2208 2208
#define CONFIG_TMC_DRIVER_TMC2209 2209
#ifndef CONFIG_TMC_DRIVER_TYPE
#define CONFIG_TMC_DRIVER_TYPE CONFIG_TMC_DRIVER_TMC2209
#endif
#define CONFIG_ENABLE_ACTIVE_LOW           true
#ifndef CONFIG_USE_TMC_UART
#define CONFIG_USE_TMC_UART true
#endif
#define CONFIG_USE_UART_CURRENT_CONTROL    true
#ifndef CONFIG_USE_STALL_GUARD
#define CONFIG_USE_STALL_GUARD true
#endif
#define CONFIG_TMC_DRIVER_ADDRESS          0b00
#define CONFIG_TMC_R_SENSE                 0.11f
#define CONFIG_TMC_HOLD_CURRENT_MULTIPLIER 1.0f

// StallGuard blanking and debounce. Higher SGTHRS values are more sensitive;
// the ignore windows prevent start/ramp transients from latching as stalls.
#define CONFIG_STALL_IGNORE_AFTER_START_MS  1000UL
#define CONFIG_STALL_IGNORE_AFTER_CHANGE_MS 500UL
#define CONFIG_STALL_IGNORE_DURING_RAMP_MS  200UL

// Pedal detection and expression calibration. Press SELECT while a pedal is
// connected to print raw/filtered ADC values for calibration.
#define CONFIG_PEDAL_DISCONNECTED_ADC           950
#define CONFIG_PEDAL_SWITCH_SLEEVE_ADC          120
#define CONFIG_PEDAL_SWITCH_TIP_LOW_ADC         120
#define CONFIG_PEDAL_SWITCH_TIP_HIGH_ADC        900
#define CONFIG_EXPRESSION_HEEL_ADC              72
#define CONFIG_EXPRESSION_TOE_ADC               332
#define CONFIG_EXPRESSION_STOP_RPM              5.0f
#define CONFIG_EXPRESSION_MAX_RPM               300.0f
#define CONFIG_EXPRESSION_INVERT                false
#define CONFIG_EXPRESSION_LOW_SPEED_POSITION    0.25f
#define CONFIG_EXPRESSION_LOW_SPEED_RPM         15.0f
#define CONFIG_EXPRESSION_TARGET_HYSTERESIS_RPM 1.0f

// Input timing. Button polling is intentionally fast; motor timing is handled
// by Timer1 interrupts, so these values should not affect step pulse cadence.
#define CONFIG_BUTTON_POLL_INTERVAL_MS  15UL
#define CONFIG_BUTTON_DEBOUNCE_MS       35UL
#define CONFIG_PEDAL_POLL_INTERVAL_MS   25UL
#define CONFIG_PEDAL_MODE_DEBOUNCE_MS   200UL
#define CONFIG_PEDAL_SWITCH_DEBOUNCE_MS 35UL

// LCD keypad ADC thresholds. Version 1 is the older V1.0 resistor ladder;
// version 2 is the newer V1.1/V2-style ladder.
#define CONFIG_LCD_SHIELD_VERSION 1
#define CONFIG_LCD_BUTTON_NONE_MIN_ADC   1000
#define CONFIG_LCD_BUTTON_RIGHT_MAX_ADC  50
#define CONFIG_LCD_BUTTON_UP_MAX_ADC     (CONFIG_LCD_SHIELD_VERSION == 1 ? 195 : 250)
#define CONFIG_LCD_BUTTON_DOWN_MAX_ADC   (CONFIG_LCD_SHIELD_VERSION == 1 ? 380 : 450)
#define CONFIG_LCD_BUTTON_LEFT_MAX_ADC   (CONFIG_LCD_SHIELD_VERSION == 1 ? 555 : 650)
#define CONFIG_LCD_BUTTON_SELECT_MAX_ADC (CONFIG_LCD_SHIELD_VERSION == 1 ? 790 : 850)

// Board pinout. The LCD keypad shield owns D4-D9 and A0.
#define CONFIG_STEP_PIN         2   // TMC2209 STEP
#define CONFIG_DIR_PIN          3   // TMC2209 DIR
#define CONFIG_EN_PIN           11  // TMC2209 EN, usually active LOW
#define CONFIG_BUTTON_PIN       A0  // LCD keypad analog buttons
#define CONFIG_DIAG_PIN         A1  // TMC2209 DIAG / StallGuard output
#define CONFIG_TMC_RX_PIN       A2  // Arduino RX, tied to PDN_UART node
#define CONFIG_PEDAL_SLEEVE_PIN A3  // TRS sleeve sense, INPUT_PULLUP
#define CONFIG_PEDAL_TIP_PIN    A4  // TRS tip sense, INPUT_PULLUP
#define CONFIG_TMC_TX_PIN       12  // Arduino TX through 1k to PDN_UART

// LCD keypad shield pinout.
#define CONFIG_LCD_RS_PIN     8
#define CONFIG_LCD_ENABLE_PIN 9
#define CONFIG_LCD_D4_PIN     4
#define CONFIG_LCD_D5_PIN     5
#define CONFIG_LCD_D6_PIN     6
#define CONFIG_LCD_D7_PIN     7

// Advanced timing. Most builds should leave this alone.
#define CONFIG_STEALTHCHOP_STANDSTILL_TUNE_MS 150UL
#define CONFIG_STEP_PULSE_US                  20

// ---------------------------------------------------------------------------
// Firmware-facing names
// ---------------------------------------------------------------------------
// These aliases let the implementation read clean names while config_user.h
// only has to override CONFIG_* defaults.

#define MOTOR_STEPS_PER_REV CONFIG_MOTOR_STEPS_PER_REV
#define MICROSTEPS CONFIG_MICROSTEPS
#define DEFAULT_SPEED_INDEX CONFIG_DEFAULT_SPEED_INDEX
#define STEPS_PER_REV (MOTOR_STEPS_PER_REV * MICROSTEPS)

extern const float SPEED_PRESETS_RPM[];
extern const uint16_t RUN_CURRENT_MA[];
extern const uint8_t STALL_GUARD_THRESHOLDS[];
extern const unsigned long STALL_DIAG_DEBOUNCE_MS[];
extern const size_t SPEED_PRESET_COUNT;

#define START_RAMP_RPM CONFIG_START_RAMP_RPM
#define SPEED_RAMP_RPM_PER_SECOND CONFIG_SPEED_RAMP_RPM_PER_SECOND
#define SPEED_RAMP_INTERVAL_MS CONFIG_SPEED_RAMP_INTERVAL_MS

#define TMC_DRIVER_TMC2208 CONFIG_TMC_DRIVER_TMC2208
#define TMC_DRIVER_TMC2209 CONFIG_TMC_DRIVER_TMC2209
#define TMC_DRIVER_TYPE CONFIG_TMC_DRIVER_TYPE
#define ENABLE_ACTIVE_LOW CONFIG_ENABLE_ACTIVE_LOW
#define USE_TMC_UART CONFIG_USE_TMC_UART
#define USE_UART_CURRENT_CONTROL CONFIG_USE_UART_CURRENT_CONTROL
#define USE_STALL_GUARD CONFIG_USE_STALL_GUARD
#define TMC_DRIVER_ADDRESS CONFIG_TMC_DRIVER_ADDRESS
#define TMC_R_SENSE CONFIG_TMC_R_SENSE
#define TMC_HOLD_CURRENT_MULTIPLIER CONFIG_TMC_HOLD_CURRENT_MULTIPLIER

#define STALL_IGNORE_AFTER_START_MS CONFIG_STALL_IGNORE_AFTER_START_MS
#define STALL_IGNORE_AFTER_CHANGE_MS CONFIG_STALL_IGNORE_AFTER_CHANGE_MS
#define STALL_IGNORE_DURING_RAMP_MS CONFIG_STALL_IGNORE_DURING_RAMP_MS

#define PEDAL_DISCONNECTED_ADC CONFIG_PEDAL_DISCONNECTED_ADC
#define PEDAL_SWITCH_SLEEVE_ADC CONFIG_PEDAL_SWITCH_SLEEVE_ADC
#define PEDAL_SWITCH_TIP_LOW_ADC CONFIG_PEDAL_SWITCH_TIP_LOW_ADC
#define PEDAL_SWITCH_TIP_HIGH_ADC CONFIG_PEDAL_SWITCH_TIP_HIGH_ADC
#define EXPRESSION_HEEL_ADC CONFIG_EXPRESSION_HEEL_ADC
#define EXPRESSION_TOE_ADC CONFIG_EXPRESSION_TOE_ADC
#define EXPRESSION_STOP_RPM CONFIG_EXPRESSION_STOP_RPM
#define EXPRESSION_MAX_RPM CONFIG_EXPRESSION_MAX_RPM
#define EXPRESSION_INVERT CONFIG_EXPRESSION_INVERT
#define EXPRESSION_LOW_SPEED_POSITION CONFIG_EXPRESSION_LOW_SPEED_POSITION
#define EXPRESSION_LOW_SPEED_RPM CONFIG_EXPRESSION_LOW_SPEED_RPM
#define EXPRESSION_TARGET_HYSTERESIS_RPM CONFIG_EXPRESSION_TARGET_HYSTERESIS_RPM

#define BUTTON_POLL_INTERVAL_MS CONFIG_BUTTON_POLL_INTERVAL_MS
#define BUTTON_DEBOUNCE_MS CONFIG_BUTTON_DEBOUNCE_MS
#define PEDAL_POLL_INTERVAL_MS CONFIG_PEDAL_POLL_INTERVAL_MS
#define PEDAL_MODE_DEBOUNCE_MS CONFIG_PEDAL_MODE_DEBOUNCE_MS
#define PEDAL_SWITCH_DEBOUNCE_MS CONFIG_PEDAL_SWITCH_DEBOUNCE_MS

#define LCD_SHIELD_VERSION CONFIG_LCD_SHIELD_VERSION
#define LCD_BUTTON_NONE_MIN_ADC CONFIG_LCD_BUTTON_NONE_MIN_ADC
#define LCD_BUTTON_RIGHT_MAX_ADC CONFIG_LCD_BUTTON_RIGHT_MAX_ADC
#define LCD_BUTTON_UP_MAX_ADC CONFIG_LCD_BUTTON_UP_MAX_ADC
#define LCD_BUTTON_DOWN_MAX_ADC CONFIG_LCD_BUTTON_DOWN_MAX_ADC
#define LCD_BUTTON_LEFT_MAX_ADC CONFIG_LCD_BUTTON_LEFT_MAX_ADC
#define LCD_BUTTON_SELECT_MAX_ADC CONFIG_LCD_BUTTON_SELECT_MAX_ADC

#define STEP_PIN CONFIG_STEP_PIN
#define DIR_PIN CONFIG_DIR_PIN
#define EN_PIN CONFIG_EN_PIN
#define BUTTON_PIN CONFIG_BUTTON_PIN
#define LCD_RS_PIN CONFIG_LCD_RS_PIN
#define LCD_ENABLE_PIN CONFIG_LCD_ENABLE_PIN
#define LCD_D4_PIN CONFIG_LCD_D4_PIN
#define LCD_D5_PIN CONFIG_LCD_D5_PIN
#define LCD_D6_PIN CONFIG_LCD_D6_PIN
#define LCD_D7_PIN CONFIG_LCD_D7_PIN
#define DIAG_PIN CONFIG_DIAG_PIN
#define TMC_RX_PIN CONFIG_TMC_RX_PIN
#define PEDAL_SLEEVE_PIN CONFIG_PEDAL_SLEEVE_PIN
#define PEDAL_TIP_PIN CONFIG_PEDAL_TIP_PIN
#define TMC_TX_PIN CONFIG_TMC_TX_PIN

#define STEALTHCHOP_STANDSTILL_TUNE_MS CONFIG_STEALTHCHOP_STANDSTILL_TUNE_MS
#define STEP_PULSE_US CONFIG_STEP_PULSE_US

#if defined(__has_include)
#if __has_include("config_user.h")
#include "config_user.h"
#endif
#endif
