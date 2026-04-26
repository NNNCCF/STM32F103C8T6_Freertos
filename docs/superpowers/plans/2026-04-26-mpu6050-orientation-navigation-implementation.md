# MPU6050 Orientation Navigation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a basic MPU6050 driver and use its relative heading to switch between home, timer, calendar, and test pages.

**Architecture:** Create a focused `HARDWARE/SENSOR/MPU6050.*` module using a dedicated software-I2C transport on free GPIOs, then integrate relative yaw estimation, stability timing, and page routing into `USER/app_env_monitor.c` while keeping `main.c` unchanged.

**Tech Stack:** STM32F103C8, StdPeriph GPIO, FreeRTOS, ARMCC5, OLED framebuffer rendering, DS3231, AHT20, BMP280, MPU6050.

---

### Task 1: Add a basic MPU6050 driver

**Files:**
- Create: `HARDWARE/SENSOR/MPU6050.h`
- Create: `HARDWARE/SENSOR/MPU6050.c`
- Modify: `USER/Template.uvprojx`

- [ ] Create a driver that supports `WHO_AM_I`, reset/wake-up, default configuration, and a single-shot `ReadData` API that returns accel, gyro, and temperature.
- [ ] Use a dedicated software-I2C transport on free GPIOs so the new sensor does not depend on the existing sensor bus helper.
- [ ] Add `MPU6050.c` to the Keil project hardware group.

### Task 2: Add orientation-state logic to the app task

**Files:**
- Modify: `USER/app_env_monitor.c`

- [ ] Extend the app context with MPU6050 state, relative yaw, page enums, stability timing, and timer-page state.
- [ ] Calibrate gyroscope Z bias at startup while the device is expected to be flat in the home orientation.
- [ ] Integrate gyro Z over time to produce a relative heading and normalize it for page selection.
- [ ] Add candidate-page debouncing and cooldown logic before confirming page switches.

### Task 3: Render multiple pages

**Files:**
- Modify: `USER/app_env_monitor.c`

- [ ] Keep the existing environment-monitor screen as `HOME`.
- [ ] Add a `TIMER_30MIN` page that starts a 30-minute countdown when entered.
- [ ] Add a `CALENDAR` page that shows DS3231 date and weekday.
- [ ] Add a `TEST_MODE` page that shows MPU6050 debug data such as heading and raw/converted values.
- [ ] Add a page-render dispatcher so the correct screen is refreshed based on current page state.

### Task 4: Verify build and runtime assumptions

**Files:**
- Review: `OBJ/Template.build_log.htm`

- [ ] Rebuild with Keil and confirm `0 Error(s), 0 Warning(s)`.
- [ ] Verify that page switching works after stable placement at `0/90/180/270` relative orientations.
- [ ] Summarize the default MPU6050 pin mapping and the sign macro to adjust if clockwise/counterclockwise pages are reversed on hardware.
