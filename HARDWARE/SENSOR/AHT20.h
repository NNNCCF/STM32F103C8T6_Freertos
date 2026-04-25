#ifndef __AHT20_H
#define __AHT20_H

#include "stm32f10x.h"

typedef struct
{
    float temperature_c;
    float humidity_rh;
} AHT20_Data_t;

#define AHT20_ADDR           0x38u
#define AHT20_OK             0u
#define AHT20_ERR_I2C        1u
#define AHT20_ERR_BUSY       2u
#define AHT20_ERR_PARAM      3u

uint8_t AHT20_Init(void);
uint8_t AHT20_ReadData(AHT20_Data_t *data);

#endif
