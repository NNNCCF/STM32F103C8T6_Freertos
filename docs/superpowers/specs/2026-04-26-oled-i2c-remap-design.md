# OLED I2C Remap Design

Date: 2026-04-26
Status: approved in chat, pending user review of this written spec

## Goal

Reassign the OLED to `PB6/PB7` and move the environmental sensors off that bus so the display has an independent hardware I2C path. Preserve the current application behavior: AHT20 and BMP280 data continue to be sampled and shown on the OLED, while the code structure stays clean enough for later display-performance upgrades.

Target outcome:

- OLED uses `I2C1` on `PB6=SCL`, `PB7=SDA`
- OLED reset remains on `PA2`
- AHT20 and BMP280 use `I2C2` on `PB10=SCL`, `PB11=SDA`
- `main.c` stays thin and only handles startup
- application code keeps using existing high-level OLED APIs

## Hardware Mapping

Use these fixed assignments after the change:

- OLED:
  - `PB6`: `I2C1_SCL`
  - `PB7`: `I2C1_SDA`
  - `PA2`: `RES`
- Sensors:
  - `PB10`: `I2C2_SCL`
  - `PB11`: `I2C2_SDA`
- Keep existing serial and LED pins unchanged

Rationale:

- `PB6/PB7` matches the user's desired OLED wiring
- moving sensors to `PB10/PB11` avoids bus sharing because the user does not have a breakout arrangement for a shared bus
- keeping `PA2` as OLED reset minimizes hardware disturbance on an already working display path

## Architecture

The change is split into three layers:

1. `HARDWARE/OLED`
   Replace the current software-I2C transport with a hardware-I2C transport on `I2C1`.

2. `HARDWARE/SENSOR/App_I2C`
   Reconfigure the sensor bus helper to `I2C2` on `PB10/PB11` while keeping the AHT20/BMP280 driver interfaces stable.

3. `USER/app_env_monitor.*`
   Leave application logic mostly unchanged so the bus migration stays localized to the transport layer.

## OLED Driver Strategy

The current OLED driver bit-bangs `PA0/PA1`, which is the main reason the display path is hard to accelerate. This change moves the OLED to hardware `I2C1`.

Design choices:

- keep the public drawing APIs stable:
  - `OLED_Init`
  - `OLED_Clear`
  - `OLED_ShowString`
  - `OLED_Refresh`
- change only the low-level write path and initialization pins
- preserve the existing framebuffer model using `OLED_GRAM`

Refresh behavior:

- `OLED_Refresh` will still push the framebuffer page by page
- transport becomes hardware I2C instead of GPIO bit-banging
- this should already provide a clear refresh-rate improvement with lower CPU waste

DMA note:

- the immediate goal is the hardware-I2C migration, because that removes the biggest current bottleneck with the least wiring and application churn
- code organization should keep the transport logic isolated so a later DMA-backed transmit path can be added without changing the application layer again
- DMA is therefore considered a follow-up optimization path, not a required first-pass behavior of this migration

## Sensor Driver Strategy

Keep AHT20 and BMP280 on a dedicated sensor bus.

Changes:

- `App_I2C` uses `I2C2`
- pins move to `PB10/PB11`
- AHT20 and BMP280 drivers continue to call the same helper API
- no change to measurement formulas or application-visible driver return codes

This keeps sensor behavior stable while freeing `PB6/PB7` for the OLED.

## Application Design

`main.c` remains a startup file only:

- board init
- UART init
- LED init
- create environment monitor task
- start scheduler

`app_env_monitor.c` remains the feature owner:

- initialize and poll AHT20
- initialize and poll BMP280
- format text
- draw text to the OLED
- print telemetry to UART

This preserves the cleaner structure already introduced and avoids mixing startup code with sensor/display logic again.

## Error Handling

Expected failure surfaces:

- OLED hardware-I2C init or write failure
- sensor bus init failure
- AHT20 read/init failure
- BMP280 ID/read failure

Handling approach:

- preserve current sensor error reporting on OLED and UART
- if task creation fails, keep the existing LED blink fallback
- if OLED transport migration introduces display failures, keep the API contract stable so failures can be isolated to the transport layer

## Testing Plan

Validation should cover both migration correctness and functional behavior:

1. Build validation
   - Keil project compiles with the new OLED transport and remapped sensor bus

2. Wiring validation
   - OLED lights and shows the environment page using `PB6/PB7`
   - sensors still respond on `PB10/PB11`

3. Functional validation
   - UART prints changing temperature, humidity, and pressure values
   - OLED shows the same values
   - moving or warming the sensors changes readings as expected

4. Regression validation
   - scheduler still starts normally
   - no regression in board LED or UART startup

## Non-Goals

This change does not aim to:

- move the OLED to SPI
- redesign the app feature set
- share OLED and sensors on one bus
- promise a full DMA transmit implementation in the same migration if the hardware-I2C remap already solves the immediate bottleneck cleanly

## Implementation Scope

Expected touched files:

- Modify: `HARDWARE/OLED/oled.h`
- Modify: `HARDWARE/OLED/oled.c`
- Modify: `HARDWARE/SENSOR/App_I2C.h`
- Modify: `HARDWARE/SENSOR/App_I2C.c`
- Possibly modify: `USER/Template.uvprojx`
- Keep startup/application structure in:
  - `USER/main.c`
  - `USER/app_env_monitor.c`

## Constraints And Assumptions

- User will rewire OLED to `PB6/PB7` and keep OLED reset on `PA2`
- User will rewire AHT20/BMP280 to `PB10/PB11`
- Project remains on STM32F103 + StdPeriph + FreeRTOS + Keil ARMCC5
- No automatic git commit is possible here unless the project is later placed inside a git repository
