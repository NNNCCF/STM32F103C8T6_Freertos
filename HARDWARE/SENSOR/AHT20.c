#include "AHT20.h"
#include "App_I2C.h"
#include "delay.h"

#define AHT20_REG_STATUS       0x71u
#define AHT20_CMD_INIT         0xBEu
#define AHT20_CMD_MEASURE      0xACu

static uint8_t AHT20_ReadStatus(uint8_t *status)
{
    uint8_t cmd;

    if (status == 0)
    {
        return AHT20_ERR_PARAM;
    }

    cmd = AHT20_REG_STATUS;

    if (APP_I2C_Write(AHT20_ADDR, &cmd, 1u)) return AHT20_ERR_I2C;
    if (APP_I2C_Read(AHT20_ADDR, status, 1u)) return AHT20_ERR_I2C;

    return AHT20_OK;
}

static uint8_t AHT20_WriteCmd3(uint8_t cmd, uint8_t d1, uint8_t d2)
{
    uint8_t buf[3];

    buf[0] = cmd;
    buf[1] = d1;
    buf[2] = d2;

    return APP_I2C_Write(AHT20_ADDR, buf, 3u);
}

uint8_t AHT20_Init(void)
{
    uint8_t status;

    APP_I2C_Init();
    delay_ms(40);

    if (AHT20_ReadStatus(&status)) return AHT20_ERR_I2C;

    if ((status & 0x08u) == 0u)
    {
        if (AHT20_WriteCmd3(AHT20_CMD_INIT, 0x08u, 0x00u)) return AHT20_ERR_I2C;
        delay_ms(20);
    }

    return AHT20_OK;
}

uint8_t AHT20_ReadData(AHT20_Data_t *data)
{
    uint8_t raw[6];
    uint32_t h_raw;
    uint32_t t_raw;
    uint8_t i;

    if (data == 0) return AHT20_ERR_PARAM;

    if (AHT20_WriteCmd3(AHT20_CMD_MEASURE, 0x33u, 0x00u)) return AHT20_ERR_I2C;

    delay_ms(80);

    if (APP_I2C_Read(AHT20_ADDR, raw, 6u)) return AHT20_ERR_I2C;

    for (i = 0u; i < 10u; i++)
    {
        if ((raw[0] & 0x80u) == 0u)
        {
            break;
        }
        delay_ms(10);
        if (APP_I2C_Read(AHT20_ADDR, raw, 6u)) return AHT20_ERR_I2C;
    }

    if (raw[0] & 0x80u) return AHT20_ERR_BUSY;

    h_raw = ((uint32_t)raw[1] << 12) | ((uint32_t)raw[2] << 4) | ((uint32_t)raw[3] >> 4);
    t_raw = (((uint32_t)raw[3] & 0x0Fu) << 16) | ((uint32_t)raw[4] << 8) | (uint32_t)raw[5];

    data->humidity_rh = ((float)h_raw * 100.0f) / 1048576.0f;
    data->temperature_c = ((float)t_raw * 200.0f) / 1048576.0f - 50.0f;

    return AHT20_OK;
}
