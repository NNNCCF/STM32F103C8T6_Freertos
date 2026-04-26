# Serial Time Set Design

Date: 2026-04-26
Status: approved in chat

## Goal

Add a UART command that lets the user set `DS3231` time at runtime without recompiling firmware.

Target outcome:

- accept `TIME=YYYY-MM-DD HH:MM:SS` over `USART1`
- validate command format and date/time ranges
- calculate weekday automatically before writing `DS3231`
- reply over UART with a clear success or error message
- refresh the OLED clock on the next task cycle using the new time

## Current Project Context

The project already has:

- `USART1` interrupt-driven line reception using `USART_RX_BUF` and `USART_RX_STA`
- `DS3231_SetTime` and `DS3231_GetTime`
- an environment monitor FreeRTOS task that already owns RTC polling and OLED rendering

This makes the monitor task the best place to parse and execute the time-set command.

## Command Format

Supported command:

- `TIME=2026-04-26 09:30:00`

Behavior:

- command must be sent as one line ending with `CRLF`
- only the exact uppercase `TIME=` prefix is accepted
- year range is `2000-2099`

## Validation Rules

Format validation:

- fixed separators at:
  - `-` after year and month
  - space between date and time
  - `:` between hour/minute and minute/second
- all numeric fields must contain digits only

Range validation:

- month: `1-12`
- day: valid for the selected month
- leap year must be handled for February
- hour: `0-23`
- minute: `0-59`
- second: `0-59`

Weekday handling:

- compute automatically from the date
- store `1-7` in the `DS3231_Time_t.week` field

## Responses

Successful set:

- `TIME OK 2026-04-26 09:30:00`

Failure responses:

- `TIME ERR FORMAT`
- `TIME ERR RANGE`
- `TIME ERR RTC`

## Architecture

Keep the existing UART interrupt receiver unchanged.

Add command handling to `USER/app_env_monitor.c`:

- copy a completed line from `USART_RX_BUF`
- parse and validate `TIME=...`
- initialize RTC bus if needed
- call `DS3231_SetTime`
- read back time with `DS3231_GetTime`
- mark the display loop dirty so the OLED updates immediately

This keeps the feature localized and avoids parsing strings or touching I2C from the UART ISR.

## Files

Expected touched files:

- Modify: `USER/app_env_monitor.c`
- Reuse: `SYSTEM/usart/usart.h`
- Reuse: `SYSTEM/usart/usart.c`
- Reuse: `HARDWARE/DS3231/DS3231.c`

## Testing Plan

1. Build validation
   - Keil rebuild succeeds

2. Success-path validation
   - send `TIME=2026-04-26 09:30:00`
   - UART replies `TIME OK 2026-04-26 09:30:00`
   - OLED clock updates to the new time

3. Format validation
   - send malformed input such as `TIME=2026/04/26 09:30:00`
   - UART replies `TIME ERR FORMAT`

4. Range validation
   - send invalid dates such as `TIME=2026-02-30 09:30:00`
   - UART replies `TIME ERR RANGE`

5. RTC failure validation
   - disconnect `DS3231` and send a valid command
   - UART replies `TIME ERR RTC`
