# Env Monitor Homepage Clock Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a DS3231-driven large `HH:MM:SS` clock to the OLED home page and reorganize the lower half into four compact regions for temperature, humidity, pressure, and altitude.

**Architecture:** Keep the existing single FreeRTOS monitor task, extend its context with DS3231 state and derived altitude, split sampling cadence between time and sensors, and render one complete framebuffer-only home page before a single OLED refresh.

**Tech Stack:** STM32F103C8, FreeRTOS, StdPeriph I2C, SSD1306-style OLED API, DS3231, AHT20, BMP280, Keil ARMCC5.

---

### Task 1: Bring DS3231 into the monitor task and the Keil build

**Files:**
- Modify: `USER/app_env_monitor.c`
- Review: `HARDWARE/DS3231/DS3231.h`
- Modify: `USER/Template.uvprojx` if `DS3231.c` is missing from the target

- [ ] Add the DS3231 include and extend the monitor context with time state, status, and ready flags.

```c
#include "DS3231.h"

typedef struct
{
    AHT20_Data_t aht20_data;
    BMP280_Data_t bmp280_data;
    DS3231_Time_t rtc_time;
    float altitude_m;
    uint8_t aht20_ready;
    uint8_t bmp280_ready;
    uint8_t rtc_ready;
    uint8_t aht20_status;
    uint8_t bmp280_status;
    uint8_t rtc_status;
    char line[24];
} EnvMonitorContext;
```

- [ ] Add a DS3231 update helper that initializes once and then reads time on each polling cycle.

```c
static void env_monitor_update_rtc(EnvMonitorContext *context)
{
    if (context->rtc_ready == 0u)
    {
        context->rtc_status = DS3231_Init();
        context->rtc_ready = (uint8_t)(context->rtc_status == DS3231_OK);
    }
    else
    {
        context->rtc_status = DS3231_GetTime(&context->rtc_time);
        if (context->rtc_status != DS3231_OK)
        {
            context->rtc_ready = 0u;
        }
    }
}
```

- [ ] Check whether `HARDWARE/DS3231/DS3231.c` is already part of `USER/Template.uvprojx`; if not, add it to the same hardware-driver group used by the other sensors.

```xml
<File>
  <FileName>DS3231.c</FileName>
  <FileType>1</FileType>
  <FilePath>..\\HARDWARE\\DS3231\\DS3231.c</FilePath>
</File>
```

- [ ] Run a rebuild after the project-file update.

Run: `D:\Keil_v5\UV4\UV4.exe -j0 -r USER\Template.uvprojx -t Template`  
Expected: build completes without unresolved `DS3231_*` symbols

### Task 2: Add compact formatting and altitude calculation helpers

**Files:**
- Modify: `USER/app_env_monitor.c`

- [ ] Add a fixed sea-level pressure constant and a helper that derives altitude from BMP280 pressure.

```c
#include <math.h>

#define ENV_MONITOR_SEA_LEVEL_PRESSURE_PA  101325.0f

static float env_monitor_calc_altitude_m(float pressure_pa)
{
    if (pressure_pa <= 0.0f)
    {
        return 0.0f;
    }

    return 44330.0f * (1.0f - powf(pressure_pa / ENV_MONITOR_SEA_LEVEL_PRESSURE_PA, 0.1903f));
}
```

- [ ] Add a time-format helper for the large clock string.

```c
static void format_time_hms(char *buffer, const DS3231_Time_t *time)
{
    sprintf(buffer, "%02u:%02u:%02u",
            (unsigned int)time->hour,
            (unsigned int)time->min,
            (unsigned int)time->sec);
}
```

- [ ] Add compact cell-format helpers sized for `64 px` regions.

```c
static void format_cell_temp(char *buffer, float value)
{
    sprintf(buffer, "T%2.1fC", value);
}

static void format_cell_humi(char *buffer, float value)
{
    sprintf(buffer, "H%2.1f%%", value);
}

static void format_cell_press(char *buffer, float pressure_hpa)
{
    sprintf(buffer, "P%4.0fh", pressure_hpa);
}

static void format_cell_alti(char *buffer, float altitude_m)
{
    sprintf(buffer, "A%2.1fm", altitude_m);
}
```

- [ ] Update the BMP280 path so altitude is refreshed whenever pressure is valid.

```c
context->bmp280_status = BMP280_ReadData(&context->bmp280_data);
if (context->bmp280_status == BMP280_OK)
{
    context->altitude_m = env_monitor_calc_altitude_m(context->bmp280_data.pressure_pa);
}
```

### Task 3: Replace the list page with the clock dashboard layout

**Files:**
- Modify: `USER/app_env_monitor.c`
- Review: `HARDWARE/OLED/oled.h`

- [ ] Replace the current text-list renderer with a dedicated home-page renderer using the fixed coordinates from the spec.

