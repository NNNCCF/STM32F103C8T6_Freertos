#ifndef __BMP280_H
#define __BMP280_H

#include "stm32f10x.h"

typedef struct
{
    float temperature_c;
    float pressure_pa;
} BMP280_Data_t;

#define BMP280_ADDR          0x76u
#define BMP280_OK            0u
#define BMP280_ERR_I2C       1u
#define BMP280_ERR_ID        2u
#define BMP280_ERR_PARAM     3u

uint8_t BMP280_Init(void);
uint8_t BMP280_ReadData(BMP280_Data_t *data);

#endif
