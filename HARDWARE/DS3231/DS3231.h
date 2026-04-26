#ifndef __DS3231_H
#define __DS3231_H

#include "stm32f10x.h"

typedef struct
{
    uint8_t year;   /* 0-99 */
    uint8_t month;  /* 1-12 */
    uint8_t day;    /* 1-31 */
    uint8_t week;   /* 1-7 */
    uint8_t hour;   /* 0-23 */
    uint8_t min;    /* 0-59 */
    uint8_t sec;    /* 0-59 */
} DS3231_Time_t;

#define DS3231_ADDR         0x68u
#define DS3231_OK           0u
#define DS3231_ERR_I2C      1u
#define DS3231_ERR_PARAM    2u

uint8_t DS3231_Init(void);
uint8_t DS3231_SetTime(const DS3231_Time_t *time);
uint8_t DS3231_GetTime(DS3231_Time_t *time);

#endif
