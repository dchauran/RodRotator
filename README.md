# Rod Rotator

This rod turner is built from parts I already had on hand:

- A stepper motor from my first 3D printer
- An Arduino Uno clone and LCD shield from an old kit
- A spare TMC2209 stepper driver from another printer build
- Assorted electronics, including a power supply, buck converter, and 1/4 in
  TRS jack
- An off-the-shelf guitar expression pedal and a cheap on/off footswitch pedal
  from an electric piano
- A CRB rod dryer that worked fine, but that I wanted to make more capable

So if a component choice looks oddly specific, it is probably because it was
already in the parts bin.

The project currently includes:

- PlatformIO firmware for a stepper-driven fishing rod rotator
- A 3D printed motor mount for attaching a stepper motor to a metal CRB rod
  dryer stand
- An adapter for a pre-existing
  [printable lathe chuck](https://www.thingiverse.com/thing:2670620)
- A printable case, not yet designed, likely derived from
  [this existing design](https://www.thingiverse.com/thing:845415)

The firmware targets an Arduino Uno-class board with an LCD keypad shield and a
BIGTREETECH-style TMC2209 driver over STEP/DIR plus one-wire UART. It supports
keypad speed presets, ramped direction changes, TMC2209 UART setup, StallGuard
stop detection, and optional TRS pedal control.

## Hardware

Tested around this setup:

- Arduino Uno-class board, currently an Inventr.io HERO
- LCD keypad shield using the common `LiquidCrystal lcd(8, 9, 4, 5, 6, 7)`
  pinout
- TMC2209 stepper driver
- NEMA 17 stepper motor
- 24 V motor supply, recommended for higher RPM operation
- Isolated 1/4 in TRS jack for optional pedals

The Arduino, driver logic, and motor supply must share ground. Do not feed the
motor supply voltage into the Arduino barrel jack.

## Wiring

The LCD keypad shield provides its own Arduino connections when it is mounted on
the board. The remaining connections are user-wired to the TMC2209, power
supply, and optional pedal jack.

Shield pins:

| Arduino | Used by |
| --- | --- |
| A0 | LCD keypad shield buttons |
| D4-D9 | LCD keypad shield display |

User-wired TMC2209 connections (connections can be made from the breakout pins on the shield):

| Arduino | Connects to |
| --- | --- |
| D2 | TMC2209 STEP |
| D3 | TMC2209 DIR |
| D11 | TMC2209 EN |
| D12 | 1k resistor to TMC2209 PDN_UART / PDN |
| A2 | same TMC2209 PDN_UART / PDN node |
| A1 | TMC2209 DIAG |

User-wired optional pedal jack:

| TRS jack contact | Arduino |
| --- | --- |
| Ring | GND |
| Sleeve | A3 |
| Tip | A4 |

`A3` and `A4` use the Arduino internal pullups. External resistors are not
required for normal use, though small series protection resistors are harmless.

## Controls

- `SELECT` toggles run/stop when no pedal is connected.
- `UP` and `DOWN` select speed presets.
- `RIGHT` requests forward direction.
- `LEFT` requests reverse direction.
- Direction changes while running ramp down, flip direction while stopped, then
  ramp back up.
- If a stall fault is latched, `SELECT` clears it.
- In either pedal mode, moving the pedal to its off position also clears a
  latched stall.
- If a pedal is connected, `SELECT` prints pedal ADC debug values to serial.

If UART communication with the TMC2209 is not detected at startup, the LCD
shows a blocking `TMC UART ERROR` message. Press `SELECT` to continue anyway;
after that, the first LCD line shows `Stop!` or `Run !` as a persistent
reminder that UART is offline.

## Pedal Modes

The firmware auto-detects three pedal states from the TRS jack:

- No pedal: both sense pins read pulled high.
- Switch pedal, TS on/off only: sleeve is grounded by the TS plug; tip closes
  to ground when pressed.
- Expression pedal, TRS with ring common: sleeve reads an analog position value.

In switch mode, the selected keypad speed is held stopped until the pedal is
pressed.

In expression mode, one end of travel is stopped and the other maps up to
300 RPM using a shaped curve with finer control near the low end. Set
`CONFIG_EXPRESSION_INVERT` if your pedal direction is reversed.

## Tuning

Default user-tunable values live in [src/config.h](src/config.h). For personal
tuning, copy [src/config_user.example.h](src/config_user.example.h) to
`src/config_user.h` and override only the values you want to change.
`src/config_user.h` is ignored by git.

`src/config_user.h` is included at the end of `src/config.h`. Override a
setting with `#undef CONFIG_NAME` followed by `#define CONFIG_NAME ...`. The
firmware then uses those final `CONFIG_*` values internally.

The most likely values to change are:

| Override macro | Purpose |
| --- | --- |
| `CONFIG_MICROSTEPS` | TMC2209 microstep mode and RPM math |
| `CONFIG_SPEED_PRESETS_RPM` | Keypad speed presets |
| `CONFIG_RUN_CURRENT_MA` | TMC2209 RMS current per speed preset |
| `CONFIG_STALL_GUARD_THRESHOLDS` | StallGuard sensitivity per speed preset |
| `CONFIG_STALL_DIAG_DEBOUNCE_MS` | How long DIAG must remain asserted before a stall |
| `CONFIG_TMC_DRIVER_TYPE` | Select `CONFIG_TMC_DRIVER_TMC2209` or `CONFIG_TMC_DRIVER_TMC2208` |
| `CONFIG_USE_TMC_UART` | Enable TMC UART setup and diagnostics |
| `CONFIG_USE_STALL_GUARD` | Enable DIAG/StallGuard stop detection |
| `CONFIG_SPEED_RAMP_RPM_PER_SECOND` | Ramp rate for speed and direction changes |
| `CONFIG_EXPRESSION_HEEL_ADC` / `CONFIG_EXPRESSION_TOE_ADC` | Expression pedal calibration |
| `CONFIG_EXPRESSION_INVERT` | Reverse expression pedal direction |
| `CONFIG_EXPRESSION_LOW_SPEED_POSITION` / `CONFIG_EXPRESSION_LOW_SPEED_RPM` | Low-speed pedal curve |
| `CONFIG_LCD_SHIELD_VERSION` | LCD keypad button ladder, `1` for older V1.0 or `2` for newer V1.1/V2-style shields |
| `CONFIG_LCD_BUTTON_*_ADC` | LCD keypad shield button thresholds |

Keep `CONFIG_SPEED_PRESETS_RPM`, `CONFIG_RUN_CURRENT_MA`,
`CONFIG_STALL_GUARD_THRESHOLDS`, and `CONFIG_STALL_DIAG_DEBOUNCE_MS` aligned by
column. For example, the first current, StallGuard threshold, and debounce value
all apply to the first RPM preset.

Higher `CONFIG_STALL_GUARD_THRESHOLDS` values are more sensitive. StallGuard is
ignored briefly after starts, speed changes, ramps, and direction changes to
avoid false trips from normal transients.

Set `CONFIG_USE_UART_CURRENT_CONTROL` to `false` if you want the TMC2209 Vref
potentiometer to control current while firmware still configures the rest of
the driver over UART.

Set `CONFIG_USE_TMC_UART` and `CONFIG_USE_STALL_GUARD` to `false` to run the
driver as a plain STEP/DIR module. The UART and DIAG wires can stay physically
connected; the firmware leaves the UART pins inactive and skips the missing-UART
screen and StallGuard polling. In that mode, current and microstepping come from
the driver module's standalone hardware settings instead of firmware, but
`CONFIG_MICROSTEPS` must still match the module's hardware microstep setting so
RPM math stays correct.

`CONFIG_MICROSTEPS` is still relevant even with UART enabled. The firmware
writes that value to the TMC2209 during setup and also uses it to calculate the
STEP pulse rate for a requested RPM.

## VS Code and PlatformIO

This project is designed to be built in VS Code using the PlatformIO IDE extension.

1. Install [Visual Studio Code](https://code.visualstudio.com/).
2. Install the `PlatformIO IDE` extension from the VS Code Extensions view.
3. Open this repository folder in VS Code.
4. Wait for PlatformIO to finish configuring the project.
5. Connect the Arduino-compatible board over USB.
6. In the PlatformIO sidebar, open the environment matching your driver:
   `uno_tmc2209`, `uno_tmc2208`, or `uno_stepdir`.
7. Run `General > Build` to compile.
8. Run `General > Upload` to flash the board.
9. Run `Platform > Monitor` to open the serial monitor at `115200` baud.

Equivalent CLI commands:

```sh
pio run -e uno_tmc2209
pio run -e uno_tmc2209 -t upload
pio device monitor -e uno_tmc2209
```

Replace `uno_tmc2209` with `uno_tmc2208` or `uno_stepdir` as needed.

The PlatformIO environments are defined in [platformio.ini](platformio.ini):

```ini
[env:uno_tmc2209]
build_flags =
  -DCONFIG_TMC_DRIVER_TYPE=CONFIG_TMC_DRIVER_TMC2209
  -DCONFIG_USE_TMC_UART=true
  -DCONFIG_USE_STALL_GUARD=true

[env:uno_tmc2208]
build_flags =
  -DCONFIG_TMC_DRIVER_TYPE=CONFIG_TMC_DRIVER_TMC2208
  -DCONFIG_USE_TMC_UART=true
  -DCONFIG_USE_STALL_GUARD=false

[env:uno_stepdir]
build_flags =
  -DCONFIG_USE_TMC_UART=false
  -DCONFIG_USE_STALL_GUARD=false
```

All environments share this base configuration:

```ini
[env]
platform = atmelavr
board = uno
framework = arduino
monitor_speed = 115200
```

## Project Layout

- [src/main.cpp](src/main.cpp) - setup/loop orchestration
- [src/config.h](src/config.h) - tracked default firmware configuration
- [src/config_user.example.h](src/config_user.example.h) - template for local overrides
- `src/config_user.h` - optional local overrides, ignored by git
- [src/motion.cpp](src/motion.cpp) - step timer, RPM math, ramping, direction changes
- [src/driver_control.cpp](src/driver_control.cpp) - TMC2209 UART setup and StallGuard polling
- [src/pedal.cpp](src/pedal.cpp) - pedal detection and expression mapping
- [src/ui_lcd.cpp](src/ui_lcd.cpp) - LCD/keypad UI
- [src/state.cpp](src/state.cpp) - shared runtime state
- [platformio.ini](platformio.ini) - PlatformIO board and library configuration
- [LICENSE](LICENSE) - CC BY-NC-SA 4.0
