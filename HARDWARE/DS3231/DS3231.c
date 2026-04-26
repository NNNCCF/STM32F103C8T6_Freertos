#include "DS3231.h"
#include "delay.h"

#define DS3231_REG_TIME      0x00u
#define DS3231_REG_CONTROL   0x0Eu
#define DS3231_I2C_GPIO_CLK  RCC_APB2Periph_GPIOB
#define DS3231_I2C_GPIO_PORT GPIOB
#define DS3231_I2C_SCL_PIN   GPIO_Pin_8
#define DS3231_I2C_SDA_PIN   GPIO_Pin_9
#define DS3231_I2C_DELAY_US  4u

static void DS3231_I2C_Delay(void)
{
    delay_us(DS3231_I2C_DELAY_US);
}

static void DS3231_I2C_SCL_Set(void)
{
    GPIO_SetBits(DS3231_I2C_GPIO_PORT, DS3231_I2C_SCL_PIN);
}

static void DS3231_I2C_SCL_Clr(void)
{
    GPIO_ResetBits(DS3231_I2C_GPIO_PORT, DS3231_I2C_SCL_PIN);
}

static void DS3231_I2C_SDA_Set(void)
{
    GPIO_SetBits(DS3231_I2C_GPIO_PORT, DS3231_I2C_SDA_PIN);
}

static void DS3231_I2C_SDA_Clr(void)
{
    GPIO_ResetBits(DS3231_I2C_GPIO_PORT, DS3231_I2C_SDA_PIN);
}

static uint8_t DS3231_I2C_SDA_Read(void)
{
    return (uint8_t)GPIO_ReadInputDataBit(DS3231_I2C_GPIO_PORT, DS3231_I2C_SDA_PIN);
}

static void DS3231_I2C_Start(void)
{
    DS3231_I2C_SDA_Set();
    DS3231_I2C_SCL_Set();
    DS3231_I2C_Delay();
    DS3231_I2C_SDA_Clr();
    DS3231_I2C_Delay();
    DS3231_I2C_SCL_Clr();
}

static void DS3231_I2C_Stop(void)
{
    DS3231_I2C_SDA_Clr();
    DS3231_I2C_Delay();
    DS3231_I2C_SCL_Set();
    DS3231_I2C_Delay();
    DS3231_I2C_SDA_Set();
    DS3231_I2C_Delay();
}

static uint8_t DS3231_I2C_WaitAck(void)
{
    uint8_t timeout;

    timeout = 0u;
    DS3231_I2C_SDA_Set();
    DS3231_I2C_Delay();
    DS3231_I2C_SCL_Set();
    DS3231_I2C_Delay();

    while (DS3231_I2C_SDA_Read() != 0u)
    {
        timeout++;
        if (timeout > 20u)
        {
            DS3231_I2C_Stop();
            return 1u;
        }
        DS3231_I2C_Delay();
    }

    DS3231_I2C_SCL_Clr();
    return 0u;
}

static void DS3231_I2C_SendAck(uint8_t ack)
{
    DS3231_I2C_SCL_Clr();

    if (ack != 0u)
    {
        DS3231_I2C_SDA_Set();
    }
    else
    {
        DS3231_I2C_SDA_Clr();
    }

    DS3231_I2C_Delay();
    DS3231_I2C_SCL_Set();
    DS3231_I2C_Delay();
    DS3231_I2C_SCL_Clr();
    DS3231_I2C_SDA_Set();
}

static void DS3231_I2C_WriteByte(uint8_t data)
{
    uint8_t bit;

    for (bit = 0u; bit < 8u; bit++)
    {
        DS3231_I2C_SCL_Clr();
        if ((data & 0x80u) != 0u)
        {
            DS3231_I2C_SDA_Set();
        }
        else
        {
            DS3231_I2C_SDA_Clr();
        }

        data <<= 1;
        DS3231_I2C_Delay();
        DS3231_I2C_SCL_Set();
        DS3231_I2C_Delay();
    }

    DS3231_I2C_SCL_Clr();
}

static uint8_t DS3231_I2C_ReadByte(uint8_t ack)
{
    uint8_t bit;
    uint8_t data;

    data = 0u;
    DS3231_I2C_SDA_Set();

    for (bit = 0u; bit < 8u; bit++)
    {
        data <<= 1;
        DS3231_I2C_SCL_Clr();
        DS3231_I2C_Delay();
        DS3231_I2C_SCL_Set();
        if (DS3231_I2C_SDA_Read() != 0u)
        {
            data |= 0x01u;
        }
        DS3231_I2C_Delay();
    }

    DS3231_I2C_SCL_Clr();
    DS3231_I2C_SendAck(ack);
    return data;
}

