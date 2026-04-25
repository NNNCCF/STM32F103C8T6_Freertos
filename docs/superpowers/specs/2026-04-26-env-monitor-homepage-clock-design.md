# Env Monitor Homepage Clock Design

Date: 2026-04-26
Status: approved in chat

## Goal

Redesign the OLED home screen so the primary visual is a large `HH:MM:SS` clock sourced from the newly added `DS3231`, with the lower half of the screen divided into four compact status regions for temperature, humidity, pressure, and altitude.

Target outcome:

- show `DS3231` time as large text in the upper-middle of the `128x64` OLED
- keep the current AHT20 and BMP280 sensor acquisition path
- compute altitude from BMP280 pressure using a fixed sea-level reference
- render the page without blank-frame flicker
- preserve the current thin `main.c` and keep page logic inside `USER/app_env_monitor.c`

## Current Project Context

The current firmware already has:

- `OLED` on dedicated `I2C1(PB6/PB7)`
- `AHT20` and `BMP280` on `App_I2C` using `I2C2(PB10/PB11)`
- a FreeRTOS environment monitor task in `USER/app_env_monitor.c`
- a new `HARDWARE/DS3231` driver that also uses `App_I2C`

The main gap is integration:

- `DS3231` is not yet shown on the OLED home page
- the home page still uses the older list-style layout
- `DS3231` may still need to be added to the Keil project file if it is not yet part of the build

## Screen Layout

Screen size is fixed at `128x64`.

Use this layout:

- time string:
  - format: `HH:MM:SS`
  - font: `24`
  - position: horizontally centered, `x=16`, `y=4`
  - width estimate: `8 chars * 12 px = 96 px`
- horizontal divider 1:
  - `y=31`
- horizontal divider 2:
  - `y=47`
- vertical divider:
  - `x=64`
  - from `y=31` to `y=63`

This creates a clear time area on top and four compact data areas below:

- top-left: temperature
- top-right: humidity
- bottom-left: pressure
- bottom-right: altitude

## Widget Content

Each lower cell must fit in one compact line because the available height per row is only about `15-16 px`.

Recommended compact text format:

- temperature: `T25.3C`
- humidity: `H56.1%`
- pressure: `P1013h`
- altitude: `A123.4m`

Formatting rules:

- use `12`-pixel font in the four lower cells
- keep values short enough to fit in a `64 px` wide cell
- prefer one decimal for temperature, humidity, and altitude
- pressure is shown in `hPa`-scaled compact form and may be rounded to integer display precision for readability

## Data Sources

- `DS3231`
  - source for `HH:MM:SS`
  - polled through `DS3231_GetTime`
- `AHT20`
  - source for temperature and humidity
- `BMP280`
  - source for pressure
  - source for altitude after conversion

Altitude calculation:

- use standard atmosphere conversion from pressure
- reference sea-level pressure constant:
  - `101325.0f Pa`
- formula:
  - `altitude_m = 44330.0f * (1.0f - powf(pressure_pa / sea_level_pa, 0.1903f))`

This keeps the implementation simple and adequate for a dashboard display. A configurable calibration reference can be added later if needed.

## Refresh Strategy

The display should not be refreshed on a blind fixed-rate loop when nothing changed.

Use this behavior:

- poll `DS3231` on a short cadence such as `200 ms`
- poll AHT20 and BMP280 on a slower cadence such as `1000 ms`
- rebuild the OLED framebuffer in RAM first
- call `OLED_Refresh()` once per completed frame
- refresh only when:
  - the displayed second changed
  - a sensor sample changed
  - an error state changed

This reduces bus traffic and avoids the visible flash that happens when a blank frame is pushed before the final frame.

## Application Structure

Keep `USER/main.c` as startup-only code.

Keep the home page feature in `USER/app_env_monitor.c`, but split responsibilities more clearly:

- time update helper
- sensor update helper
- altitude calculation helper
- compact text formatting helpers
- home page render helper
- UART logging helper

No separate UI task is required. The existing environment monitor task remains the owner of both data collection and page rendering.

## Error Handling

If a device is not ready:

- time area shows `--:--:--` when `DS3231` is unavailable
- temperature cell shows `T ERR`
- humidity cell shows `H ERR`
- pressure cell shows `P ERR`
- altitude cell shows `A ERR`

Behavior expectations:

- one failing device must not block the rest of the page
- `DS3231` failure must not stop sensor rendering
- sensor failure must not stop time rendering
- UART logging should include enough status to confirm which source failed

## Implementation Scope

Expected touched files:

- Modify: `USER/app_env_monitor.c`
- Modify: `USER/app_env_monitor.h` if the app interface grows
- Modify: `USER/Template.uvprojx` if `DS3231.c` is not yet included in the Keil target
- Review: `HARDWARE/DS3231/DS3231.h`
- Review: `HARDWARE/DS3231/DS3231.c`
- Reuse without API changes:
  - `HARDWARE/OLED/oled.c`
  - `HARDWARE/OLED/oled.h`
  - `HARDWARE/SENSOR/AHT20.c`
  - `HARDWARE/SENSOR/BMP280.c`

## Testing Plan

Validation should cover layout correctness and runtime behavior:

1. Build validation
   - Keil rebuild completes successfully

2. Time validation
   - OLED shows a centered large `HH:MM:SS`
   - seconds tick correctly once per second

3. Layout validation
   - two horizontal lines and one center vertical line are drawn at the intended coordinates
   - each metric stays inside its cell without overlap

4. Sensor validation
   - temperature, humidity, and pressure update correctly
   - altitude changes consistently with pressure changes

5. Error-state validation
   - missing `DS3231` still leaves lower sensor cells visible
   - missing sensors still leaves time visible

## Non-Goals

This design does not aim to:

- add date display to the home page
- add page switching or menu navigation
- calibrate altitude against a user-configurable local pressure reference
- redesign the OLED transport layer again
- introduce DMA as part of this page-layout change
