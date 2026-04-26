#include <stdio.h>
#include <string.h>
#include <math.h>
#include "app_env_monitor.h"
#include "oled.h"
#include "AHT20.h"
#include "BMP280.h"
#include "usart.h"
#include "../HARDWARE/DS3231/DS3231.h"

#define ENV_MONITOR_TASK_STACK_SIZE           512U
#define ENV_MONITOR_TASK_DELAY_MS             200U
#define ENV_MONITOR_SENSOR_PERIOD_MS         1000U
#define ENV_MONITOR_SEA_LEVEL_PRESSURE_PA  101325.0f

#define ENV_MONITOR_TIME_X         16u
#define ENV_MONITOR_TIME_Y          4u
#define ENV_MONITOR_LEFT_CELL_X     4u
#define ENV_MONITOR_RIGHT_CELL_X   68u
#define ENV_MONITOR_TOP_CELL_Y     36u
#define ENV_MONITOR_BOTTOM_CELL_Y  52u
#define ENV_MONITOR_CELL_FONT       8u
#define ENV_MONITOR_DIVIDER_X      64u
#define ENV_MONITOR_TOP_LINE_Y     31u
#define ENV_MONITOR_BOTTOM_LINE_Y  47u

#define ENV_MONITOR_CMD_OK          0u
#define ENV_MONITOR_CMD_ERR_FORMAT  1u
#define ENV_MONITOR_CMD_ERR_RANGE   2u
#define ENV_MONITOR_TIME_CMD_LEN   24u

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

static uint8_t env_monitor_fetch_uart_line(char *buffer, uint16_t size)
{
    uint16_t rx_state;
    uint16_t length;

    if ((buffer == 0) || (size == 0u))
    {
        return 0u;
    }

    rx_state = USART_RX_STA;
    if ((rx_state & 0x8000u) == 0u)
    {
        return 0u;
    }

    length = (uint16_t)(rx_state & 0x3FFFu);
    if (length >= size)
    {
        length = (uint16_t)(size - 1u);
    }

    memcpy(buffer, (const void *)USART_RX_BUF, length);
    buffer[length] = '\0';
    USART_RX_STA = 0u;
    return 1u;
}

static uint8_t env_monitor_parse_fixed_uint(const char *text, uint8_t digits, uint16_t *value)
{
    uint8_t i;
    uint16_t result;

    if ((text == 0) || (value == 0))
    {
        return 1u;
    }

    result = 0u;
    for (i = 0u; i < digits; i++)
    {
        if ((text[i] < '0') || (text[i] > '9'))
        {
            return 1u;
        }
        result = (uint16_t)(result * 10u + (uint16_t)(text[i] - '0'));
    }

    *value = result;
    return 0u;
}

static uint8_t env_monitor_is_leap_year(uint16_t year)
{
    if (((year % 4u) == 0u) && (((year % 100u) != 0u) || ((year % 400u) == 0u)))
    {
        return 1u;
    }

    return 0u;
}

static uint8_t env_monitor_days_in_month(uint16_t year, uint8_t month)
{
    static const uint8_t days[12] = {31u, 28u, 31u, 30u, 31u, 30u, 31u, 31u, 30u, 31u, 30u, 31u};
    uint8_t result;

    if ((month < 1u) || (month > 12u))
    {
        return 0u;
    }

    result = days[month - 1u];
    if ((month == 2u) && (env_monitor_is_leap_year(year) != 0u))
    {
        result = 29u;
    }

    return result;
}

static uint8_t env_monitor_calc_weekday(uint16_t year, uint8_t month, uint8_t day)
{
    static const uint8_t offsets[12] = {0u, 3u, 2u, 5u, 0u, 3u, 5u, 1u, 4u, 6u, 2u, 4u};
    uint16_t adjusted_year;
    uint8_t weekday;

    adjusted_year = year;
    if (month < 3u)
    {
        adjusted_year--;
    }

    weekday = (uint8_t)((adjusted_year + adjusted_year / 4u - adjusted_year / 100u + adjusted_year / 400u +
                         offsets[month - 1u] + day) % 7u);

    return (uint8_t)((weekday == 0u) ? 7u : weekday);
}

