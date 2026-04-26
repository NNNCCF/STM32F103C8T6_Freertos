#include "atk_ms6050_app.h"
#include "atk_ms6050.h"
#include "eMPL/inv_mpu.h"
#include <string.h>

typedef struct
{
    TaskHandle_t task_handle;
    uint8_t initialized;
    uint8_t consecutive_failures;
    atk_ms6050_attitude_t attitude;
} atk_ms6050_app_state_t;

static atk_ms6050_app_state_t g_atk_ms6050_app_state;

static void atk_ms6050_app_set_status(atk_ms6050_app_status_t status)
{
    taskENTER_CRITICAL();
    g_atk_ms6050_app_state.attitude.status = status;
    taskEXIT_CRITICAL();
}

static uint32_t atk_ms6050_app_now_ms(void)
{
    return (uint32_t)(((uint32_t)xTaskGetTickCount() * 1000UL) / configTICK_RATE_HZ);
}

uint8_t atk_ms6050_app_init(TaskHandle_t task_handle)
{
    uint8_t ret;

    memset(&g_atk_ms6050_app_state, 0, sizeof(g_atk_ms6050_app_state));
    atk_ms6050_clear_last_i2c_error();
    g_atk_ms6050_app_state.task_handle = task_handle;
    g_atk_ms6050_app_state.attitude.status = ATK_MS6050_APP_STATUS_INITING;

    ret = atk_ms6050_dmp_init();
    if (ret != ATK_MS6050_EOK)
    {
        switch (ret)
        {
            case ATK_MS6050_EACK:
                atk_ms6050_app_set_status(ATK_MS6050_APP_STATUS_I2C_FAIL);
                break;

            case ATK_MS6050_EID:
                atk_ms6050_app_set_status(ATK_MS6050_APP_STATUS_ID_FAIL);
                break;

            case ATK_MS6050_ESELFTEST:
                atk_ms6050_app_set_status(ATK_MS6050_APP_STATUS_SELF_TEST_FAIL);
                break;

            default:
                atk_ms6050_app_set_status(ATK_MS6050_APP_STATUS_DMP_FAIL);
                break;
        }

        return 1;
    }

#if ATK_MS6050_USE_INT
    atk_ms6050_int_exti_init();
    atk_ms6050_int_exti_clear_flag();
    (void)ulTaskNotifyTake(pdTRUE, 0);
    atk_ms6050_int_exti_cmd(ENABLE);
#endif

    taskENTER_CRITICAL();
    g_atk_ms6050_app_state.initialized = 1;
    g_atk_ms6050_app_state.attitude.status = ATK_MS6050_APP_STATUS_READY;
    taskEXIT_CRITICAL();

    return 0;
}

