#include <stdio.h>
#include <string.h>
#include <math.h>
#include "app_env_monitor.h"
#include "oled.h"
#include "AHT20.h"
#include "BMP280.h"
#include "MPU6050.h"
#include "usart.h"
#include "../HARDWARE/BUZZER/Buzzer.h"
#include "../HARDWARE/DS3231/DS3231.h"

#define ENV_MONITOR_TASK_STACK_SIZE           1536U
#define ENV_MONITOR_TASK_DELAY_MS               30U
#define ENV_MONITOR_RTC_PERIOD_MS              200U
#define ENV_MONITOR_SENSOR_PERIOD_MS           200U
#define ENV_MONITOR_MPU_PERIOD_MS               30U
#define ENV_MONITOR_TEST_RENDER_PERIOD_MS      100U
#define ENV_MONITOR_PAGE_STABLE_MS             200U
#define ENV_MONITOR_PAGE_COOLDOWN_MS           300U
#define ENV_MONITOR_TIMER_DURATION_S        3UL
#define ENV_MONITOR_TIMER_RENDER_PERIOD_MS      30U
#define ENV_MONITOR_TIMER_BREATHE_FRAME_COUNT   60u
#define ENV_MONITOR_TIMER_BREATHE_CYCLE_MS    2000U
#define ENV_MONITOR_TIMER_CIRCLE_BASE_R        30u
#define ENV_MONITOR_TIMER_BREATHE_DELTA_PX      3
#define ENV_MONITOR_TIMER_RING_GAP              1u
#define ENV_MONITOR_BUZZER_ON_MS              150U
#define ENV_MONITOR_BUZZER_OFF_MS             150U
#define ENV_MONITOR_SHAKE_STOP_THRESHOLD_G    1.2f
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
    TickType_t last_timer_render_tick;
    TickType_t last_buzzer_toggle_tick;
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
    uint8_t timer_anim_step;
    uint8_t timer_done_latched;
    uint8_t timer_alarm_active;
    uint8_t timer_alarm_dismissed;
    uint8_t buzzer_output_on;
    uint8_t oled_rotation;
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

