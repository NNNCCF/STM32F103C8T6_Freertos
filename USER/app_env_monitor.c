#include <stdio.h>
#include <string.h>
#include <math.h>
#include "app_env_monitor.h"
#include "oled.h"
#include "AHT20.h"
#include "BMP280.h"
#include "MPU6050.h"
#include "usart.h"
#include "../HARDWARE/DS3231/DS3231.h"

#define ENV_MONITOR_TASK_STACK_SIZE           1536U
#define ENV_MONITOR_TASK_DELAY_MS               30U
#define ENV_MONITOR_RTC_PERIOD_MS              200U
#define ENV_MONITOR_SENSOR_PERIOD_MS           200U
#define ENV_MONITOR_MPU_PERIOD_MS               30U
#define ENV_MONITOR_TEST_RENDER_PERIOD_MS      100U
#define ENV_MONITOR_PAGE_STABLE_MS             200U
#define ENV_MONITOR_PAGE_COOLDOWN_MS           300U
#define ENV_MONITOR_TIMER_DURATION_S        1800UL
#define ENV_MONITOR_SEA_LEVEL_PRESSURE_PA  101325.0f
#define ENV_MONITOR_ACCEL_AXIS_TRIGGER_G       0.90f
#define ENV_MONITOR_ACCEL_CROSS_AXIS_MAX_G     0.35f

#define ENV_MONITOR_TIME_X         16u
#define ENV_MONITOR_TIME_Y          4u
#define ENV_MONITOR_LEFT_CELL_X     1u
#define ENV_MONITOR_RIGHT_CELL_X   65u
#define ENV_MONITOR_TOP_CELL_Y     36u
#define ENV_MONITOR_BOTTOM_CELL_Y  52u
#define ENV_MONITOR_CELL_FONT       8u
#define ENV_MONITOR_DIVIDER_X      64u
#define ENV_MONITOR_TOP_LINE_Y     31u
#define ENV_MONITOR_BOTTOM_LINE_Y  49u

#define ENV_MONITOR_CMD_OK          0u
#define ENV_MONITOR_CMD_ERR_FORMAT  1u
#define ENV_MONITOR_CMD_ERR_RANGE   2u
#define ENV_MONITOR_TIME_CMD_LEN   24u

typedef enum
{
    ENV_PAGE_HOME = 0,
    ENV_PAGE_TIMER_30MIN,
    ENV_PAGE_CALENDAR,
    ENV_PAGE_TEST_MODE,
    ENV_PAGE_INVALID = 255
} EnvMonitorPage_t;

typedef enum
{
    ENV_ZH_WEN = 0,
    ENV_ZH_SHI = 1,
    ENV_ZH_DU = 2,
    ENV_ZH_QI = 3,
    ENV_ZH_YA = 4,
    ENV_ZH_GAO = 5,
    ENV_ZH_DU_2 = 6,
    ENV_ZH_CUO = 7,
    ENV_ZH_WU_2 = 8,
    ENV_ZH_FEN = 9,
    ENV_ZH_DAO = 10,
    ENV_ZH_JI = 11,
    ENV_ZH_SHI_TIME = 12,
    ENV_ZH_JIAN = 13,
    ENV_ZH_DAO_REACHED = 14,
    ENV_ZH_HUI = 15,
    ENV_ZH_ZHU = 16,
    ENV_ZH_YE = 17,
    ENV_ZH_RI = 18,
    ENV_ZH_LI = 19,
    ENV_ZH_ZHOU = 20,
    ENV_ZH_YI = 21,
    ENV_ZH_ER = 22,
    ENV_ZH_SAN = 23,
    ENV_ZH_SI = 24,
    ENV_ZH_WU = 25,
    ENV_ZH_LIU = 26,
    ENV_ZH_ZHONG = 27,
    ENV_ZH_DAO_3 = 28,
    ENV_ZH_JI_2 = 29,
    ENV_ZH_SHI_2 = 30
} EnvMonitorZhGlyph_t;