uint8_t atk_ms6050_app_wait_and_sample(TickType_t timeout_ticks)
{
    float pitch;
    float roll;
    float yaw;
#if ATK_MS6050_USE_INT
    uint8_t timed_out = 0;
#else
    TickType_t start_tick;
#endif

    if (g_atk_ms6050_app_state.initialized == 0)
    {
        return 1;
    }

#if ATK_MS6050_USE_INT
    if (ulTaskNotifyTake(pdTRUE, timeout_ticks) == 0)
    {
        if (atk_ms6050_int_pin_read() != Bit_RESET)
        {
            timed_out = 1;
        }
    }
#else
    start_tick = xTaskGetTickCount();
    for (;;)
    {
        if (atk_ms6050_dmp_get_data(&pitch, &roll, &yaw) == 0)
        {
            break;
        }

        if ((xTaskGetTickCount() - start_tick) >= timeout_ticks)
        {
            atk_ms6050_app_set_status(ATK_MS6050_APP_STATUS_WAIT_TIMEOUT);
            return 1;
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
#endif

#if ATK_MS6050_USE_INT
    if (atk_ms6050_dmp_get_data(&pitch, &roll, &yaw) != 0)
    {
        if (timed_out != 0)
        {
            atk_ms6050_app_set_status(ATK_MS6050_APP_STATUS_WAIT_TIMEOUT);
            return 1;
        }

        g_atk_ms6050_app_state.consecutive_failures++;
        if (g_atk_ms6050_app_state.consecutive_failures >= 3)
        {
            atk_ms6050_app_set_status(ATK_MS6050_APP_STATUS_FIFO_FAIL);
        }
        return 1;
    }
#endif

    g_atk_ms6050_app_state.consecutive_failures = 0;

    taskENTER_CRITICAL();
    g_atk_ms6050_app_state.attitude.pitch = pitch;
    g_atk_ms6050_app_state.attitude.roll = roll;
    g_atk_ms6050_app_state.attitude.yaw = yaw;
    g_atk_ms6050_app_state.attitude.timestamp_ms = atk_ms6050_app_now_ms();
    g_atk_ms6050_app_state.attitude.sample_count++;
    g_atk_ms6050_app_state.attitude.valid = 1;
    g_atk_ms6050_app_state.attitude.status = ATK_MS6050_APP_STATUS_READY;
    taskEXIT_CRITICAL();

    return 0;
}

void atk_ms6050_app_get_attitude(atk_ms6050_attitude_t *attitude)
{
    if (attitude == 0)
    {
        return;
    }

    taskENTER_CRITICAL();
    *attitude = g_atk_ms6050_app_state.attitude;
    taskEXIT_CRITICAL();
}

uint8_t atk_ms6050_app_get_last_device_id(void)
{
    return atk_ms6050_get_last_device_id();
}

uint8_t atk_ms6050_app_get_last_init_step(void)
{
    return atk_ms6050_get_last_init_step();
}

uint8_t atk_ms6050_app_get_last_mpu_init_substep(void)
{
    return atk_ms6050_get_last_mpu_init_substep();
}

uint8_t atk_ms6050_app_get_last_mpu6500_rev(void)
{
    return atk_ms6050_get_last_mpu6500_rev();
}

uint8_t atk_ms6050_app_get_last_fifo_step(void)
{
    return atk_ms6050_get_last_fifo_step();
}

uint8_t atk_ms6050_app_get_last_fifo_reset_step(void)
{
    return atk_ms6050_get_last_fifo_reset_step();
}

uint8_t atk_ms6050_app_get_last_i2c_error_phase(void)
{
    return atk_ms6050_get_last_i2c_error_phase();
}

uint8_t atk_ms6050_app_get_last_i2c_error_dir(void)
{
    return atk_ms6050_get_last_i2c_error_dir();
}

uint8_t atk_ms6050_app_get_last_i2c_error_reg(void)
{
    return atk_ms6050_get_last_i2c_error_reg();
}

uint8_t atk_ms6050_app_get_last_i2c_error_index(void)
{
    return atk_ms6050_get_last_i2c_error_index();
}

const char *atk_ms6050_app_status_string(atk_ms6050_app_status_t status)
{
    switch (status)
    {
        case ATK_MS6050_APP_STATUS_IDLE:
            return "IDLE";

        case ATK_MS6050_APP_STATUS_INITING:
            return "INITING";

        case ATK_MS6050_APP_STATUS_READY:
            return "READY";

        case ATK_MS6050_APP_STATUS_WAIT_TIMEOUT:
            return "WAIT_TIMEOUT";

        case ATK_MS6050_APP_STATUS_I2C_FAIL:
            return "I2C_FAIL";

        case ATK_MS6050_APP_STATUS_ID_FAIL:
            return "ID_FAIL";

        case ATK_MS6050_APP_STATUS_DMP_FAIL:
            return "DMP_FAIL";

        case ATK_MS6050_APP_STATUS_SELF_TEST_FAIL:
            return "SELF_TEST_FAIL";

        case ATK_MS6050_APP_STATUS_FIFO_FAIL:
            return "FIFO_FAIL";

        default:
            return "UNKNOWN";
    }
}

void atk_ms6050_app_irq_handler(void)
{
    BaseType_t higher_priority_task_woken = pdFALSE;

    if (EXTI_GetITStatus(ATK_MS6050_INT_EXTI_LINE) != RESET)
    {
        atk_ms6050_int_exti_clear_flag();

        if (g_atk_ms6050_app_state.task_handle != 0)
        {
            vTaskNotifyGiveFromISR(g_atk_ms6050_app_state.task_handle, &higher_priority_task_woken);
            portYIELD_FROM_ISR(higher_priority_task_woken);
        }
    }
}
