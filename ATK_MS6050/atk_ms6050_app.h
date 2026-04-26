#ifndef __ATK_MS6050_APP_H
#define __ATK_MS6050_APP_H

#include "sys.h"
#include "includes.h"

typedef enum
{
    ATK_MS6050_APP_STATUS_IDLE = 0,
    ATK_MS6050_APP_STATUS_INITING,
    ATK_MS6050_APP_STATUS_READY,
    ATK_MS6050_APP_STATUS_WAIT_TIMEOUT,
    ATK_MS6050_APP_STATUS_I2C_FAIL,
    ATK_MS6050_APP_STATUS_ID_FAIL,
    ATK_MS6050_APP_STATUS_DMP_FAIL,
    ATK_MS6050_APP_STATUS_SELF_TEST_FAIL,
    ATK_MS6050_APP_STATUS_FIFO_FAIL
} atk_ms6050_app_status_t;

typedef struct
{
    float pitch;
    float roll;
    float yaw;
    uint32_t timestamp_ms;
    uint32_t sample_count;
    uint8_t valid;
    atk_ms6050_app_status_t status;
} atk_ms6050_attitude_t;

uint8_t atk_ms6050_app_init(TaskHandle_t task_handle);
uint8_t atk_ms6050_app_wait_and_sample(TickType_t timeout_ticks);
void atk_ms6050_app_get_attitude(atk_ms6050_attitude_t *attitude);
uint8_t atk_ms6050_app_get_last_device_id(void);
uint8_t atk_ms6050_app_get_last_init_step(void);
uint8_t atk_ms6050_app_get_last_mpu_init_substep(void);
uint8_t atk_ms6050_app_get_last_mpu6500_rev(void);
uint8_t atk_ms6050_app_get_last_fifo_step(void);
uint8_t atk_ms6050_app_get_last_fifo_reset_step(void);
uint8_t atk_ms6050_app_get_last_i2c_error_phase(void);
uint8_t atk_ms6050_app_get_last_i2c_error_dir(void);
uint8_t atk_ms6050_app_get_last_i2c_error_reg(void);
uint8_t atk_ms6050_app_get_last_i2c_error_index(void);
const char *atk_ms6050_app_status_string(atk_ms6050_app_status_t status);
void atk_ms6050_app_irq_handler(void);

#endif
