#include <stdio.h>
#include "app_env_monitor.h"
#include "oled.h"
#include "AHT20.h"
#include "BMP280.h"

#define ENV_MONITOR_TASK_STACK_SIZE  512U
#define ENV_MONITOR_TASK_DELAY_MS    50U

typedef struct
{
    AHT20_Data_t aht20_data;
    BMP280_Data_t bmp280_data;
    uint8_t aht20_ready;
    uint8_t bmp280_ready;
    uint8_t aht20_status;
    uint8_t bmp280_status;
    char line[24];
} EnvMonitorContext;

static void format_signed_1(char *buffer, const char *label, float value, const char *unit)
{
    long scaled;
    unsigned long abs_scaled;

    if (value >= 0.0f)
    {
        scaled = (long)(value * 10.0f + 0.5f);
    }
    else
    {
        scaled = (long)(value * 10.0f - 0.5f);
    }

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

static void format_unsigned_1(char *buffer, const char *label, float value, const char *unit)
{
    unsigned long scaled;

    if (value < 0.0f)
    {
        value = 0.0f;
    }

    scaled = (unsigned long)(value * 10.0f + 0.5f);
    sprintf(buffer, "%s%lu.%lu%s", label, scaled / 10UL, scaled % 10UL, unit);
}

static void env_monitor_update_aht20(EnvMonitorContext *context)
{
    if (context->aht20_ready == 0u)
    {
        context->aht20_status = AHT20_Init();
        context->aht20_ready = (uint8_t)(context->aht20_status == AHT20_OK);
    }
    else
    {
        context->aht20_status = AHT20_ReadData(&context->aht20_data);
        if (context->aht20_status != AHT20_OK)
        {
            context->aht20_ready = 0u;
        }
    }
}

static void env_monitor_update_bmp280(EnvMonitorContext *context)
{
    if (context->bmp280_ready == 0u)
    {
        context->bmp280_status = BMP280_Init();
        context->bmp280_ready = (uint8_t)(context->bmp280_status == BMP280_OK);
    }
    else
    {
        context->bmp280_status = BMP280_ReadData(&context->bmp280_data);
        if (context->bmp280_status != BMP280_OK)
        {
            context->bmp280_ready = 0u;
        }
    }
}

static void env_monitor_render_oled(EnvMonitorContext *context)
{
    OLED_Clear();

    if ((context->aht20_ready != 0u) && (context->aht20_status == AHT20_OK))
    {
        format_signed_1(context->line, "AHT T:", context->aht20_data.temperature_c, "C");
        OLED_ShowString(0, 0, (u8 *)context->line, 8, 1);

        format_unsigned_1(context->line, "AHT H:", context->aht20_data.humidity_rh, "%");
        OLED_ShowString(0, 16, (u8 *)context->line, 8, 1);
    }
    else
    {
        sprintf(context->line, "AHT20 ERR:%u", (unsigned int)context->aht20_status);
        OLED_ShowString(0, 0, (u8 *)context->line, 8, 1);
    }

    if ((context->bmp280_ready != 0u) && (context->bmp280_status == BMP280_OK))
    {
        format_signed_1(context->line, "BMP T:", context->bmp280_data.temperature_c, "C");
        OLED_ShowString(0, 32, (u8 *)context->line, 8, 1);

        format_unsigned_1(context->line, "BMP P:", context->bmp280_data.pressure_pa / 100.0f, "hPa");
        OLED_ShowString(0, 48, (u8 *)context->line, 8, 1);
    }
    else
    {
        sprintf(context->line, "BMP280 ERR:%u", (unsigned int)context->bmp280_status);
        OLED_ShowString(0, 32, (u8 *)context->line, 8, 1);
    }

    OLED_Refresh();
}

static void env_monitor_log_uart(EnvMonitorContext *context)
{
    if ((context->aht20_ready != 0u) && (context->aht20_status == AHT20_OK) &&
        (context->bmp280_ready != 0u) && (context->bmp280_status == BMP280_OK))
    {
        format_signed_1(context->line, "AHT T:", context->aht20_data.temperature_c, "C");
        printf("%s  ", context->line);

        format_unsigned_1(context->line, "AHT H:", context->aht20_data.humidity_rh, "%");
        printf("%s  ", context->line);

        format_signed_1(context->line, "BMP T:", context->bmp280_data.temperature_c, "C");
        printf("%s  ", context->line);

        format_unsigned_1(context->line, "BMP P:", context->bmp280_data.pressure_pa / 100.0f, "hPa");
        printf("%s\r\n", context->line);
    }
    else
    {
        printf("AHT:%u BMP:%u\r\n",
               (unsigned int)context->aht20_status,
               (unsigned int)context->bmp280_status);
    }
}

static void env_monitor_task(void *pvParameters)
{
    EnvMonitorContext context;

    (void)pvParameters;

    context.aht20_ready = 0u;
    context.bmp280_ready = 0u;
    context.aht20_status = AHT20_ERR_I2C;
    context.bmp280_status = BMP280_ERR_I2C;

    OLED_Init();
    OLED_Clear();
    OLED_ShowString(0, 0, (u8 *)"ENV MONITOR", 8, 1);
    OLED_Refresh();
    vTaskDelay(pdMS_TO_TICKS(20));

    for (;;)
    {
        env_monitor_update_aht20(&context);
        env_monitor_update_bmp280(&context);
        env_monitor_render_oled(&context);
        env_monitor_log_uart(&context);

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
