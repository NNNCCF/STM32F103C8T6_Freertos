# MPU6050 Orientation Navigation Design

Date: 2026-04-26
Status: approved in chat

## Goal

Add an `MPU6050` driver and use static device orientation to switch application pages.

Target outcome:

- read `MPU6050` acceleration data reliably
- treat the normal flat, upright placement as the default home orientation
- switch pages based on stable static orientation rather than rotation gestures
- avoid false triggers with stability timing and cooldown rules
- keep the design lightweight by not requiring DMP for the first version

## User Interaction Model

The device is used as an orientation-controlled navigator.

Page mapping:

- normal flat upright placement:
  - `HOME`
- clockwise 90 degrees:
  - `TIMER_30MIN`
- upside down 180 degrees:
  - `CALENDAR`
- counterclockwise 90 degrees:
  - `TEST_MODE`

The agreed interaction style is:

- static orientation based
- not gesture based
- the target page changes only after the device remains stable in that orientation long enough

## Current Project Context

The current project already has:

- a FreeRTOS application task in `USER/app_env_monitor.c`
- an OLED home page driven by that task
- a shared hardware-I2C sensor helper in `HARDWARE/SENSOR/App_I2C.*`
- AHT20 and BMP280 sensor drivers using that helper
- no active `MPU6050` source files in the repository at the moment

The Keil project still contains old include-path traces for `ATK_MS6050`, but the actual driver sources are not present. This means the cleanest approach is to add a small fresh `MPU6050` driver that matches the existing sensor-driver style instead of reviving the older DMP package.

## Recommended Approach

Three possible approaches:

1. Recommended: basic MPU6050 driver + accelerometer-based orientation classifier
   - fastest path to a stable page-navigation feature
   - no DMP dependency
   - enough for static orientation control

2. Basic driver + accel/gyro fusion
   - more robust under motion
   - more code and tuning effort

3. DMP-based attitude angles
   - richest data
   - much heavier integration burden than this interaction needs

Recommendation:

- implement a basic `MPU6050` driver first
- use accelerometer gravity direction for page selection
- leave DMP as a future enhancement if needed later

## Sensor Strategy

The first version only needs accelerometer data for page routing.

Driver scope:

- device identity check using `WHO_AM_I`
- wake-up and configuration
- raw accelerometer readout
- raw gyro and temperature read helpers may be exposed for completeness, but they are not required for the initial page-routing logic

Configuration target:

- accelerometer full scale: `+-2g`
- gyroscope full scale: conservative default such as `+-500 dps`
- low-pass filter and sample rate set to stable default values

## Bus And Wiring Assumption

Initial implementation should follow the existing sensor-driver style and target `HARDWARE/SENSOR/App_I2C.*`.

Assumption for the first version:

- `MPU6050` uses the same helper pattern as the other sensors
- the first implementation targets the shared sensor bus helper on `I2C2(PB10/PB11)`

Important note:

- this design assumes the final hardware can place `MPU6050` on that sensor bus
- if the final board wiring cannot share `PB10/PB11`, only the low-level transport should change; the higher-level orientation classifier and page-state machine should remain untouched

The first version does not require an interrupt pin because it uses polling rather than DMP/FIFO interrupt handling.

## Orientation Classification

The classifier should only accept orientation changes when the device is still flat enough for gravity-based interpretation.

Orientation gating:

- `az` must stay strongly positive to confirm the screen is facing upward
- if the board is tilted too far out of plane, keep the current page instead of forcing a new classification

Initial thresholds:

- flat-screen-up gate:
  - `az > 0.75g`
- horizontal orientation confidence:
  - use `ax` and `ay` signs and magnitude once the flat gate is satisfied
- page-direction threshold:
  - dominant in-plane axis should exceed about `0.60g`

Nominal direction rules:

- `HOME`
  - upright baseline orientation
- `TIMER_30MIN`
  - right-side dominant orientation
- `CALENDAR`
  - inverted orientation relative to home
- `TEST_MODE`
  - left-side dominant orientation

The exact sign mapping for `ax` and `ay` depends on the final mounting direction of the sensor on the board. The implementation must isolate this mapping into one conversion function so it can be corrected with minimal code changes if physical axis direction differs from the expected board orientation.

## Stability And Cooldown

The page router must not react instantly to transient movement.

Use these rules:

- candidate page must remain unchanged for about `600-800 ms`
- after a confirmed page switch, apply about `1000 ms` cooldown before another switch
- when readings fall into an ambiguous threshold zone, keep the current page

This gives a stable appliance-like interaction instead of a twitchy motion-controlled interface.

## Application Architecture

Keep `USER/main.c` thin.

Add a small MPU6050 module and integrate orientation handling into the application task layer.

Proposed responsibilities:

1. `HARDWARE/SENSOR/MPU6050.*`
   - low-level device driver
   - initialization
   - raw data reads

2. `USER/app_env_monitor.c`
   - orientation classification
   - stability timer and cooldown logic
   - current page state
   - page rendering dispatch

3. page renderers
   - `HOME`
   - `TIMER_30MIN`
   - `CALENDAR`
   - `TEST_MODE`

The first version can keep all routing logic in `app_env_monitor.c` if that stays manageable, but the orientation classifier should be kept in focused helper functions so the file does not become unreadable.

## Error Handling

Expected failure surfaces:

- `MPU6050` not detected
- I2C read failure
- unstable or ambiguous orientation

Handling approach:

- if `MPU6050` init fails, stay on the current default page and print a UART error
- if runtime reads fail, keep the last confirmed page
- ambiguous orientations should never trigger a page switch

## Testing Plan

Validation should cover both the driver and the navigation behavior:

1. Build validation
   - Keil rebuild succeeds with the new driver

2. Driver validation
   - `WHO_AM_I` passes
   - raw accel values change consistently when the board is rotated

3. Orientation validation
   - flat upright position enters `HOME`
   - 90-degree clockwise position enters `TIMER_30MIN`
   - 180-degree inverted position enters `CALENDAR`
   - 90-degree counterclockwise position enters `TEST_MODE`

4. Stability validation
   - brief movement does not cause page flutter
   - repeated small shakes near thresholds do not trigger repeated page switches

5. Failure validation
   - unplugged or failing MPU6050 leaves the application on a safe page and reports the issue on UART

## Non-Goals

This design does not aim to:

- add DMP in the first implementation
- recognize dynamic gestures
- derive precise Euler angles for display
- require an interrupt-driven MPU6050 pipeline
