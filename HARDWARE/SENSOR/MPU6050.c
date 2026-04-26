#include "MPU6050.h"
#include "atk_ms6050.h"
#include "atk_ms6050_iic.h"
#include "delay.h"

#define MPU6050_DATA_FRAME_LEN          14u
#define MPU6050_GYRO_LSB_PER_DPS        16.4f

static uint8_t mpu6050_map_status(uint8_t status)
{
    if (status == ATK_MS6050_EOK)
    {
        return MPU6050_OK;
    }

    if (status == ATK_MS6050_EID)
    {
        return MPU6050_ERR_ID;
    }

    return MPU6050_ERR_I2C;
}

static uint8_t mpu6050_prepare_bus(void)
{
    atk_ms6050_board_init();
    atk_ms6050_iic_init();
    delay_ms(20);
    return MPU6050_OK;
}

uint8_t MPU6050_ReadWhoAmI(uint8_t *who_am_i)
{
    uint8_t status;

    if (who_am_i == 0)
    {
        return MPU6050_ERR_PARAM;
    }

    mpu6050_prepare_bus();
    status = atk_ms6050_get_device_id(who_am_i);
    if (status != ATK_MS6050_EOK)
    {
        return mpu6050_map_status(status);
    }

    if ((*who_am_i != ATK_MS6050_DEVICE_ID) && (*who_am_i != ATK_MS6050_DEVICE_ID_ALT))
    {
        return MPU6050_ERR_ID;
    }

    return MPU6050_OK;
}

uint8_t MPU6050_Init(void)
{
    return mpu6050_map_status(atk_ms6050_init());
}

uint8_t MPU6050_ReadData(MPU6050_Data_t *data)
{
    uint8_t raw[MPU6050_DATA_FRAME_LEN];
    uint8_t status;

    if (data == 0)
    {
        return MPU6050_ERR_PARAM;
    }

    status = atk_ms6050_read(ATK_MS6050_IIC_ADDR, MPU_ACCEL_XOUTH_REG, MPU6050_DATA_FRAME_LEN, raw);
    if (status != ATK_MS6050_EOK)
    {
        return mpu6050_map_status(status);
    }

    data->accel_x_raw = (int16_t)(((uint16_t)raw[0] << 8) | raw[1]);
    data->accel_y_raw = (int16_t)(((uint16_t)raw[2] << 8) | raw[3]);
    data->accel_z_raw = (int16_t)(((uint16_t)raw[4] << 8) | raw[5]);
    data->temp_raw = (int16_t)(((uint16_t)raw[6] << 8) | raw[7]);
    data->gyro_x_raw = (int16_t)(((uint16_t)raw[8] << 8) | raw[9]);
    data->gyro_y_raw = (int16_t)(((uint16_t)raw[10] << 8) | raw[11]);
    data->gyro_z_raw = (int16_t)(((uint16_t)raw[12] << 8) | raw[13]);

    data->accel_x_g = (float)data->accel_x_raw / 16384.0f;
    data->accel_y_g = (float)data->accel_y_raw / 16384.0f;
    data->accel_z_g = (float)data->accel_z_raw / 16384.0f;
    data->temp_c = 36.53f + (float)data->temp_raw / 340.0f;
    data->gyro_x_dps = (float)data->gyro_x_raw / MPU6050_GYRO_LSB_PER_DPS;
    data->gyro_y_dps = (float)data->gyro_y_raw / MPU6050_GYRO_LSB_PER_DPS;
    data->gyro_z_dps = (float)data->gyro_z_raw / MPU6050_GYRO_LSB_PER_DPS;

    return MPU6050_OK;
}
