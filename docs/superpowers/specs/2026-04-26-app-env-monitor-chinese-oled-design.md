# App Env Monitor Chinese OLED Design

Date: 2026-04-26
Status: approved in chat

## Goal

Change every OLED page rendered by `USER/app_env_monitor.c` from English UI copy to Chinese UI copy, while keeping numbers, time strings, and a small set of technical abbreviations in ASCII.

Target outcome:

- keep the existing page-switching and sensor-update behavior
- localize fixed OLED labels and prompts into Chinese
- preserve numeric readability for time, sensor values, countdowns, and diagnostics
- avoid broad font-driver changes for a small application-level copy update
- keep the main implementation focused in `USER/app_env_monitor.c`

## Current Project Context

The project already has:

- page rendering centralized in `USER/app_env_monitor.c`
- ASCII text rendering through `OLED_ShowString`
- point-based Chinese glyph rendering through `OLED_ShowChinese`
- a limited built-in Chinese font table in `HARDWARE/OLED/oledfont.h`

The main limitation is that the shared OLED font table does not contain all of the Chinese characters needed for every application page. That makes a driver-level localization pass heavier than necessary for this feature.

## Recommended Strategy

Use a small application-local Chinese glyph set inside `USER/app_env_monitor.c`.

Recommended approach:

- add a compact table of only the Chinese characters needed by current pages
- store each required character as a fixed `16x16` bitmap
- add local helper functions that draw one Chinese glyph or a short Chinese phrase onto the existing OLED framebuffer
- keep all dynamic numbers and technical abbreviations on the existing `OLED_ShowString` path

Why this is the best fit:

- it keeps the change scoped to the feature owner file
- it avoids expanding global font assets for unrelated modules
- it avoids inventing a new mixed-encoding string protocol for the OLED driver
- it makes later wording changes easy because each page explicitly controls layout

## Character and Rendering Model

Chinese content in this feature is limited to fixed labels and prompts.

Rendering rules:

- use `16x16` Chinese bitmaps for readable short labels
- use ASCII rendering for:
  - `HH:MM:SS`
  - `YYYY-MM-DD`
  - countdown values such as `30:00`
  - sensor values such as `25.3C`
  - technical abbreviations such as `RTC`, `MPU`, `PA0/PA1`, `Y`, `GZ`, `AX`
- build mixed lines by drawing Chinese labels first, then placing ASCII content immediately after them

Expected helper shape:

- one helper that draws a single named Chinese glyph at `x,y`
- one helper that draws a short sequence of named glyphs
- optional small formatting helpers for mixed Chinese-plus-ASCII lines where alignment needs to stay consistent

## Page Copy Plan

Apply Chinese UI copy to every page rendered in `USER/app_env_monitor.c`.

### Home Page

Keep the large time display unchanged.

Lower four cells become:

- top-left: `温25.3C`
- top-right: `湿64.2%`
- bottom-left: `压1013`
- bottom-right: `高12.3m`

Error states become:

- `温错`
- `湿错`
- `压错`
- `高错`

This keeps the compact four-cell layout while making the page immediately readable in Chinese.

### Timer Page

Replace English prompts with:

- title: `30分倒计时`
- finished state: `时间到`
- active reminder: `翻回主页`

The countdown value itself stays as ASCII `MM:SS`.

### Calendar Page

Replace English title and weekday wording with:

- title: `日历`
- weekday format: `周一` to `周日`

The date and time remain numeric:

- date: `2026-04-26`
- time: `09:30:00`

If `RTC` is unavailable:

- main error: `RTC错`
- fallback time string remains `--:--:--`

### Test Page

Replace the page title with:

- left title: `测试`

Keep the diagnostic abbreviations unchanged where they serve as compact engineering labels:

- `MPU`
- `Y`
- `GZ`
- `AX`
- `AY`
- `AZ`
- `TP`

If the sensor is unavailable:

- main error: `MPU错`
- wiring hint: `查PA0/PA1`

This page stays engineer-friendly while still localizing the user-facing parts.

## Layout and Sizing Constraints

The OLED remains `128x64`, so wording must stay short.

Layout expectations:

- Chinese labels in metric cells should use one character where possible:
  - `温`
  - `湿`
  - `压`
  - `高`
- multi-character Chinese titles should be placed with explicit coordinates instead of relying on string-width assumptions
- timer and calendar pages should stay visually centered after localization
- no page should exceed the existing buffer boundaries or overlap dividers

Because Chinese `16x16` glyphs are wider than the current `8`-pixel cell font, the implementation may slightly adjust `x` coordinates for localized labels while keeping the page structure unchanged.

## Application Structure

Keep the feature owned by `USER/app_env_monitor.c`.

Expected internal additions:

- a small enum or symbolic index set for the required Chinese glyphs
- a static `16x16` glyph table containing only the characters used by current pages
- a helper to draw one glyph into `OLED_GRAM`
- a helper to draw short Chinese phrases by glyph index sequence
- page-specific rendering updates that combine the new Chinese helpers with existing ASCII formatting helpers

Do not move this feature into the shared OLED driver unless the project later needs broad Chinese support across multiple modules.

## Error Handling

Localization must not change runtime error behavior.

Behavior expectations:

- device-ready checks stay exactly as they are today
- only the displayed copy changes
- one failing device must not block unrelated page content
- unsupported or missing glyph cases should not occur at runtime because the glyph table is fixed and closed over the approved page copy

If future wording expands beyond the current glyph set, the implementation should fail at compile-time review rather than silently showing blank glyphs.

## Implementation Scope

Expected touched files:

- Modify: `USER/app_env_monitor.c`

Expected reused files without functional changes:

- `HARDWARE/OLED/oled.c`
- `HARDWARE/OLED/oled.h`
- `HARDWARE/OLED/oledfont.h`

Non-goals for this change:

- no shared OLED API redesign
- no dynamic Chinese text encoding support
- no large global Chinese font pack expansion
- no page-navigation behavior changes
- no sensor or RTC logic changes beyond display text composition

## Testing Plan

1. Build validation
   - Keil rebuild succeeds after the new local glyph table and helpers are added

2. Home page validation
   - large time still renders correctly
   - four metric cells show `温`, `湿`, `压`, `高` labels without overlap
   - error states show the Chinese fault labels

3. Timer page validation
   - `30分倒计时` title fits the top area
   - active countdown still updates once per second
   - `时间到` and `翻回主页` render legibly in their intended states

4. Calendar page validation
   - `日历` title renders correctly
   - weekday shows `周一` through `周日`
   - `RTC错` appears when RTC is unavailable

5. Test page validation
   - `测试` title renders correctly
   - engineering abbreviations remain readable
   - `MPU错` and `查PA0/PA1` render correctly when the sensor is unavailable

6. Regression validation
   - page switching behavior is unchanged
   - OLED refresh behavior is unchanged
   - sensor acquisition and UART logging behavior are unchanged
