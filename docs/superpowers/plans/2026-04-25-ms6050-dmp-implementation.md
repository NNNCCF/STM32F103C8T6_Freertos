# MS6050 DMP Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port the existing `ATK_MS6050 + eMPL` package to this `STM32F103C8 + StdPeriph + FreeRTOS` project and print continuous `pitch / roll / yaw` on `USART1`.

**Architecture:** Keep the InvenSense DMP/eMPL algorithm files mostly intact and replace only the project-coupled parts: GPIO, software I2C, time base, interrupt wait, and application-facing wrapper. Run sampling and UART output in separate FreeRTOS tasks so DMP timing stays stable while serial printing remains slow and readable.

**Tech Stack:** STM32F10x StdPeriph, FreeRTOS V11.3.0, ARMCC5, software I2C, USART1, InvenSense eMPL/DMP.

---

### Task 1: Rewrite the board adaptation layer

**Files:**
- Modify: `HARDWARE/ATK_MS6050/atk_ms6050.h`
- Modify: `HARDWARE/ATK_MS6050/atk_ms6050.c`
- Modify: `HARDWARE/ATK_MS6050/atk_ms6050_iic.h`
- Modify: `HARDWARE/ATK_MS6050/atk_ms6050_iic.c`

- [ ] Replace HAL-specific GPIO macros with StdPeriph-compatible GPIO, RCC, EXTI, and bit-band access.
- [ ] Keep the approved pin mapping fixed as `PB10=SCL`, `PB11=SDA`, `PB12=INT`, `PC0=AD0`.
- [ ] Implement software I2C with explicit SDA input/output switching so ACK reads are reliable on F103.
- [ ] Add device init helpers for AD0 output, INT input, and basic status reads.
- [ ] Keep the low-level read/write API names stable so eMPL can reuse them unchanged.

### Task 2: Adapt eMPL platform hooks

**Files:**
- Modify: `HARDWARE/ATK_MS6050/eMPL/inv_mpu.c`
- Modify: `HARDWARE/ATK_MS6050/eMPL/inv_mpu_dmp_motion_driver.c`
- Modify: `HARDWARE/ATK_MS6050/eMPL/inv_mpu.h`

- [ ] Fix include paths so they reference the current project layout instead of `./BSP/...`.
- [ ] Keep `MPU6050` and `MOTION_DRIVER_TARGET_MSP430` selected inside headers/source so no `uvprojx` macro edit is required.
- [ ] Replace `HAL_GetTick()` with a project-owned millisecond time source that works before and after the scheduler starts.
- [ ] Leave the DMP math and firmware image unchanged unless compilation or runtime correctness requires a minimal fix.
- [ ] Preserve the existing DMP helper functions for init, self-test, and quaternion-to-Euler conversion.

### Task 3: Add an application-facing sensor wrapper

**Files:**
- Create: `HARDWARE/ATK_MS6050/atk_ms6050_app.h`
- Create: `HARDWARE/ATK_MS6050/atk_ms6050_app.c`

- [ ] Add a small wrapper that exposes `init`, `wait/read`, `get latest attitude`, and `status/error` APIs.
- [ ] Store the latest `pitch / roll / yaw` plus timestamp and init state in one shared struct.
- [ ] Protect shared state with a short FreeRTOS critical section so the print task reads a consistent snapshot.
- [ ] Convert raw DMP failures into clear module error states such as `ID_FAIL`, `I2C_FAIL`, `DMP_FAIL`, `FIFO_FAIL`.

### Task 4: Create FreeRTOS tasks and startup flow

**Files:**
- Modify: `USER/main.c`

- [ ] Restore system startup for a FreeRTOS application: priority grouping, delay init, USART init, LED init, task creation, scheduler start.
- [ ] Create a sensor task that initializes MS6050, runs DMP acquisition, and refreshes the shared attitude result.
- [ ] Create a UART task that prints formatted attitude data at about `10Hz`.
- [ ] Keep printing slower than sampling so the UART never becomes the timing bottleneck.
- [ ] Use the LED only as a small state indicator if needed, not as the primary data path.

### Task 5: Verify integration and list manual Keil imports

**Files:**
- Review: `USER/Template.uvprojx`
- Review: build output and dependency list

- [ ] Check that no automatic `uvprojx` edit was made during implementation.
- [ ] Confirm which source files are already present in the Keil project and which new MS6050 files still need manual import.
- [ ] Confirm which include paths are still missing for `ATK_MS6050` and `eMPL`.
- [ ] Summarize any remaining hardware-side assumptions for the user before final handoff.