```c
static void env_monitor_render_homepage(EnvMonitorContext *context)
{
    OLED_ClearBuffer();

    if ((context->rtc_ready != 0u) && (context->rtc_status == DS3231_OK))
    {
        format_time_hms(context->line, &context->rtc_time);
        OLED_ShowString(16, 4, (u8 *)context->line, 24, 1);
    }
    else
    {
        OLED_ShowString(16, 4, (u8 *)"--:--:--", 24, 1);
    }

    OLED_DrawLine(0, 31, 127, 31, 1);
    OLED_DrawLine(0, 47, 127, 47, 1);
    OLED_DrawLine(64, 31, 64, 63, 1);
}
```

- [ ] Add the four cell render branches with compact per-cell strings and error fallbacks.

```c
if ((context->aht20_ready != 0u) && (context->aht20_status == AHT20_OK))
{
    format_cell_temp(context->line, context->aht20_data.temperature_c);
    OLED_ShowString(4, 34, (u8 *)context->line, 12, 1);

    format_cell_humi(context->line, context->aht20_data.humidity_rh);
    OLED_ShowString(68, 34, (u8 *)context->line, 12, 1);
}
else
{
    OLED_ShowString(4, 34, (u8 *)"T ERR", 12, 1);
    OLED_ShowString(68, 34, (u8 *)"H ERR", 12, 1);
}
```

```c
if ((context->bmp280_ready != 0u) && (context->bmp280_status == BMP280_OK))
{
    format_cell_press(context->line, context->bmp280_data.pressure_pa / 100.0f);
    OLED_ShowString(4, 50, (u8 *)context->line, 12, 1);

    format_cell_alti(context->line, context->altitude_m);
    OLED_ShowString(68, 50, (u8 *)context->line, 12, 1);
}
else
{
    OLED_ShowString(4, 50, (u8 *)"P ERR", 12, 1);
    OLED_ShowString(68, 50, (u8 *)"A ERR", 12, 1);
}

OLED_Refresh();
```

- [ ] Remove or shorten the old startup splash if it visually conflicts with the new home page.

```c
OLED_Init();
OLED_ClearBuffer();
env_monitor_render_homepage(&context);
```

### Task 4: Split the polling cadence so time is responsive and sensors stay stable

**Files:**
- Modify: `USER/app_env_monitor.c`

- [ ] Replace the current single `50 ms` blind loop with separate time and sensor cadences.

```c
#define ENV_MONITOR_TASK_DELAY_MS        200U
#define ENV_MONITOR_SENSOR_PERIOD_MS    1000U
```

- [ ] Add simple tick bookkeeping so DS3231 is checked every loop while AHT20/BMP280 are only refreshed when their slower period expires.

```c
TickType_t last_sensor_tick = 0u;

for (;;)
{
    env_monitor_update_rtc(&context);

    if ((xTaskGetTickCount() - last_sensor_tick) >= pdMS_TO_TICKS(ENV_MONITOR_SENSOR_PERIOD_MS))
    {
        env_monitor_update_aht20(&context);
        env_monitor_update_bmp280(&context);
        last_sensor_tick = xTaskGetTickCount();
    }

    if ((last_displayed_sec != context->rtc_time.sec) || (sensor_dirty != 0u) || (status_dirty != 0u))
    {
        env_monitor_render_homepage(&context);
        last_displayed_sec = context->rtc_time.sec;
    }

    vTaskDelay(pdMS_TO_TICKS(ENV_MONITOR_TASK_DELAY_MS));
}
```

### Task 5: Update UART output and verify the new dashboard end-to-end

**Files:**
- Modify: `USER/app_env_monitor.c`
- Review: `OBJ/Template.build_log.htm`
- Review: `USER/Listings/Template.map`

- [ ] Extend UART logging so runtime output includes time and altitude, which helps verify DS3231 and the derived metric without relying only on the OLED.

```c
printf("%02u:%02u:%02u  T=%.1fC  H=%.1f%%  P=%.1fhPa  A=%.1fm\r\n",
       (unsigned int)context->rtc_time.hour,
       (unsigned int)context->rtc_time.min,
       (unsigned int)context->rtc_time.sec,
       context->aht20_data.temperature_c,
       context->aht20_data.humidity_rh,
       context->bmp280_data.pressure_pa / 100.0f,
       context->altitude_m);
```

- [ ] Run a full rebuild.

Run: `D:\Keil_v5\UV4\UV4.exe -j0 -r USER\Template.uvprojx -t Template`  
Expected: `0 Error(s), 0 Warning(s)`

- [ ] Verify the on-device behavior.

Run:
- power the board
- confirm OLED shows centered `HH:MM:SS`
- confirm lines appear at `y=31` and `y=47`
- confirm the center divider appears at `x=64`
- confirm lower values stay inside their cells
- confirm seconds tick once per second
- confirm UART prints time plus `T/H/P/A`

- [ ] Commit only the files related to this feature after verification.

```bash
git add USER/app_env_monitor.c USER/Template.uvprojx docs/superpowers/specs/2026-04-26-env-monitor-homepage-clock-design.md docs/superpowers/plans/2026-04-26-env-monitor-homepage-clock-implementation.md
git commit -m "feat: add clock dashboard homepage"
```
