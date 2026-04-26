# Serial Time Set Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a UART command that sets DS3231 time using `TIME=YYYY-MM-DD HH:MM:SS`.

**Architecture:** Reuse the existing USART interrupt line buffer, parse the command in `app_env_monitor.c`, validate it there, then call `DS3231_SetTime` and `DS3231_GetTime` from the monitor task so all RTC and display state stays in one place.

**Tech Stack:** STM32F103C8, FreeRTOS, USART1 interrupt RX, DS3231 soft I2C, Keil ARMCC5.

---

### Task 1: Add UART command parsing helpers

**Files:**
- Modify: `USER/app_env_monitor.c`

- [ ] Add helpers that copy a completed line from `USART_RX_BUF`, parse fixed-width decimal fields, validate date/time ranges, and compute weekday.
- [ ] Keep the accepted input format fixed to `TIME=YYYY-MM-DD HH:MM:SS`.

### Task 2: Handle RTC set commands in the monitor task

**Files:**
- Modify: `USER/app_env_monitor.c`

- [ ] Add a command handler that processes one UART line per loop.
- [ ] On success, call `DS3231_SetTime`, then `DS3231_GetTime`, update context, and print `TIME OK ...`.
- [ ] On failure, print one of `TIME ERR FORMAT`, `TIME ERR RANGE`, or `TIME ERR RTC`.

### Task 3: Integrate command handling into the main update loop

**Files:**
- Modify: `USER/app_env_monitor.c`

- [ ] Run UART command handling before normal RTC polling.
- [ ] Treat a successful time set as a display-dirty event so the OLED updates immediately.

### Task 4: Build and verify

**Files:**
- Review: `OBJ/Template.build_log.htm`

- [ ] Rebuild with Keil and confirm `0 Error(s), 0 Warning(s)`.
- [ ] Verify the valid command updates both UART output and OLED time.