static uint8_t env_monitor_calendar_column(uint8_t week)
{
    return (uint8_t)(week % 7u);
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

static void env_monitor_fill_rect(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t mode)
{
    uint8_t x;
    uint8_t y;

    for (y = y0; y <= y1; y++)
    {
        for (x = x0; x <= x1; x++)
        {
            OLED_DrawPoint(x, y, mode);
        }
    }
}

static uint8_t env_monitor_center_x(uint8_t width, uint8_t content_width)
{
    if (content_width >= width)
    {
        return 0u;
    }

    return (uint8_t)((width - content_width) / 2u);
}

static uint8_t env_monitor_page_rotation(EnvMonitorPage_t page)
{
    switch (page)
    {
    case ENV_PAGE_TIMER_30MIN:
        return OLED_ROTATE_270;
    case ENV_PAGE_CALENDAR:
        return OLED_ROTATE_180;
    case ENV_PAGE_TEST_MODE:
        return OLED_ROTATE_90;
    case ENV_PAGE_HOME:
    default:
        return OLED_ROTATE_0;
    }
}

static void env_monitor_apply_rotation(EnvMonitorContext *context)
{
    uint8_t rotation;

    if (context == 0)
    {
        return;
    }

    rotation = env_monitor_page_rotation(context->current_page);
    if (context->oled_rotation == rotation)
    {
        return;
    }

    OLED_SetRotation(rotation);
    context->oled_rotation = rotation;
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

static void env_monitor_stop_timer_alarm(EnvMonitorContext *context)
{
    if (context == 0)
    {
        return;
    }

    context->timer_alarm_active = 0u;
    context->buzzer_output_on = 0u;
    Buzzer_Off();
}

static void env_monitor_reset_timer_state(EnvMonitorContext *context, TickType_t now_tick)
{
    if (context == 0)
    {
        return;
    }

    context->timer_started_tick = now_tick;
    context->last_timer_render_tick = now_tick;
    context->last_buzzer_toggle_tick = now_tick;
    context->timer_remaining_seconds = ENV_MONITOR_TIMER_DURATION_S;
    context->timer_anim_step = 0u;
    context->timer_done_latched = 0u;
    context->timer_alarm_active = 0u;
    context->timer_alarm_dismissed = 0u;
    context->buzzer_output_on = 0u;
    Buzzer_Off();
}

static float env_monitor_accel_magnitude_g(const MPU6050_Data_t *data)
{
    if (data == 0)
    {
        return 0.0f;
    }

    return sqrtf(data->accel_x_g * data->accel_x_g +
                 data->accel_y_g * data->accel_y_g +
                 data->accel_z_g * data->accel_z_g);
}

static int8_t env_monitor_timer_breathe_delta(uint8_t step)
{
    uint8_t phase;
    int16_t scaled_x10;
    int16_t amplitude_x10;

    amplitude_x10 = (int16_t)(ENV_MONITOR_TIMER_BREATHE_DELTA_PX * 10);

    phase = (uint8_t)(step % ENV_MONITOR_TIMER_BREATHE_FRAME_COUNT);
    if (phase < (ENV_MONITOR_TIMER_BREATHE_FRAME_COUNT / 2u))
    {
        scaled_x10 = (int16_t)(-amplitude_x10 + ((int16_t)phase * (amplitude_x10 * 2)) /
                                               (int16_t)((ENV_MONITOR_TIMER_BREATHE_FRAME_COUNT / 2u) - 1u));
    }
    else
    {
        phase = (uint8_t)(phase - (ENV_MONITOR_TIMER_BREATHE_FRAME_COUNT / 2u));
        scaled_x10 = (int16_t)(amplitude_x10 - ((int16_t)phase * (amplitude_x10 * 2)) /
                                                (int16_t)((ENV_MONITOR_TIMER_BREATHE_FRAME_COUNT / 2u) - 1u));
    }

    if (scaled_x10 >= 0)
    {
        return (int8_t)((scaled_x10 + 5) / 10);
    }

    return (int8_t)((scaled_x10 - 5) / 10);
}

static void env_monitor_draw_timer_circles(uint8_t x, uint8_t y, uint8_t radius)
{
    OLED_DrawCircle(x, y, radius, 1u);

    if (radius > ENV_MONITOR_TIMER_RING_GAP)
    {
        OLED_DrawCircle(x, y, (uint8_t)(radius - ENV_MONITOR_TIMER_RING_GAP), 1u);
    }

    if (radius > (ENV_MONITOR_TIMER_RING_GAP * 2u))
    {
        OLED_DrawCircle(x, y, (uint8_t)(radius - ENV_MONITOR_TIMER_RING_GAP * 2u), 1u);
    }
}

static void env_monitor_switch_page(EnvMonitorContext *context, EnvMonitorPage_t new_page, TickType_t now_tick)
{
    if ((context->current_page == ENV_PAGE_TIMER_30MIN) && (new_page != ENV_PAGE_TIMER_30MIN))
    {
        env_monitor_stop_timer_alarm(context);
    }

    context->current_page = new_page;
    context->candidate_page = new_page;
    context->candidate_since_tick = now_tick;
    context->last_page_switch_tick = now_tick;

    if (new_page == ENV_PAGE_TIMER_30MIN)
    {
        env_monitor_reset_timer_state(context, now_tick);
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

static uint8_t env_monitor_update_timer_runtime(EnvMonitorContext *context, TickType_t now_tick)
{
    TickType_t elapsed_ticks;
    uint32_t elapsed_ms;
    uint32_t remaining_ms;
    uint32_t remaining_seconds;
    uint8_t previous_anim_step;
    uint8_t previous_done_latched;
    uint8_t dirty;

    if ((context == 0) || (context->current_page != ENV_PAGE_TIMER_30MIN))
    {
        return 0u;
    }

    elapsed_ticks = (TickType_t)(now_tick - context->timer_started_tick);
    elapsed_ms = (uint32_t)pdTICKS_TO_MS(elapsed_ticks);
    previous_anim_step = context->timer_anim_step;
    previous_done_latched = context->timer_done_latched;
    dirty = 0u;

    if (elapsed_ms >= (ENV_MONITOR_TIMER_DURATION_S * 1000UL))
    {
        remaining_seconds = 0u;
    }
    else
    {
        remaining_ms = ENV_MONITOR_TIMER_DURATION_S * 1000UL - elapsed_ms;
        remaining_seconds = (remaining_ms + 999u) / 1000u;
    }

    context->timer_anim_step = (uint8_t)(((elapsed_ms % ENV_MONITOR_TIMER_BREATHE_CYCLE_MS) *
                                          ENV_MONITOR_TIMER_BREATHE_FRAME_COUNT) /
                                         ENV_MONITOR_TIMER_BREATHE_CYCLE_MS);

    if (remaining_seconds != context->timer_remaining_seconds)
    {
        context->timer_remaining_seconds = remaining_seconds;
        dirty = 1u;
    }

    if (context->timer_anim_step != previous_anim_step)
    {
        dirty = 1u;
    }

    if (remaining_seconds == 0u)
    {
        context->timer_done_latched = 1u;

        if ((context->timer_alarm_active == 0u) && (context->timer_alarm_dismissed == 0u))
        {
            context->timer_alarm_active = 1u;
            context->buzzer_output_on = 1u;
            context->last_buzzer_toggle_tick = now_tick;
            Buzzer_On();
        }
    }

    if (context->timer_done_latched != previous_done_latched)
    {
        dirty = 1u;
    }

    if ((TickType_t)(now_tick - context->last_timer_render_tick) >= pdMS_TO_TICKS(ENV_MONITOR_TIMER_RENDER_PERIOD_MS))
    {
        context->last_timer_render_tick = now_tick;
        dirty = 1u;
    }

    if (context->timer_alarm_active != 0u)
    {
        if ((context->mpu_ready != 0u) && (context->mpu_status == MPU6050_OK))
        {
            if (env_monitor_accel_magnitude_g(&context->mpu6050_data) > ENV_MONITOR_SHAKE_STOP_THRESHOLD_G)
            {
                context->timer_alarm_dismissed = 1u;
                env_monitor_stop_timer_alarm(context);
                dirty = 1u;
            }
        }

        if ((context->timer_alarm_active != 0u) &&
            ((TickType_t)(now_tick - context->last_buzzer_toggle_tick) >=
             pdMS_TO_TICKS((context->buzzer_output_on != 0u) ? ENV_MONITOR_BUZZER_ON_MS :
                                                             ENV_MONITOR_BUZZER_OFF_MS)))
        {
            context->last_buzzer_toggle_tick = now_tick;
            context->buzzer_output_on = (uint8_t)!context->buzzer_output_on;

            if (context->buzzer_output_on != 0u)
            {
                Buzzer_On();
            }
            else
            {
                Buzzer_Off();
            }
        }
    }

    return dirty;
}

static void env_monitor_draw_homepage_ornaments(void)
{
    OLED_DrawCircle(14u, 2u, 7u, 2u);
    OLED_DrawCircle(1u, 2u, 13u, 1u);

    OLED_DrawCircle(126u, 17u, 5u, 1u);
    OLED_DrawCircle(126u, 12u, 10u, 2u);

    OLED_DrawCircle(52u, 63u, 6u, 1u);
    OLED_DrawCircle(52u, 63u, 12u, 1u);
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
    const uint8_t label_top_y = 33u;
    const uint8_t label_bottom_y = 50u;
    const uint8_t left_value_x = 26u;
    const uint8_t right_value_x = 90u;

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
    env_monitor_draw_homepage_ornaments();

    if ((context->aht20_ready != 0u) && (context->aht20_status == AHT20_OK))
    {
        env_monitor_show_zh_phrase(ENV_MONITOR_LEFT_CELL_X, label_top_y, top_left_label, 2u, 12u);
        format_tagged_signed_1(context->line, "", context->aht20_data.temperature_c, "C");
        OLED_ShowString(left_value_x, ENV_MONITOR_TOP_CELL_Y, (u8 *)context->line, 8u, 1u);

        env_monitor_show_zh_phrase(ENV_MONITOR_RIGHT_CELL_X, label_top_y, top_right_label, 2u, 12u);
        format_tagged_unsigned_1(context->line, "", context->aht20_data.humidity_rh, "%");
        OLED_ShowString(right_value_x, ENV_MONITOR_TOP_CELL_Y, (u8 *)context->line, 8u, 1u);
    }
    else
    {
        env_monitor_show_zh_phrase(ENV_MONITOR_LEFT_CELL_X, ENV_MONITOR_TOP_CELL_Y, top_left_error, 2u, 12u);
        env_monitor_show_zh_phrase(ENV_MONITOR_RIGHT_CELL_X, ENV_MONITOR_TOP_CELL_Y, top_right_error, 2u, 12u);
    }

    if ((context->bmp280_ready != 0u) && (context->bmp280_status == BMP280_OK))
    {
        env_monitor_show_zh_phrase(ENV_MONITOR_LEFT_CELL_X, label_bottom_y, bottom_left_label, 2u, 12u);
        format_pressure_value(context->line, context->bmp280_data.pressure_pa);
        OLED_ShowString(left_value_x, ENV_MONITOR_BOTTOM_CELL_Y, (u8 *)context->line, 8u, 1u);

        env_monitor_show_zh_phrase(ENV_MONITOR_RIGHT_CELL_X, label_bottom_y, bottom_right_label, 2u, 12u);
        format_tagged_signed_1(context->line, "", context->altitude_m, "m");
        OLED_ShowString(right_value_x, ENV_MONITOR_BOTTOM_CELL_Y, (u8 *)context->line, 8u, 1u);
    }
    else
    {
        env_monitor_show_zh_phrase(ENV_MONITOR_LEFT_CELL_X, ENV_MONITOR_BOTTOM_CELL_Y, bottom_left_error, 2u, 12u);
        env_monitor_show_zh_phrase(ENV_MONITOR_RIGHT_CELL_X, ENV_MONITOR_BOTTOM_CELL_Y, bottom_right_error, 2u, 12u);
    }

    OLED_Refresh();
}

static void env_monitor_render_timer_page(EnvMonitorContext *context)
{
    static const uint8_t timer_title[3] = {ENV_ZH_DAO, ENV_ZH_JI, ENV_ZH_SHI_TIME};
    static const uint8_t timer_done[3] = {ENV_ZH_SHI_TIME, ENV_ZH_JIAN, ENV_ZH_DAO_REACHED};
    uint8_t width;
    uint8_t height;
    uint8_t circle_center_x;
    uint8_t circle_center_y;
    uint8_t circle_radius;
    uint8_t done_y;

    OLED_ClearBuffer();
    width = OLED_GetWidth();
    height = OLED_GetHeight();
    circle_center_x = (uint8_t)(width / 2u);
    circle_center_y = (uint8_t)(height / 2u - 6u);
    circle_radius = (uint8_t)((int16_t)ENV_MONITOR_TIMER_CIRCLE_BASE_R +
                              (int16_t)env_monitor_timer_breathe_delta(context->timer_anim_step));
    done_y = (uint8_t)(circle_center_y + ENV_MONITOR_TIMER_CIRCLE_BASE_R + 12u);

    env_monitor_show_zh_phrase(env_monitor_center_x(width, 48u), 8u, timer_title, 3u, 16u);
    env_monitor_draw_timer_circles(circle_center_x, circle_center_y, circle_radius);
    format_mm_ss(context->line, context->timer_remaining_seconds);
    OLED_ShowString(env_monitor_center_x(width, 40u), (uint8_t)(circle_center_y - 8u), (u8 *)context->line, 16, 1);

    if (context->timer_done_latched != 0u)
    {
        env_monitor_show_zh_phrase(env_monitor_center_x(width, 36u), done_y, timer_done, 3u, 12u);
    }

    OLED_Refresh();
}

static void env_monitor_render_calendar_page(EnvMonitorContext *context)
{
    static const uint8_t weekday_headers[7] =
    {
        ENV_ZH_RI, ENV_ZH_YI, ENV_ZH_ER, ENV_ZH_SAN, ENV_ZH_SI, ENV_ZH_WU, ENV_ZH_LIU
    };
    static const uint8_t rtc_error[3] = {ENV_ZH_SHI_TIME, ENV_ZH_ZHONG, ENV_ZH_CUO};
    const uint8_t grid_x0 = 1u;
    const uint8_t grid_y0 = 9u;
    const uint8_t cell_w = 18u;
    const uint8_t cell_h = 9u;
    uint16_t year_full;
    uint8_t days_in_month;
    uint8_t first_week;
    uint8_t first_col;
    uint8_t row;
    uint8_t col;
    uint8_t day;
    uint8_t x;
    uint8_t y;
    char day_text[3];

    OLED_ClearBuffer();

    if ((context->rtc_ready != 0u) && (context->rtc_status == DS3231_OK))
    {
        year_full = (uint16_t)(2000u + context->rtc_time.year);
        days_in_month = env_monitor_days_in_month(year_full, context->rtc_time.month);
        first_week = env_monitor_calc_weekday(year_full, context->rtc_time.month, 1u);
        first_col = env_monitor_calendar_column(first_week);

        for (col = 0u; col < 7u; col++)
        {
            x = (uint8_t)(grid_x0 + col * cell_w + 5u);
            env_monitor_show_zh_glyph(x, 0u, 8u, weekday_headers[col]);
        }

        for (day = 1u; day <= days_in_month; day++)
        {
            col = (uint8_t)((first_col + day - 1u) % 7u);
            row = (uint8_t)((first_col + day - 1u) / 7u);
            x = (uint8_t)(grid_x0 + col * cell_w);
            y = (uint8_t)(grid_y0 + row * cell_h);

            if (day == context->rtc_time.day)
            {
                env_monitor_fill_rect((uint8_t)(x + 1u), (uint8_t)(y + 1u),
                                      (uint8_t)(x + cell_w - 2u), (uint8_t)(y + cell_h - 2u), 1u);
                sprintf(day_text, "%u", (unsigned int)day);
                OLED_ShowString((uint8_t)(x + ((day < 10u) ? 6u : 3u)), (uint8_t)(y + 1u), (u8 *)day_text, 8u, 0u);
            }
            else
            {
                sprintf(day_text, "%u", (unsigned int)day);
                OLED_ShowString((uint8_t)(x + ((day < 10u) ? 6u : 3u)), (uint8_t)(y + 1u), (u8 *)day_text, 8u, 1u);
            }
        }
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
    uint8_t width;

    OLED_ClearBuffer();
    width = OLED_GetWidth();

    OLED_ShowString((u8)((width - 24u) / 2u), 0u, (u8 *)"TEST", 12, 1);

    if ((context->mpu_ready != 0u) && (context->mpu_status == MPU6050_OK))
    {
        format_tagged_signed_1(context->line, "AX:", context->mpu6050_data.accel_x_g, "g");
        OLED_ShowString(0u, 18u, (u8 *)context->line, 8, 1);

        format_tagged_signed_1(context->line, "AY:", context->mpu6050_data.accel_y_g, "g");
        OLED_ShowString(0u, 34u, (u8 *)context->line, 8, 1);

        format_tagged_signed_1(context->line, "AZ:", context->mpu6050_data.accel_z_g, "g");
        OLED_ShowString(0u, 50u, (u8 *)context->line, 8, 1);

        format_tagged_signed_1(context->line, "TP:", context->mpu6050_data.temp_c, "C");
        OLED_ShowString(0u, 66u, (u8 *)context->line, 8, 1);
    }
    else
    {
        OLED_ShowString(4u, 28u, (u8 *)"MPU ERR", 16, 1);
        OLED_ShowString(11u, 54u, (u8 *)"PA0/PA1", 8, 1);
    }

    OLED_Refresh();
}

static void env_monitor_render_page(EnvMonitorContext *context)
{
    env_monitor_apply_rotation(context);

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
    context.oled_rotation = 0xFFu;

    OLED_Init();
    Buzzer_Init();

    now_tick = xTaskGetTickCount();
    context.last_rtc_tick = now_tick - pdMS_TO_TICKS(ENV_MONITOR_RTC_PERIOD_MS);
    context.last_sensor_tick = now_tick - pdMS_TO_TICKS(ENV_MONITOR_SENSOR_PERIOD_MS);
    context.last_mpu_poll_tick = now_tick - pdMS_TO_TICKS(ENV_MONITOR_MPU_PERIOD_MS);
    context.candidate_since_tick = now_tick;
    context.last_page_switch_tick = now_tick - pdMS_TO_TICKS(ENV_MONITOR_PAGE_COOLDOWN_MS);
    context.last_test_render_tick = now_tick - pdMS_TO_TICKS(ENV_MONITOR_TEST_RENDER_PERIOD_MS);
    env_monitor_reset_timer_state(&context, now_tick);
    env_monitor_render_page(&context);

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
            context.last_mpu_poll_tick = now_tick;
        }

        timer_dirty = env_monitor_update_timer_runtime(&context, now_tick);

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