static uint8_t env_monitor_parse_time_command(const char *command, DS3231_Time_t *time)
{
    uint16_t year_full;
    uint16_t month;
    uint16_t day;
    uint16_t hour;
    uint16_t minute;
    uint16_t second;
    uint8_t max_day;

    if ((command == 0) || (time == 0))
    {
        return ENV_MONITOR_CMD_ERR_FORMAT;
    }

    if ((strlen(command) != ENV_MONITOR_TIME_CMD_LEN) || (strncmp(command, "TIME=", 5) != 0))
    {
        return ENV_MONITOR_CMD_ERR_FORMAT;
    }

    if ((command[9] != '-') || (command[12] != '-') || (command[15] != ' ') ||
        (command[18] != ':') || (command[21] != ':'))
    {
        return ENV_MONITOR_CMD_ERR_FORMAT;
    }

    if ((env_monitor_parse_fixed_uint(&command[5], 4u, &year_full) != 0u) ||
        (env_monitor_parse_fixed_uint(&command[10], 2u, &month) != 0u) ||
        (env_monitor_parse_fixed_uint(&command[13], 2u, &day) != 0u) ||
        (env_monitor_parse_fixed_uint(&command[16], 2u, &hour) != 0u) ||
        (env_monitor_parse_fixed_uint(&command[19], 2u, &minute) != 0u) ||
        (env_monitor_parse_fixed_uint(&command[22], 2u, &second) != 0u))
    {
        return ENV_MONITOR_CMD_ERR_FORMAT;
    }

    if ((year_full < 2000u) || (year_full > 2099u) || (month < 1u) || (month > 12u) ||
        (hour > 23u) || (minute > 59u) || (second > 59u))
    {
        return ENV_MONITOR_CMD_ERR_RANGE;
    }

    max_day = env_monitor_days_in_month(year_full, (uint8_t)month);
    if ((day < 1u) || (day > max_day))
    {
        return ENV_MONITOR_CMD_ERR_RANGE;
    }

    time->year = (uint8_t)(year_full % 100u);
    time->month = (uint8_t)month;
    time->day = (uint8_t)day;
    time->hour = (uint8_t)hour;
    time->min = (uint8_t)minute;
    time->sec = (uint8_t)second;
    time->week = env_monitor_calc_weekday(year_full, (uint8_t)month, (uint8_t)day);

    return ENV_MONITOR_CMD_OK;
}

static uint8_t env_monitor_handle_uart_command(EnvMonitorContext *context)
{
    char command[USART_REC_LEN];
    DS3231_Time_t new_time;
    uint8_t parse_status;

    if ((context == 0) || (env_monitor_fetch_uart_line(command, sizeof(command)) == 0u))
    {
        return 0u;
    }

    parse_status = env_monitor_parse_time_command(command, &new_time);
    if (parse_status == ENV_MONITOR_CMD_ERR_FORMAT)
    {
        printf("TIME ERR FORMAT\r\n");
        return 0u;
    }

    if (parse_status == ENV_MONITOR_CMD_ERR_RANGE)
    {
        printf("TIME ERR RANGE\r\n");
        return 0u;
    }

    if (context->rtc_ready == 0u)
    {
        context->rtc_status = DS3231_Init();
        context->rtc_ready = (uint8_t)(context->rtc_status == DS3231_OK);
    }

    if (context->rtc_ready == 0u)
    {
        printf("TIME ERR RTC\r\n");
        return 0u;
    }

    context->rtc_status = DS3231_SetTime(&new_time);
    if (context->rtc_status != DS3231_OK)
    {
        context->rtc_ready = 0u;
        printf("TIME ERR RTC\r\n");
        return 0u;
    }

    context->rtc_status = DS3231_GetTime(&context->rtc_time);
    context->rtc_ready = (uint8_t)(context->rtc_status == DS3231_OK);
    if (context->rtc_ready == 0u)
    {
        printf("TIME ERR RTC\r\n");
        return 0u;
    }

    printf("TIME OK 20%02u-%02u-%02u %02u:%02u:%02u\r\n",
           (unsigned int)context->rtc_time.year,
           (unsigned int)context->rtc_time.month,
           (unsigned int)context->rtc_time.day,
           (unsigned int)context->rtc_time.hour,
           (unsigned int)context->rtc_time.min,
           (unsigned int)context->rtc_time.sec);

    return 1u;
}

