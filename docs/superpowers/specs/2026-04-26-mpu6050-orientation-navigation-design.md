# MPU6050 Orientation Navigation Design

Date: 2026-04-26
Status: approved in chat

## Goal

Switch application pages using stable `MPU6050` accelerometer orientation only.

Target outcome:

- keep `HOME`, `TIMER_30MIN`, `CALENDAR`, and `TEST_MODE`
- stop using integrated `yaw` for page switching
- avoid page drift while the board is stationary
- keep the interaction based on four clear placements
- preserve the existing timer-page behavior when entering `TIMER_30MIN`

## Root Cause And Direction Change

The earlier orientation-navigation version relied on gyroscope integration to derive `yaw`.
That produced the exact failure the user reported: the board could remain still while the displayed angle kept changing, which then made page state unstable.

The user confirmed that the accelerometer readings are much more trustworthy for this product and that the intended interaction is not free-angle navigation. It is a four-position page selector.

Therefore the approved design changes from:

- gyro-based continuous heading

to:

- accelerometer-based discrete page classification

This design intentionally treats gyroscope drift as irrelevant for page routing by removing it from the decision path.

## User-Confirmed Orientation Mapping

The page mapping is fixed to the current sensor mounting and the user-confirmed measured directions:

- `ax ~= +1g`
  - `HOME`
- `ay ~= +1g`
  - `TIMER_30MIN`
- `ax ~= -1g`
  - `CALENDAR`
- `ay ~= -1g`
  - `TEST_MODE`

Interpretation:

- this mapping is already validated against the current board orientation
- implementation must use these exact axis/sign relationships unless the hardware is rewired later

## Driver And Data Source

The user explicitly requested the project use the root-level `ATK_MS6050` driver.

Recommended integration path:

1. keep `ATK_MS6050/*` as the actual low-level sensor driver
2. keep `HARDWARE/SENSOR/MPU6050.h` as the project-facing API already used by `USER/app_env_monitor.c`
3. implement `HARDWARE/SENSOR/MPU6050.c` as a thin compatibility wrapper around `ATK_MS6050`

This keeps application code stable while changing the sensor backend to the user-provided known-good driver.

## Bus And Wiring Constraint

Important existing constraint:

- `HARDWARE/SENSOR/App_I2C.*` already uses `PB10/PB11` for AHT20, BMP280, and DS3231
- the copied `ATK_MS6050` package also defaults to `PB10/PB11`

To avoid breaking the other sensors, the approved direction is:

- keep the existing environment-sensor bus unchanged
- retarget the `ATK_MS6050` software-I2C pins to the dedicated `MPU6050` wiring already used in this project

The exact pin text shown to the user in OLED or UART messages must match the final retargeted mapping after implementation.

## Orientation Classification Rules

Page switching uses only converted acceleration values from `MPU6050_ReadData()`:

- `accel_x_g`
- `accel_y_g`
- `accel_z_g`

Approved rules:

- `HOME`
  - `accel_x_g >= +0.90f`
  - `fabs(accel_y_g) <= 0.35f`
- `TIMER_30MIN`
  - `accel_y_g >= +0.90f`
  - `fabs(accel_x_g) <= 0.35f`
- `CALENDAR`
  - `accel_x_g <= -0.90f`
  - `fabs(accel_y_g) <= 0.35f`
- `TEST_MODE`
  - `accel_y_g <= -0.90f`
  - `fabs(accel_x_g) <= 0.35f`

If none of the four rules match:

- do not switch pages
- keep the current page
- treat the pose as in-transition or ambiguous

This keeps the classifier intentionally strict so diagonal or half-rotated placements do not cause flicker.

## Stability And Cooldown

The page router still needs debouncing, but it no longer needs angle smoothing.

Approved timing:

- candidate page stable time: about `200 ms`
- switch cooldown time: about `300 ms`

Behavior:

- a page candidate must remain unchanged for the full stable window before it is accepted
- after a real page switch, ignore further switches until the cooldown window expires
- if the pose becomes ambiguous, keep the current page and clear the current candidate

The `TIMER_30MIN` page keeps its current semantics:

- only when the page is actually entered should the 30-minute countdown restart

## Application Architecture

Keep the feature centered in `USER/app_env_monitor.c`.

Responsibilities after the redesign:

1. `ATK_MS6050/*`
   - low-level device communication
   - initialization
   - raw accel/gyro/temp reads

2. `HARDWARE/SENSOR/MPU6050.*`
   - project-facing wrapper API
   - data conversion into `MPU6050_Data_t`
   - error-code mapping

3. `USER/app_env_monitor.c`
   - page classification from accelerometer values
   - stability and cooldown timing
   - timer-page state
   - page rendering dispatch

## Logic To Remove

The following page-switching logic is no longer part of the approved design:

- `yaw_deg` as the routing source
- gyroscope Z bias calibration for page routing
- `gyro_z_dps` deadband logic for page routing
- angle normalization and angle-target snapping
- any page classifier based on `0 / 90 / 180 / -90` heading buckets

These parts should be removed or left only where strictly needed for compatibility, but they must not influence current-page selection.

## Test Page And UART Output

The test page remains available for sensor bring-up, but it should focus on acceleration rather than heading.

Approved direction:

- keep a compact engineer-facing test page
- show `AX`, `AY`, and `AZ`
- do not depend on `Y` or `GZ` for normal page routing

UART logging should also stop presenting `yaw` as the primary navigation value.
It may log:

- current page name
- `AX / AY / AZ`
- `MPU` error status when reads fail

## Error Handling

Expected failure cases:

- `ATK_MS6050` init failure
- device ID mismatch
- runtime read failure
- ambiguous pose that matches no approved page

Required behavior:

- on init or read failure, keep the current safe page instead of forcing a switch
- continue reporting `MPU ERR` for diagnosis
- ambiguous orientation must not be treated as an error; it simply means "do not change page"

## Non-Goals

This design does not aim to:

- compute precise Euler angles
- keep continuous `yaw` for navigation
- recognize dynamic gestures
- make page switching depend on gyroscope drift correction
- redesign unrelated OLED, RTC, AHT20, or BMP280 behavior

## Testing Plan

1. Driver validation
   - `ATK_MS6050` initializes successfully through the project wrapper
   - `WHO_AM_I` passes on the current board
   - accel readings update sensibly while the board is reoriented

2. Page-mapping validation
   - `ax ~= +1g` enters `HOME`
   - `ay ~= +1g` enters `TIMER_30MIN`
   - `ax ~= -1g` enters `CALENDAR`
   - `ay ~= -1g` enters `TEST_MODE`

3. Stability validation
   - slight hand shake near a target pose does not cause repeated switching
   - diagonal placement does not trigger false page changes
   - stationary placement does not drift across pages over time

4. Timer validation
   - entering `TIMER_30MIN` resets the countdown once
   - remaining on that pose does not repeatedly restart the timer

5. Failure validation
   - disconnected or unreadable MPU keeps the app on a safe page
   - OLED and UART still report `MPU` failure clearly