static void DS3231_I2C_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(DS3231_I2C_GPIO_CLK, ENABLE);

    gpio.GPIO_Pin = DS3231_I2C_SCL_PIN | DS3231_I2C_SDA_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_OD;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(DS3231_I2C_GPIO_PORT, &gpio);

    DS3231_I2C_SDA_Set();
    DS3231_I2C_SCL_Set();
}

static uint8_t DS3231_WriteBytes(uint8_t reg, const uint8_t *data, uint8_t len)
{
    uint8_t i;

    DS3231_I2C_Start();
    DS3231_I2C_WriteByte((uint8_t)(DS3231_ADDR << 1));
    if (DS3231_I2C_WaitAck() != 0u) return 1u;

    DS3231_I2C_WriteByte(reg);
    if (DS3231_I2C_WaitAck() != 0u) return 1u;

    for (i = 0u; i < len; i++)
    {
        DS3231_I2C_WriteByte(data[i]);
        if (DS3231_I2C_WaitAck() != 0u) return 1u;
    }

    DS3231_I2C_Stop();
    return 0u;
}

static uint8_t DS3231_ReadBytes(uint8_t reg, uint8_t *data, uint8_t len)
{
    uint8_t i;

    if ((data == 0) || (len == 0u))
    {
        return 1u;
    }

    DS3231_I2C_Start();
    DS3231_I2C_WriteByte((uint8_t)(DS3231_ADDR << 1));
    if (DS3231_I2C_WaitAck() != 0u) return 1u;

    DS3231_I2C_WriteByte(reg);
    if (DS3231_I2C_WaitAck() != 0u) return 1u;

    DS3231_I2C_Start();
    DS3231_I2C_WriteByte((uint8_t)((DS3231_ADDR << 1) | 0x01u));
    if (DS3231_I2C_WaitAck() != 0u) return 1u;

    for (i = 0u; i < len; i++)
    {
        data[i] = DS3231_I2C_ReadByte((uint8_t)(i == (uint8_t)(len - 1u)));
    }

    DS3231_I2C_Stop();
    return 0u;
}

static uint8_t DS3231_DecToBcd(uint8_t dec)
{
    return (uint8_t)(((dec / 10u) << 4) | (dec % 10u));
}

static uint8_t DS3231_BcdToDec(uint8_t bcd)
{
    return (uint8_t)(((bcd >> 4) * 10u) + (bcd & 0x0Fu));
}

uint8_t DS3231_Init(void)
{
    uint8_t control;

    DS3231_I2C_Init();
    control = 0x00u;
    if (DS3231_WriteBytes(DS3231_REG_CONTROL, &control, 1u) != 0u) return DS3231_ERR_I2C;
    return DS3231_OK;
}

uint8_t DS3231_SetTime(const DS3231_Time_t *time)
{
    uint8_t buf[7];

    if (time == 0) return DS3231_ERR_PARAM;

    buf[0] = DS3231_DecToBcd(time->sec);
    buf[1] = DS3231_DecToBcd(time->min);
    buf[2] = DS3231_DecToBcd(time->hour);
    buf[3] = DS3231_DecToBcd(time->week);
    buf[4] = DS3231_DecToBcd(time->day);
    buf[5] = DS3231_DecToBcd(time->month);
    buf[6] = DS3231_DecToBcd(time->year);

    if (DS3231_WriteBytes(DS3231_REG_TIME, buf, 7u) != 0u) return DS3231_ERR_I2C;
    return DS3231_OK;
}

uint8_t DS3231_GetTime(DS3231_Time_t *time)
{
    uint8_t buf[7];

    if (time == 0) return DS3231_ERR_PARAM;

    if (DS3231_ReadBytes(DS3231_REG_TIME, buf, 7u) != 0u) return DS3231_ERR_I2C;

    time->sec = DS3231_BcdToDec((uint8_t)(buf[0] & 0x7Fu));
    time->min = DS3231_BcdToDec((uint8_t)(buf[1] & 0x7Fu));
    time->hour = DS3231_BcdToDec((uint8_t)(buf[2] & 0x3Fu));
    time->week = DS3231_BcdToDec((uint8_t)(buf[3] & 0x07u));
    time->day = DS3231_BcdToDec((uint8_t)(buf[4] & 0x3Fu));
    time->month = DS3231_BcdToDec((uint8_t)(buf[5] & 0x1Fu));
    time->year = DS3231_BcdToDec(buf[6]);

    return DS3231_OK;
}