static long round_signed_1(float value)
{
    if (value >= 0.0f)
    {
        return (long)(value * 10.0f + 0.5f);
    }

    return (long)(value * 10.0f - 0.5f);
}

static unsigned long round_unsigned_1(float value)
{
    if (value < 0.0f)
    {
        value = 0.0f;
    }

    return (unsigned long)(value * 10.0f + 0.5f);
}

static void format_time_hms(char *buffer, const DS3231_Time_t *time)
{
    sprintf(buffer, "%02u:%02u:%02u",
            (unsigned int)time->hour,
            (unsigned int)time->min,
            (unsigned int)time->sec);
}

static void format_tagged_signed_1(char *buffer, const char *label, float value, const char *unit)
{
    long scaled;
    unsigned long abs_scaled;

    scaled = round_signed_1(value);
    abs_scaled = (unsigned long)((scaled < 0L) ? -scaled : scaled);

    if (scaled < 0L)
    {
        sprintf(buffer, "%s-%lu.%lu%s", label, abs_scaled / 10UL, abs_scaled % 10UL, unit);
    }
    else
    {
        sprintf(buffer, "%s%lu.%lu%s", label, abs_scaled / 10UL, abs_scaled % 10UL, unit);
    }
}

static void format_tagged_unsigned_1(char *buffer, const char *label, float value, const char *unit)
{
    unsigned long scaled;

    scaled = round_unsigned_1(value);
    sprintf(buffer, "%s%lu.%lu%s", label, scaled / 10UL, scaled % 10UL, unit);
}

static void format_pressure_cell(char *buffer, float pressure_pa)
{
    unsigned long pressure_hpa;

    if (pressure_pa < 0.0f)
    {
        pressure_pa = 0.0f;
    }

    pressure_hpa = (unsigned long)(pressure_pa / 100.0f + 0.5f);
    sprintf(buffer, "P%luhPa", pressure_hpa);
}

static float env_monitor_calc_altitude_m(float pressure_pa)
{
    if (pressure_pa <= 0.0f)
    {
        return 0.0f;
    }

    return 44330.0f * (1.0f - powf(pressure_pa / ENV_MONITOR_SEA_LEVEL_PRESSURE_PA, 0.1903f));
}

static uint8_t env_monitor_update_rtc(EnvMonitorContext *context)
{
    uint8_t previous_ready;
    uint8_t previous_status;
    uint8_t previous_hour;
    uint8_t previous_min;
    uint8_t previous_sec;

    previous_ready = context->rtc_ready;
    previous_status = context->rtc_status;
    previous_hour = context->rtc_time.hour;
    previous_min = context->rtc_time.min;
    previous_sec = context->rtc_time.sec;

    if (context->rtc_ready == 0u)
    {
        context->rtc_status = DS3231_Init();
        context->rtc_ready = (uint8_t)(context->rtc_status == DS3231_OK);
    }

    if (context->rtc_ready != 0u)
    {
        context->rtc_status = DS3231_GetTime(&context->rtc_time);
        if (context->rtc_status != DS3231_OK)
        {
            context->rtc_ready = 0u;
        }
    }

    return (uint8_t)((previous_ready != context->rtc_ready) ||
                     (previous_status != context->rtc_status) ||
                     (previous_hour != context->rtc_time.hour) ||
                     (previous_min != context->rtc_time.min) ||
                     (previous_sec != context->rtc_time.sec));
}

