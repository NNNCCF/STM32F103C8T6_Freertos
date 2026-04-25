#include "BMP280.h"
#include "App_I2C.h"
#include "delay.h"

#define BMP280_REG_ID         0xD0u
#define BMP280_REG_RESET      0xE0u
#define BMP280_REG_CTRL_MEAS  0xF4u
#define BMP280_REG_CONFIG     0xF5u
#define BMP280_REG_PRESS_MSB  0xF7u
#define BMP280_REG_CALIB00    0x88u

static uint8_t bmp280_addr = BMP280_ADDR;

static uint16_t dig_T1;
static int16_t dig_T2;
static int16_t dig_T3;
static uint16_t dig_P1;
static int16_t dig_P2;
static int16_t dig_P3;
static int16_t dig_P4;
static int16_t dig_P5;
static int16_t dig_P6;
static int16_t dig_P7;
static int16_t dig_P8;
static int16_t dig_P9;
static int32_t t_fine;

static int32_t BMP280_CompensateTemp(int32_t adc_T)
{
    int32_t var1;
    int32_t var2;

    var1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1))) * ((int32_t)dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)dig_T1)) * ((adc_T >> 4) - ((int32_t)dig_T1))) >> 12) * ((int32_t)dig_T3)) >> 14;
    t_fine = var1 + var2;

    return (t_fine * 5 + 128) >> 8;
}

static uint32_t BMP280_CompensatePress(int32_t adc_P)
{
    int64_t var1;
    int64_t var2;
    int64_t p;

    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)dig_P6;
    var2 = var2 + ((var1 * (int64_t)dig_P5) << 17);
    var2 = var2 + (((int64_t)dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)dig_P3) >> 8) + ((var1 * (int64_t)dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)dig_P1) >> 33;

    if (var1 == 0)
    {
        return 0u;
    }

    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)dig_P8) * p) >> 19;

    p = ((p + var1 + var2) >> 8) + (((int64_t)dig_P7) << 4);
    return (uint32_t)(p >> 8);
}

uint8_t BMP280_Init(void)
{
    uint8_t id;
    uint8_t calib[24];

    APP_I2C_Init();

    bmp280_addr = BMP280_ADDR;

    if (APP_I2C_ReadByte(bmp280_addr, BMP280_REG_ID, &id) || (id != 0x58u))
    {
        bmp280_addr = 0x77u;
        if (APP_I2C_ReadByte(bmp280_addr, BMP280_REG_ID, &id)) return BMP280_ERR_I2C;
        if (id != 0x58u) return BMP280_ERR_ID;
    }

    if (APP_I2C_WriteByte(bmp280_addr, BMP280_REG_RESET, 0xB6u)) return BMP280_ERR_I2C;
    delay_ms(5);

    if (APP_I2C_ReadBytes(bmp280_addr, BMP280_REG_CALIB00, calib, 24u)) return BMP280_ERR_I2C;

    dig_T1 = (uint16_t)((calib[1] << 8) | calib[0]);
    dig_T2 = (int16_t)((calib[3] << 8) | calib[2]);
    dig_T3 = (int16_t)((calib[5] << 8) | calib[4]);
    dig_P1 = (uint16_t)((calib[7] << 8) | calib[6]);
    dig_P2 = (int16_t)((calib[9] << 8) | calib[8]);
    dig_P3 = (int16_t)((calib[11] << 8) | calib[10]);
    dig_P4 = (int16_t)((calib[13] << 8) | calib[12]);
    dig_P5 = (int16_t)((calib[15] << 8) | calib[14]);
    dig_P6 = (int16_t)((calib[17] << 8) | calib[16]);
    dig_P7 = (int16_t)((calib[19] << 8) | calib[18]);
    dig_P8 = (int16_t)((calib[21] << 8) | calib[20]);
    dig_P9 = (int16_t)((calib[23] << 8) | calib[22]);

    if (APP_I2C_WriteByte(bmp280_addr, BMP280_REG_CTRL_MEAS, 0x27u)) return BMP280_ERR_I2C;
    if (APP_I2C_WriteByte(bmp280_addr, BMP280_REG_CONFIG, 0xA0u)) return BMP280_ERR_I2C;

    return BMP280_OK;
}

uint8_t BMP280_ReadData(BMP280_Data_t *data)
{
    uint8_t raw[6];
    int32_t adc_P;
    int32_t adc_T;
    int32_t t_x100;
    uint32_t p_pa;

    if (data == 0) return BMP280_ERR_PARAM;

    if (APP_I2C_ReadBytes(bmp280_addr, BMP280_REG_PRESS_MSB, raw, 6u)) return BMP280_ERR_I2C;

    adc_P = (int32_t)(((uint32_t)raw[0] << 12) | ((uint32_t)raw[1] << 4) | (raw[2] >> 4));
    adc_T = (int32_t)(((uint32_t)raw[3] << 12) | ((uint32_t)raw[4] << 4) | (raw[5] >> 4));

    t_x100 = BMP280_CompensateTemp(adc_T);
    p_pa = BMP280_CompensatePress(adc_P);

    data->temperature_c = (float)t_x100 / 100.0f;
    data->pressure_pa = (float)p_pa;

    return BMP280_OK;
}
