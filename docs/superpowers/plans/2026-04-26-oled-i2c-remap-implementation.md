# OLED I2C Remap Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move OLED to `I2C1(PB6/PB7)` and move AHT20/BMP280 to `I2C2(PB10/PB11)` while keeping the current environment monitor behavior.

**Architecture:** Keep application logic in `USER/app_env_monitor.c`, keep `main.c` thin, replace the OLED software-I2C transport with a dedicated hardware-I2C implementation on `I2C1`, and retarget the shared sensor bus helper to `I2C2`.

**Tech Stack:** STM32F103C8, StdPeriph I2C, FreeRTOS, Keil ARMCC5, SSD1306-style OLED API, AHT20, BMP280.

---

### Task 1: Remap the sensor bus helper to I2C2

**Files:**
- Modify: `HARDWARE/SENSOR/App_I2C.h`
- Modify: `HARDWARE/SENSOR/App_I2C.c`

- [ ] Change the `App_I2C` constants from `I2C1/PB6/PB7` to `I2C2/PB10/PB11`.
- [ ] Keep the public helper API stable so `AHT20.c` and `BMP280.c` do not need logic changes.
- [ ] Preserve the existing blocking read/write behavior and timeout handling.

### Task 2: Replace OLED software I2C with hardware I2C1

**Files:**
- Modify: `HARDWARE/OLED/oled.h`
- Modify: `HARDWARE/OLED/oled.c`

- [ ] Replace `PA0/PA1` software-I2C definitions with `PB6/PB7` hardware-I2C configuration.
- [ ] Keep `PA2` as the OLED reset pin.
- [ ] Preserve the current public drawing API so higher-level code compiles unchanged.
- [ ] Rework `OLED_WR_Byte` and `OLED_Refresh` to transmit through `I2C1`.
- [ ] Keep the framebuffer model based on `OLED_GRAM`.

### Task 3: Keep startup and app structure stable

**Files:**
- Review: `USER/main.c`
- Review: `USER/app_env_monitor.c`

- [ ] Ensure no application-layer code assumes the old OLED pinout.
- [ ] Keep `main.c` as startup-only code.
- [ ] Keep `app_env_monitor.c` as the owner of the environment display loop.

### Task 4: Build and verify

**Files:**
- Review: `USER/Template.uvprojx`
- Review: `OBJ/Template.build_log.htm`

- [ ] Run a full Keil rebuild.
- [ ] Confirm `0 Error(s), 0 Warning(s)`.
- [ ] Confirm the remapped modules are included in the build output.