static uint8_t env_monitor_update_aht20(EnvMonitorContext *context)
{
    uint8_t previous_ready;
    uint8_t previous_status;
    uint8_t dirty;

    previous_ready = context->aht20_ready;
    previous_status = context->aht20_status;
    dirty = 0u;

    if (context->aht20_ready == 0u)
    {
        context->aht20_status = AHT20_Init();
        context->aht20_ready = (uint8_t)(context->aht20_status == AHT20_OK);
    }

    if (context->aht20_ready != 0u)
    {
        context->aht20_status = AHT20_ReadData(&context->aht20_data);
        if (context->aht20_status != AHT20_OK)
        {
            context->aht20_ready = 0u;
        }
        else
        {
            dirty = 1u;
        }
    }

    if ((previous_ready != context->aht20_ready) || (previous_status != context->aht20_status))
    {
        dirty = 1u;
    }

    return dirty;
}

static uint8_t env_monitor_update_bmp280(EnvMonitorContext *context)
{
    uint8_t previous_ready;
    uint8_t previous_status;
    uint8_t dirty;

    previous_ready = context->bmp280_ready;
    previous_status = context->bmp280_status;
    dirty = 0u;

    if (context->bmp280_ready == 0u)
    {
        context->bmp280_status = BMP280_Init();
        context->bmp280_ready = (uint8_t)(context->bmp280_status == BMP280_OK);
    }

    if (context->bmp280_ready != 0u)
    {
        context->bmp280_status = BMP280_ReadData(&context->bmp280_data);
        if (context->bmp280_status != BMP280_OK)
        {
            context->bmp280_ready = 0u;
        }
        else
        {
            context->altitude_m = env_monitor_calc_altitude_m(context->bmp280_data.pressure_pa);
            dirty = 1u;
        }
    }

    if ((previous_ready != context->bmp280_ready) || (previous_status != context->bmp280_status))
    {
        dirty = 1u;
    }

    return dirty;
}

static void env_monitor_render_homepage(EnvMonitorContext *context)
{
    OLED_ClearBuffer();

    if ((context->rtc_ready != 0u) && (context->rtc_status == DS3231_OK))
    {
        format_time_hms(context->line, &context->rtc_time);
        OLED_ShowString(ENV_MONITOR_TIME_X, ENV_MONITOR_TIME_Y, (u8 *)context->line, 24, 1);
    }
    else
    {
        OLED_ShowString(ENV_MONITOR_TIME_X, ENV_MONITOR_TIME_Y, (u8 *)"--:--:--", 24, 1);
    }

    OLED_DrawLine(0u, ENV_MONITOR_TOP_LINE_Y, 127u, ENV_MONITOR_TOP_LINE_Y, 1u);
    OLED_DrawLine(0u, ENV_MONITOR_BOTTOM_LINE_Y, 127u, ENV_MONITOR_BOTTOM_LINE_Y, 1u);
    OLED_DrawLine(ENV_MONITOR_DIVIDER_X, ENV_MONITOR_TOP_LINE_Y, ENV_MONITOR_DIVIDER_X, 63u, 1u);

    if ((context->aht20_ready != 0u) && (context->aht20_status == AHT20_OK))
    {
        format_tagged_signed_1(context->line, "T", context->aht20_data.temperature_c, "C");
        OLED_ShowString(ENV_MONITOR_LEFT_CELL_X, ENV_MONITOR_TOP_CELL_Y, (u8 *)context->line, ENV_MONITOR_CELL_FONT, 1);

        format_tagged_unsigned_1(context->line, "H", context->aht20_data.humidity_rh, "%");
        OLED_ShowString(ENV_MONITOR_RIGHT_CELL_X, ENV_MONITOR_TOP_CELL_Y, (u8 *)context->line, ENV_MONITOR_CELL_FONT, 1);
    }
    else
    {
        OLED_ShowString(ENV_MONITOR_LEFT_CELL_X, ENV_MONITOR_TOP_CELL_Y, (u8 *)"T ERR", ENV_MONITOR_CELL_FONT, 1);
        OLED_ShowString(ENV_MONITOR_RIGHT_CELL_X, ENV_MONITOR_TOP_CELL_Y, (u8 *)"H ERR", ENV_MONITOR_CELL_FONT, 1);
    }

    if ((context->bmp280_ready != 0u) && (context->bmp280_status == BMP280_OK))
    {
        format_pressure_cell(context->line, context->bmp280_data.pressure_pa);
        OLED_ShowString(ENV_MONITOR_LEFT_CELL_X, ENV_MONITOR_BOTTOM_CELL_Y, (u8 *)context->line, ENV_MONITOR_CELL_FONT, 1);

        format_tagged_signed_1(context->line, "A", context->altitude_m, "m");
        OLED_ShowString(ENV_MONITOR_RIGHT_CELL_X, ENV_MONITOR_BOTTOM_CELL_Y, (u8 *)context->line, ENV_MONITOR_CELL_FONT, 1);
    }
    else
    {
        OLED_ShowString(ENV_MONITOR_LEFT_CELL_X, ENV_MONITOR_BOTTOM_CELL_Y, (u8 *)"P ERR", ENV_MONITOR_CELL_FONT, 1);
        OLED_ShowString(ENV_MONITOR_RIGHT_CELL_X, ENV_MONITOR_BOTTOM_CELL_Y, (u8 *)"A ERR", ENV_MONITOR_CELL_FONT, 1);
    }

    OLED_Refresh();
}

