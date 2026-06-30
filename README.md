# Rod Rotator

Arduino / PlatformIO firmware for a stepper-driven fishing rod rotator. The
current target is an Arduino Uno-class board with an LCD keypad shield and a
BIGTREETECH-style TMC2209 driver over STEP/DIR plus one-wire UART.

The firmware supports keypad speed presets, bidirectional ramped direction
changes, TMC2209 UART setup, StallGuard stop detection, and optional TRS pedal
control.

## Hardware

Tested around this setup:

- Arduino Uno-class board, currently an Inventr.io HERO
- LCD keypad shield using the common `LiquidCrystal lcd(8, 9, 4, 5, 6, 7)` pinout
- TMC2209 stepper driver
- NEMA 17 stepper motor
- 24 V motor supply recommended for higher RPM operation
- Isolated 1/4 in TRS jack for optional pedals

Do not feed motor supply voltage into the Arduino barrel jack. The Arduino,
driver logic, and motor supply must share ground.

## Wiring

| Arduino | Connects to |
| --- | --- |
| D2 | TMC2209 STEP |
| D3 | TMC2209 DIR |
| D11 | TMC2209 EN |
| D12 | 1k resistor to TMC2209 PDN_UART / PDN |
| A2 | same TMC2209 PDN_UART / PDN node |
| A1 | TMC2209 DIAG |
| A0 | LCD keypad buttons |
| D4-D9 | LCD keypad shield |

For the optional pedal jack:

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
- If a pedal is connected, `SELECT` prints pedal ADC debug values to serial.

The first LCD line shows `Stop!` or `Run !` when UART communication with the
TMC2209 is not detected.

## Pedal Modes

The firmware auto-detects three pedal states from the TRS jack:

- No pedal: both sense pins read pulled high.
- Switch pedal: sleeve is grounded by the TS plug; tip closes to ground when
  pressed.
- Expression pedal: sleeve reads an analog position value.

In switch mode, the selected keypad speed is held stopped until the pedal is
pressed.

In expression mode, heel-down is stopped and toe-down maps up to 300 RPM using
a shaped curve with finer control near the low end.

## Tuning

Most user-tunable values live near the top of [src/main.cpp](src/main.cpp) in
the `User configuration` section.

The most likely values to change are:

| Setting | Purpose |
| --- | --- |
| `MICROSTEPS` | TMC2209 microstep mode and RPM math |
| `SPEED_PRESETS_RPM[]` | Keypad speed presets |
| `RUN_CURRENT_MA[]` | TMC2209 RMS current per speed preset |
| `STALL_GUARD_THRESHOLDS[]` | StallGuard sensitivity per speed preset |
| `STALL_DIAG_DEBOUNCE_MS[]` | How long DIAG must remain asserted before a stall |
| `SPEED_RAMP_RPM_PER_SECOND` | Ramp rate for speed and direction changes |
| `EXPRESSION_HEEL_ADC` / `EXPRESSION_TOE_ADC` | Expression pedal calibration |
| `EXPRESSION_LOW_SPEED_POSITION` / `EXPRESSION_LOW_SPEED_RPM` | Low-speed pedal curve |
| `LCD_SHIELD_VERSION` | LCD keypad button ladder, `1` for older V1.0 or `2` for newer V1.1/V2-style shields |
| `LCD_BUTTON_*_ADC` | LCD keypad shield button thresholds |

Keep `SPEED_PRESETS_RPM[]`, `RUN_CURRENT_MA[]`, `STALL_GUARD_THRESHOLDS[]`, and
`STALL_DIAG_DEBOUNCE_MS[]` aligned by column. For example, the first current,
StallGuard threshold, and debounce value all apply to the first RPM preset.

Higher `STALL_GUARD_THRESHOLDS[]` values are more sensitive. StallGuard is
ignored briefly after starts, speed changes, ramps, and direction changes to
avoid false trips from normal transients.

Set `USE_UART_CURRENT_CONTROL` to `false` if you want the TMC2209 Vref
potentiometer to control current while firmware still configures the rest of
the driver over UART.

`MICROSTEPS` is still relevant even with UART enabled. The firmware writes that
value to the TMC2209 during setup and also uses it to calculate the STEP pulse
rate for a requested RPM.

## PlatformIO

Build:

```sh
pio run
```

Upload:

```sh
pio run -t upload
```

Serial monitor:

```sh
pio device monitor
```

## Project Layout

- [src/main.cpp](src/main.cpp) - firmware
- [platformio.ini](platformio.ini) - PlatformIO board and library configuration
- [LICENSE](LICENSE) - CC BY-NC-SA 4.0