typedef struct
{
    AHT20_Data_t aht20_data;
    BMP280_Data_t bmp280_data;
    DS3231_Time_t rtc_time;
    MPU6050_Data_t mpu6050_data;
    float altitude_m;
    TickType_t last_rtc_tick;
    TickType_t last_sensor_tick;
    TickType_t last_mpu_poll_tick;
    TickType_t candidate_since_tick;
    TickType_t last_page_switch_tick;
    TickType_t timer_started_tick;
    TickType_t last_test_render_tick;
    uint32_t timer_remaining_seconds;
    EnvMonitorPage_t current_page;
    EnvMonitorPage_t candidate_page;
    uint8_t aht20_ready;
    uint8_t bmp280_ready;
    uint8_t rtc_ready;
    uint8_t mpu_ready;
    uint8_t aht20_status;
    uint8_t bmp280_status;
    uint8_t rtc_status;
    uint8_t mpu_status;
    uint8_t oled_display_turn;
    char line[32];
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

static void format_date_ymd(char *buffer, const DS3231_Time_t *time)
{
    sprintf(buffer, "20%02u-%02u-%02u",
            (unsigned int)time->year,
            (unsigned int)time->month,
            (unsigned int)time->day);
}

static void format_mm_ss(char *buffer, uint32_t total_seconds)
{
    unsigned long minutes;
    unsigned long seconds;

    minutes = (unsigned long)(total_seconds / 60u);
    seconds = (unsigned long)(total_seconds % 60u);
    sprintf(buffer, "%02lu:%02lu", minutes, seconds);
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

static void format_pressure_value(char *buffer, float pressure_pa)
{
    unsigned long pressure_hpa;

    if (pressure_pa < 0.0f)
    {
        pressure_pa = 0.0f;
    }

    pressure_hpa = (unsigned long)(pressure_pa / 100.0f + 0.5f);
    sprintf(buffer, "%luhPa", pressure_hpa);
}

static float env_monitor_calc_altitude_m(float pressure_pa)
{
    if (pressure_pa <= 0.0f)
    {
        return 0.0f;
    }

    return 44330.0f * (1.0f - powf(pressure_pa / ENV_MONITOR_SEA_LEVEL_PRESSURE_PA, 0.1903f));
}

static uint8_t env_monitor_weekday_glyph(uint8_t week)
{
    switch (week)
    {
    case 1u:
        return ENV_ZH_YI;
    case 2u:
        return ENV_ZH_ER;
    case 3u:
        return ENV_ZH_SAN;
    case 4u:
        return ENV_ZH_SI;
    case 5u:
        return ENV_ZH_WU;
    case 6u:
        return ENV_ZH_LIU;
    case 7u:
    default:
        return ENV_ZH_RI;
    }
}

static void env_monitor_show_zh_glyph(uint8_t x, uint8_t y,uint8_t size, uint8_t glyph)
{
    OLED_ShowChinese(x, y, glyph, size, 1u);
}

static void env_monitor_show_zh_phrase(uint8_t x, uint8_t y, const uint8_t *glyphs, uint8_t len, uint8_t size)
{
    uint8_t i;

    if (glyphs == 0)
    {
        return;
    }

    for (i = 0u; i < len; i++)
    {
        env_monitor_show_zh_glyph((uint8_t)(x + i * size), y, size, glyphs[i]);
    }
}

static uint8_t env_monitor_page_display_turn(EnvMonitorPage_t page)
{
    switch (page)
    {
    case ENV_PAGE_CALENDAR:
    case ENV_PAGE_TEST_MODE:
        return 1u;
    case ENV_PAGE_HOME:
    case ENV_PAGE_TIMER_30MIN:
    default:
        return 0u;
    }
}

static void env_monitor_apply_display_turn(EnvMonitorContext *context)
{
    uint8_t display_turn;

    if (context == 0)
    {
        return;
    }

    display_turn = env_monitor_page_display_turn(context->current_page);
    if (context->oled_display_turn == display_turn)
    {
        return;
    }

    OLED_DisplayTurn(display_turn);
    context->oled_display_turn = display_turn;
}

static const char *env_monitor_page_text(EnvMonitorPage_t page)
{
    switch (page)
    {
    case ENV_PAGE_HOME:
        return "HOME";
    case ENV_PAGE_TIMER_30MIN:
        return "TIMER";
    case ENV_PAGE_CALENDAR:
        return "CAL";
    case ENV_PAGE_TEST_MODE:
        return "TEST";
    default:
        return "UNK";
    }
}

static EnvMonitorPage_t env_monitor_classify_page_from_accel(const MPU6050_Data_t *data)
{
    if (data == 0)
    {
        return ENV_PAGE_INVALID;
    }

    if ((data->accel_x_g >= ENV_MONITOR_ACCEL_AXIS_TRIGGER_G) &&
        (fabsf(data->accel_y_g) <= ENV_MONITOR_ACCEL_CROSS_AXIS_MAX_G))
    {
        return ENV_PAGE_HOME;
    }

    if ((data->accel_y_g >= ENV_MONITOR_ACCEL_AXIS_TRIGGER_G) &&
        (fabsf(data->accel_x_g) <= ENV_MONITOR_ACCEL_CROSS_AXIS_MAX_G))
    {
        return ENV_PAGE_TIMER_30MIN;
    }

    if ((data->accel_x_g <= -ENV_MONITOR_ACCEL_AXIS_TRIGGER_G) &&
        (fabsf(data->accel_y_g) <= ENV_MONITOR_ACCEL_CROSS_AXIS_MAX_G))
    {
        return ENV_PAGE_CALENDAR;
    }

    if ((data->accel_y_g <= -ENV_MONITOR_ACCEL_AXIS_TRIGGER_G) &&
        (fabsf(data->accel_x_g) <= ENV_MONITOR_ACCEL_CROSS_AXIS_MAX_G))
    {
        return ENV_PAGE_TEST_MODE;
    }

    return ENV_PAGE_INVALID;
}

static void env_monitor_switch_page(EnvMonitorContext *context, EnvMonitorPage_t new_page, TickType_t now_tick)
{
    context->current_page = new_page;
    context->candidate_page = new_page;
    context->candidate_since_tick = now_tick;
    context->last_page_switch_tick = now_tick;

    if (new_page == ENV_PAGE_TIMER_30MIN)
    {
        context->timer_started_tick = now_tick;
        context->timer_remaining_seconds = ENV_MONITOR_TIMER_DURATION_S;
    }

    if (new_page == ENV_PAGE_TEST_MODE)
    {
        context->last_test_render_tick = now_tick - pdMS_TO_TICKS(ENV_MONITOR_TEST_RENDER_PERIOD_MS);
    }
}

static uint8_t env_monitor_update_page_state(EnvMonitorContext *context, EnvMonitorPage_t observed_page, TickType_t now_tick)
{
    if (observed_page == ENV_PAGE_INVALID)
    {
        context->candidate_page = ENV_PAGE_INVALID;
        context->candidate_since_tick = now_tick;
        return 0u;
    }

    if (observed_page == context->current_page)
    {
        context->candidate_page = observed_page;
        context->candidate_since_tick = now_tick;
        return 0u;
    }

    if (observed_page != context->candidate_page)
    {
        context->candidate_page = observed_page;
        context->candidate_since_tick = now_tick;
        return 0u;
    }

    if ((TickType_t)(now_tick - context->last_page_switch_tick) < pdMS_TO_TICKS(ENV_MONITOR_PAGE_COOLDOWN_MS))
    {
        return 0u;
    }

    if ((TickType_t)(now_tick - context->candidate_since_tick) < pdMS_TO_TICKS(ENV_MONITOR_PAGE_STABLE_MS))
    {
        return 0u;
    }

    env_monitor_switch_page(context, observed_page, now_tick);
    return 1u;
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

static uint8_t env_monitor_read_mpu_sample(EnvMonitorContext *context, TickType_t now_tick)
{
    EnvMonitorPage_t observed_page;

    context->mpu_status = MPU6050_ReadData(&context->mpu6050_data);
    if (context->mpu_status != MPU6050_OK)
    {
        context->mpu_ready = 0u;
        return 0u;
    }

    observed_page = env_monitor_classify_page_from_accel(&context->mpu6050_data);
    return env_monitor_update_page_state(context, observed_page, now_tick);
}

static uint8_t env_monitor_update_mpu(EnvMonitorContext *context, TickType_t now_tick)
{
    uint8_t previous_ready;
    uint8_t previous_status;
    EnvMonitorPage_t previous_page;
    uint8_t dirty;

    previous_ready = context->mpu_ready;
    previous_status = context->mpu_status;
    previous_page = context->current_page;
    dirty = 0u;

    if (context->mpu_ready == 0u)
    {
        context->mpu_status = MPU6050_Init();
        context->mpu_ready = (uint8_t)(context->mpu_status == MPU6050_OK);

        if (context->mpu_ready != 0u)
        {
            dirty |= env_monitor_read_mpu_sample(context, now_tick);
        }
    }
    else
    {
        dirty |= env_monitor_read_mpu_sample(context, now_tick);
    }

    if ((context->mpu_ready != 0u) &&
        (context->current_page == ENV_PAGE_TEST_MODE) &&
        ((TickType_t)(now_tick - context->last_test_render_tick) >= pdMS_TO_TICKS(ENV_MONITOR_TEST_RENDER_PERIOD_MS)))
    {
        context->last_test_render_tick = now_tick;
        dirty = 1u;
    }

    if ((previous_ready != context->mpu_ready) || (previous_status != context->mpu_status))
    {
        if (context->mpu_ready != 0u)
        {
            printf("MPU READY PA0/PA1\r\n");
        }
        else
        {
            printf("MPU ERR %u\r\n", (unsigned int)context->mpu_status);
        }
        dirty = 1u;
    }

    if (previous_page != context->current_page)
    {
        dirty = 1u;
    }

    return dirty;
}

static uint8_t env_monitor_update_timer(EnvMonitorContext *context, TickType_t now_tick)
{
    TickType_t elapsed_ticks;
    uint32_t elapsed_ms;
    uint32_t remaining_ms;
    uint32_t remaining_seconds;

    if (context->current_page != ENV_PAGE_TIMER_30MIN)
    {
        return 0u;
    }

    elapsed_ticks = (TickType_t)(now_tick - context->timer_started_tick);
    elapsed_ms = (uint32_t)pdTICKS_TO_MS(elapsed_ticks);

    if (elapsed_ms >= (ENV_MONITOR_TIMER_DURATION_S * 1000UL))
    {
        remaining_seconds = 0u;
    }
    else
    {
        remaining_ms = ENV_MONITOR_TIMER_DURATION_S * 1000UL - elapsed_ms;
        remaining_seconds = (remaining_ms + 999u) / 1000u;
    }

    if (remaining_seconds != context->timer_remaining_seconds)
    {
        context->timer_remaining_seconds = remaining_seconds;
        return 1u;
    }

    return 0u;
}

static void env_monitor_render_homepage(EnvMonitorContext *context)
{
    static const uint8_t top_left_label[2] = {ENV_ZH_WEN, ENV_ZH_DU};
    static const uint8_t top_right_label[2] = {ENV_ZH_SHI, ENV_ZH_DU};
    static const uint8_t bottom_left_label[2] = {ENV_ZH_QI, ENV_ZH_YA};
    static const uint8_t bottom_right_label[2] = {ENV_ZH_GAO, ENV_ZH_DU_2};
    static const uint8_t top_left_error[2] = {ENV_ZH_WEN, ENV_ZH_CUO};
    static const uint8_t top_right_error[2] = {ENV_ZH_SHI, ENV_ZH_CUO};
    static const uint8_t bottom_left_error[2] = {ENV_ZH_YA, ENV_ZH_CUO};
    static const uint8_t bottom_right_error[2] = {ENV_ZH_GAO, ENV_ZH_CUO};

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
        env_monitor_show_zh_phrase(ENV_MONITOR_LEFT_CELL_X, ENV_MONITOR_TOP_CELL_Y, top_left_label, 2u, 8u);
        format_tagged_signed_1(context->line, "", context->aht20_data.temperature_c, "C");
        OLED_ShowString((u8)(ENV_MONITOR_LEFT_CELL_X + 18u), ENV_MONITOR_TOP_CELL_Y, (u8 *)context->line, ENV_MONITOR_CELL_FONT, 1u);

        env_monitor_show_zh_phrase(ENV_MONITOR_RIGHT_CELL_X, ENV_MONITOR_TOP_CELL_Y, top_right_label, 2u, 8u);
        format_tagged_unsigned_1(context->line, "", context->aht20_data.humidity_rh, "%");
        OLED_ShowString((u8)(ENV_MONITOR_RIGHT_CELL_X + 18u), ENV_MONITOR_TOP_CELL_Y, (u8 *)context->line, ENV_MONITOR_CELL_FONT, 1u);
    }
    else
    {
        env_monitor_show_zh_phrase(ENV_MONITOR_LEFT_CELL_X, ENV_MONITOR_TOP_CELL_Y, top_left_error, 2u, 8u);
        env_monitor_show_zh_phrase(ENV_MONITOR_RIGHT_CELL_X, ENV_MONITOR_TOP_CELL_Y, top_right_error, 2u, 8u);
    }

    if ((context->bmp280_ready != 0u) && (context->bmp280_status == BMP280_OK))
    {
        env_monitor_show_zh_phrase(ENV_MONITOR_LEFT_CELL_X, ENV_MONITOR_BOTTOM_CELL_Y, bottom_left_label, 2u, 8u);
        format_pressure_value(context->line, context->bmp280_data.pressure_pa);
        OLED_ShowString((u8)(ENV_MONITOR_LEFT_CELL_X + 18u), ENV_MONITOR_BOTTOM_CELL_Y, (u8 *)context->line, ENV_MONITOR_CELL_FONT, 1u);

        env_monitor_show_zh_phrase(ENV_MONITOR_RIGHT_CELL_X, ENV_MONITOR_BOTTOM_CELL_Y, bottom_right_label, 2u, 8u);
        format_tagged_signed_1(context->line, "", context->altitude_m, "m");
        OLED_ShowString((u8)(ENV_MONITOR_RIGHT_CELL_X + 18u), ENV_MONITOR_BOTTOM_CELL_Y, (u8 *)context->line, ENV_MONITOR_CELL_FONT, 1u);
    }
    else
    {
        env_monitor_show_zh_phrase(ENV_MONITOR_LEFT_CELL_X, ENV_MONITOR_BOTTOM_CELL_Y, bottom_left_error, 2u, 8u);
        env_monitor_show_zh_phrase(ENV_MONITOR_RIGHT_CELL_X, ENV_MONITOR_BOTTOM_CELL_Y, bottom_right_error, 2u, 8u);
    }

    OLED_Refresh();
}

static void env_monitor_render_timer_page(EnvMonitorContext *context)
{
    static const uint8_t timer_title[4] = {ENV_ZH_FEN, ENV_ZH_DAO, ENV_ZH_JI, ENV_ZH_SHI_TIME};
    static const uint8_t timer_done[3] = {ENV_ZH_SHI_TIME, ENV_ZH_JIAN, ENV_ZH_DAO_REACHED};
    static const uint8_t timer_home[3] = {ENV_ZH_HUI, ENV_ZH_ZHU, ENV_ZH_YE};

    OLED_ClearBuffer();

    OLED_ShowString(24u, 0u, (u8 *)"30", 16, 1);
    env_monitor_show_zh_phrase(40u, 0u, timer_title, 4u, 16u);
    format_mm_ss(context->line, context->timer_remaining_seconds);
    OLED_ShowString(34u, 18u, (u8 *)context->line, 24, 1);

    if (context->timer_remaining_seconds == 0u)
    {
        env_monitor_show_zh_phrase(40u, 48u, timer_done, 3u, 12u);
    }
    else
    {
        env_monitor_show_zh_phrase(40u, 48u, timer_home, 3u, 12u);
    }

    OLED_Refresh();
}

static void env_monitor_render_calendar_page(EnvMonitorContext *context)
{
    static const uint8_t calendar_title[2] = {ENV_ZH_RI, ENV_ZH_LI};
    static const uint8_t rtc_error[3] = {ENV_ZH_SHI_TIME, ENV_ZH_ZHONG, ENV_ZH_CUO};

    OLED_ClearBuffer();

    env_monitor_show_zh_phrase(48u, 0u, calendar_title, 2u, 16u);

    if ((context->rtc_ready != 0u) && (context->rtc_status == DS3231_OK))
    {
        format_date_ymd(context->line, &context->rtc_time);
        OLED_ShowString(24u, 16u, (u8 *)context->line, 16, 1);

        env_monitor_show_zh_glyph(20u, 40u, 12u, ENV_ZH_ZHOU);
        env_monitor_show_zh_glyph(36u, 40u, 12u, env_monitor_weekday_glyph(context->rtc_time.week));
        sprintf(context->line, "%02u:%02u:%02u",
                (unsigned int)context->rtc_time.hour,
                (unsigned int)context->rtc_time.min,
                (unsigned int)context->rtc_time.sec);
        OLED_ShowString(56u, 42u, (u8 *)context->line, 12, 1);
    }
    else
    {
        env_monitor_show_zh_phrase(40u, 20u, rtc_error, 3u, 12u);
        OLED_ShowString(28u, 44u, (u8 *)"--:--:--", 12, 1);
    }

    OLED_Refresh();
}

static void env_monitor_render_test_page(EnvMonitorContext *context)
{
    OLED_ClearBuffer();

    OLED_ShowString(0u, 0u, (u8 *)"TEST", 12, 1);
    OLED_ShowString(68u, 0u, (u8 *)env_monitor_page_text(context->current_page), 12, 1);

    if ((context->mpu_ready != 0u) && (context->mpu_status == MPU6050_OK))
    {
        format_tagged_signed_1(context->line, "AX", context->mpu6050_data.accel_x_g, "g");
        OLED_ShowString(0u, 16u, (u8 *)context->line, 8, 1);

        format_tagged_signed_1(context->line, "AY", context->mpu6050_data.accel_y_g, "g");
        OLED_ShowString(64u, 16u, (u8 *)context->line, 8, 1);

        format_tagged_signed_1(context->line, "AZ", context->mpu6050_data.accel_z_g, "g");
        OLED_ShowString(0u, 34u, (u8 *)context->line, 8, 1);

        format_tagged_signed_1(context->line, "TP", context->mpu6050_data.temp_c, "C");
        OLED_ShowString(64u, 34u, (u8 *)context->line, 8, 1);
    }
    else
    {
        OLED_ShowString(28u, 26u, (u8 *)"MPU ERR", 16, 1);
        OLED_ShowString(28u, 50u, (u8 *)"PA0/PA1", 8, 1);
    }

    OLED_Refresh();
}

static void env_monitor_render_page(EnvMonitorContext *context)
{
    env_monitor_apply_display_turn(context);

    switch (context->current_page)
    {
    case ENV_PAGE_TIMER_30MIN:
        env_monitor_render_timer_page(context);
        break;
    case ENV_PAGE_CALENDAR:
        env_monitor_render_calendar_page(context);
        break;
    case ENV_PAGE_TEST_MODE:
        env_monitor_render_test_page(context);
        break;
    case ENV_PAGE_HOME:
    default:
        env_monitor_render_homepage(context);
        break;
    }
}

static void env_monitor_log_uart(EnvMonitorContext *context)
{
    printf("PG:%s  ", env_monitor_page_text(context->current_page));

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
        printf("%s  ", context->line);
    }
    else
    {
        printf("P:ERR  A:ERR  ");
    }

    if ((context->mpu_ready != 0u) && (context->mpu_status == MPU6050_OK))
    {
        format_tagged_signed_1(context->line, "AX:", context->mpu6050_data.accel_x_g, "g");
        printf("%s  ", context->line);

        format_tagged_signed_1(context->line, "AY:", context->mpu6050_data.accel_y_g, "g");
        printf("%s  ", context->line);

        format_tagged_signed_1(context->line, "AZ:", context->mpu6050_data.accel_z_g, "g");
        printf("%s", context->line);
    }
    else
    {
        printf("AX:ERR  AY:ERR  AZ:ERR");
    }

    printf("\r\n");
}

static void env_monitor_task(void *pvParameters)
{
    EnvMonitorContext context;
    TickType_t now_tick;
    uint8_t command_dirty;
    uint8_t rtc_dirty;
    uint8_t sensor_dirty;
    uint8_t page_dirty;
    uint8_t timer_dirty;

    (void)pvParameters;

    memset(&context, 0, sizeof(context));
    context.aht20_status = AHT20_ERR_I2C;
    context.bmp280_status = BMP280_ERR_I2C;
    context.rtc_status = DS3231_ERR_I2C;
    context.mpu_status = 0xFFu;
    context.current_page = ENV_PAGE_HOME;
    context.candidate_page = ENV_PAGE_HOME;
    context.timer_remaining_seconds = ENV_MONITOR_TIMER_DURATION_S;
    context.oled_display_turn = 0xFFu;

    OLED_Init();
    env_monitor_render_page(&context);

    now_tick = xTaskGetTickCount();
    context.last_rtc_tick = now_tick - pdMS_TO_TICKS(ENV_MONITOR_RTC_PERIOD_MS);
    context.last_sensor_tick = now_tick - pdMS_TO_TICKS(ENV_MONITOR_SENSOR_PERIOD_MS);
    context.last_mpu_poll_tick = now_tick - pdMS_TO_TICKS(ENV_MONITOR_MPU_PERIOD_MS);
    context.candidate_since_tick = now_tick;
    context.last_page_switch_tick = now_tick - pdMS_TO_TICKS(ENV_MONITOR_PAGE_COOLDOWN_MS);
    context.timer_started_tick = now_tick;
    context.last_test_render_tick = now_tick - pdMS_TO_TICKS(ENV_MONITOR_TEST_RENDER_PERIOD_MS);

    for (;;)
    {
        now_tick = xTaskGetTickCount();
        command_dirty = env_monitor_handle_uart_command(&context);
        rtc_dirty = 0u;
        sensor_dirty = 0u;
        page_dirty = 0u;
        timer_dirty = 0u;

        if ((TickType_t)(now_tick - context.last_rtc_tick) >= pdMS_TO_TICKS(ENV_MONITOR_RTC_PERIOD_MS))
        {
            rtc_dirty = env_monitor_update_rtc(&context);
            context.last_rtc_tick = now_tick;
        }

        if ((TickType_t)(now_tick - context.last_sensor_tick) >= pdMS_TO_TICKS(ENV_MONITOR_SENSOR_PERIOD_MS))
        {
            sensor_dirty |= env_monitor_update_aht20(&context);
            sensor_dirty |= env_monitor_update_bmp280(&context);
            context.last_sensor_tick = now_tick;
        }

        if ((TickType_t)(now_tick - context.last_mpu_poll_tick) >= pdMS_TO_TICKS(ENV_MONITOR_MPU_PERIOD_MS))
        {
            page_dirty = env_monitor_update_mpu(&context, now_tick);
            context.last_mpu_poll_tick = xTaskGetTickCount();
        }

        timer_dirty = env_monitor_update_timer(&context, xTaskGetTickCount());

        if ((command_dirty != 0u) || (rtc_dirty != 0u) || (sensor_dirty != 0u) || (page_dirty != 0u) || (timer_dirty != 0u))
        {
            env_monitor_render_page(&context);
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