static void env_monitor_log_uart(EnvMonitorContext *context)
{
    if ((context->rtc_ready != 0u) && (context->rtc_status == DS3231_OK))
    {
        format_time_hms(context->line, &context->rtc_time);
        printf("%s  ", context->line);
    }
    else
    {
        printf("--:--:--  ");
    }

    if ((context->aht20_ready != 0u) && (context->aht20_status == AHT20_OK))
    {
        format_tagged_signed_1(context->line, "T:", context->aht20_data.temperature_c, "C");
        printf("%s  ", context->line);

        format_tagged_unsigned_1(context->line, "H:", context->aht20_data.humidity_rh, "%");
        printf("%s  ", context->line);
    }
    else
    {
        printf("T:ERR  H:ERR  ");
    }

    if ((context->bmp280_ready != 0u) && (context->bmp280_status == BMP280_OK))
    {
        format_tagged_unsigned_1(context->line, "P:", context->bmp280_data.pressure_pa / 100.0f, "hPa");
        printf("%s  ", context->line);

        format_tagged_signed_1(context->line, "A:", context->altitude_m, "m");
        printf("%s\r\n", context->line);
    }
    else
    {
        printf("P:ERR  A:ERR\r\n");
    }
}

static void env_monitor_task(void *pvParameters)
{
    EnvMonitorContext context;
    TickType_t last_sensor_tick;
    TickType_t now_tick;
    uint8_t command_dirty;
    uint8_t rtc_dirty;
    uint8_t sensor_dirty;

    (void)pvParameters;

    memset(&context, 0, sizeof(context));
    context.aht20_status = AHT20_ERR_I2C;
    context.bmp280_status = BMP280_ERR_I2C;
    context.rtc_status = DS3231_ERR_I2C;

    OLED_Init();
    env_monitor_render_homepage(&context);

    last_sensor_tick = xTaskGetTickCount() - pdMS_TO_TICKS(ENV_MONITOR_SENSOR_PERIOD_MS);

    for (;;)
    {
        command_dirty = env_monitor_handle_uart_command(&context);
        rtc_dirty = env_monitor_update_rtc(&context);
        sensor_dirty = 0u;
        now_tick = xTaskGetTickCount();

        if ((TickType_t)(now_tick - last_sensor_tick) >= pdMS_TO_TICKS(ENV_MONITOR_SENSOR_PERIOD_MS))
        {
            sensor_dirty |= env_monitor_update_aht20(&context);
            sensor_dirty |= env_monitor_update_bmp280(&context);
            last_sensor_tick = now_tick;
        }

        if ((command_dirty != 0u) || (rtc_dirty != 0u) || (sensor_dirty != 0u))
        {
            env_monitor_render_homepage(&context);
            env_monitor_log_uart(&context);
        }

        vTaskDelay(pdMS_TO_TICKS(ENV_MONITOR_TASK_DELAY_MS));
    }
}

BaseType_t app_env_monitor_start(UBaseType_t priority)
{
    return xTaskCreate(env_monitor_task,
                       "env_task",
                       ENV_MONITOR_TASK_STACK_SIZE,
                       NULL,
                       priority,
                       NULL);
}
