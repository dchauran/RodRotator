#pragma once

// Copy this file to src/config_user.h for local tuning. config_user.h is
// intentionally ignored by git, so your machine-specific values stay private.
// Uncomment only the values you want to override. Since config_user.h is
// included at the end of config.h, override with #undef followed by #define.

// #undef CONFIG_LCD_SHIELD_VERSION
// #define CONFIG_LCD_SHIELD_VERSION 2

// #undef CONFIG_MICROSTEPS
// #define CONFIG_MICROSTEPS 4

// Driver type is normally selected by the PlatformIO environment:
// uno_tmc2209, uno_tmc2208, or uno_stepdir. Override here only if you want a
// local config_user.h to take precedence over the selected build environment.
// TMC2208 supports UART but not StallGuard.
// #undef CONFIG_TMC_DRIVER_TYPE
// #define CONFIG_TMC_DRIVER_TYPE CONFIG_TMC_DRIVER_TMC2208

// Plain STEP/DIR fallback. This ignores UART and StallGuard without requiring
// the existing UART/DIAG wires to be disconnected.
// #undef CONFIG_USE_TMC_UART
// #define CONFIG_USE_TMC_UART false
// #undef CONFIG_USE_STALL_GUARD
// #define CONFIG_USE_STALL_GUARD false

// #undef CONFIG_SPEED_RAMP_RPM_PER_SECOND
// #define CONFIG_SPEED_RAMP_RPM_PER_SECOND 60.0f

// LCD keypad shield pinout, if your shield differs from the common layout.
// #undef CONFIG_LCD_RS_PIN
// #define CONFIG_LCD_RS_PIN 8
// #undef CONFIG_LCD_ENABLE_PIN
// #define CONFIG_LCD_ENABLE_PIN 9
// #undef CONFIG_LCD_D4_PIN
// #define CONFIG_LCD_D4_PIN 4
// #undef CONFIG_LCD_D5_PIN
// #define CONFIG_LCD_D5_PIN 5
// #undef CONFIG_LCD_D6_PIN
// #define CONFIG_LCD_D6_PIN 6
// #undef CONFIG_LCD_D7_PIN
// #define CONFIG_LCD_D7_PIN 7

// Keep these arrays aligned by column.
// Preset column:                       0      1      2      3       4       5       6
// #undef CONFIG_SPEED_PRESETS_RPM
// #define CONFIG_SPEED_PRESETS_RPM      { 5.0,  10.0,  50.0,  75.0,  100.0,  150.0,  300.0 }
// #undef CONFIG_RUN_CURRENT_MA
// #define CONFIG_RUN_CURRENT_MA         { 750,  650,   550,   650,   700,    825,    1000 }
// #undef CONFIG_STALL_GUARD_THRESHOLDS
// #define CONFIG_STALL_GUARD_THRESHOLDS { 13,   21,    75,    120,   140,    170,    190 }
// #undef CONFIG_STALL_DIAG_DEBOUNCE_MS
// #define CONFIG_STALL_DIAG_DEBOUNCE_MS { 25,   25,    25,    25,    20,     15,     8 }

// #undef CONFIG_EXPRESSION_HEEL_ADC
// #define CONFIG_EXPRESSION_HEEL_ADC 72
// #undef CONFIG_EXPRESSION_TOE_ADC
// #define CONFIG_EXPRESSION_TOE_ADC 332

// Reverse expression pedal direction if your pedal reads toe-down as low ADC.
// #undef CONFIG_EXPRESSION_INVERT
// #define CONFIG_EXPRESSION_INVERT true
