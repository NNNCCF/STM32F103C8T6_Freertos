#ifndef __MPU6050_H
#define __MPU6050_H

#include "stm32f10x.h"

typedef struct
{
    int16_t accel_x_raw;
    int16_t accel_y_raw;
    int16_t accel_z_raw;
    int16_t temp_raw;
    int16_t gyro_x_raw;
    int16_t gyro_y_raw;
    int16_t gyro_z_raw;
    float accel_x_g;
    float accel_y_g;
    float accel_z_g;
    float temp_c;
    float gyro_x_dps;
    float gyro_y_dps;
    float gyro_z_dps;
} MPU6050_Data_t;

#define MPU6050_ADDR_LOW      0x68u
#define MPU6050_ADDR_HIGH     0x69u
#define MPU6050_OK            0u
#define MPU6050_ERR_I2C       1u
#define MPU6050_ERR_ID        2u
#define MPU6050_ERR_PARAM     3u

uint8_t MPU6050_Init(void);
uint8_t MPU6050_ReadWhoAmI(uint8_t *who_am_i);
uint8_t MPU6050_ReadData(MPU6050_Data_t *data);

#endif
