#include "atk_ms6050.h"
#include "atk_ms6050_iic.h"
#include "delay.h"

static uint8_t g_atk_ms6050_last_device_id = 0xFF;
static uint8_t g_atk_ms6050_last_i2c_error_phase = 0;
static uint8_t g_atk_ms6050_last_i2c_error_dir = 0;
static uint8_t g_atk_ms6050_last_i2c_error_reg = 0xFF;
static uint8_t g_atk_ms6050_last_i2c_error_index = 0xFF;

static void atk_ms6050_record_i2c_error(uint8_t dir, uint8_t reg, uint8_t phase, uint8_t index)
{
    g_atk_ms6050_last_i2c_error_dir = dir;
    g_atk_ms6050_last_i2c_error_reg = reg;
    g_atk_ms6050_last_i2c_error_phase = phase;
    g_atk_ms6050_last_i2c_error_index = index;
}

void atk_ms6050_board_init(void)
{
    GPIO_InitTypeDef gpio_init_struct;

    RCC_APB2PeriphClockCmd(ATK_MS6050_AD0_GPIO_CLK | ATK_MS6050_INT_GPIO_CLK, ENABLE);

    gpio_init_struct.GPIO_Pin = ATK_MS6050_AD0_GPIO_PIN;
    gpio_init_struct.GPIO_Speed = GPIO_Speed_50MHz;
    gpio_init_struct.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(ATK_MS6050_AD0_GPIO_PORT, &gpio_init_struct);
    ATK_MS6050_AD0(0);

    gpio_init_struct.GPIO_Pin = ATK_MS6050_INT_GPIO_PIN;
    gpio_init_struct.GPIO_Speed = GPIO_Speed_50MHz;
    gpio_init_struct.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(ATK_MS6050_INT_GPIO_PORT, &gpio_init_struct);
}

void atk_ms6050_int_exti_init(void)
{
    EXTI_InitTypeDef exti_init_struct;
    NVIC_InitTypeDef nvic_init_struct;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    GPIO_EXTILineConfig(ATK_MS6050_INT_PORT_SOURCE, ATK_MS6050_INT_PIN_SOURCE);

    exti_init_struct.EXTI_Line = ATK_MS6050_INT_EXTI_LINE;
    exti_init_struct.EXTI_Mode = EXTI_Mode_Interrupt;
    exti_init_struct.EXTI_Trigger = EXTI_Trigger_Falling;
    exti_init_struct.EXTI_LineCmd = DISABLE;
    EXTI_Init(&exti_init_struct);

    nvic_init_struct.NVIC_IRQChannel = ATK_MS6050_INT_IRQn;
    nvic_init_struct.NVIC_IRQChannelPreemptionPriority = 6;
    nvic_init_struct.NVIC_IRQChannelSubPriority = 0;
    nvic_init_struct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic_init_struct);

    atk_ms6050_int_exti_clear_flag();
}

void atk_ms6050_int_exti_cmd(FunctionalState state)
{
    EXTI_InitTypeDef exti_init_struct;

    exti_init_struct.EXTI_Line = ATK_MS6050_INT_EXTI_LINE;
    exti_init_struct.EXTI_Mode = EXTI_Mode_Interrupt;
    exti_init_struct.EXTI_Trigger = EXTI_Trigger_Falling;
    exti_init_struct.EXTI_LineCmd = state;
    EXTI_Init(&exti_init_struct);

    atk_ms6050_int_exti_clear_flag();
}

void atk_ms6050_int_exti_clear_flag(void)
{
    EXTI_ClearITPendingBit(ATK_MS6050_INT_EXTI_LINE);
}

uint8_t atk_ms6050_int_pin_read(void)
{
    return (uint8_t)GPIO_ReadInputDataBit(ATK_MS6050_INT_GPIO_PORT, ATK_MS6050_INT_GPIO_PIN);
}

uint8_t atk_ms6050_write(uint8_t addr, uint8_t reg, uint8_t len, uint8_t *dat)
{
    uint8_t i;

    atk_ms6050_iic_start();
    atk_ms6050_iic_send_byte((uint8_t)((addr << 1) | 0));
    if (atk_ms6050_iic_wait_ack())
    {
        atk_ms6050_record_i2c_error(0, reg, 1, 0xFF);
        atk_ms6050_iic_stop();
        return ATK_MS6050_EACK;
    }

    atk_ms6050_iic_send_byte(reg);
    if (atk_ms6050_iic_wait_ack())
    {
        atk_ms6050_record_i2c_error(0, reg, 2, 0xFF);
        atk_ms6050_iic_stop();
        return ATK_MS6050_EACK;
    }

    for (i = 0; i < len; i++)
    {
        atk_ms6050_iic_send_byte(dat[i]);
        if (atk_ms6050_iic_wait_ack())
        {
            atk_ms6050_record_i2c_error(0, reg, 3, i);
            atk_ms6050_iic_stop();
            return ATK_MS6050_EACK;
        }
    }

    atk_ms6050_iic_stop();
    return ATK_MS6050_EOK;
}

uint8_t atk_ms6050_write_byte(uint8_t addr, uint8_t reg, uint8_t dat)
{
    return atk_ms6050_write(addr, reg, 1, &dat);
}

uint8_t atk_ms6050_read(uint8_t addr, uint8_t reg, uint8_t len, uint8_t *dat)
{
    atk_ms6050_iic_start();
    atk_ms6050_iic_send_byte((uint8_t)((addr << 1) | 0));
    if (atk_ms6050_iic_wait_ack())
    {
        atk_ms6050_record_i2c_error(1, reg, 1, 0xFF);
        atk_ms6050_iic_stop();
        return ATK_MS6050_EACK;
    }

    atk_ms6050_iic_send_byte(reg);
    if (atk_ms6050_iic_wait_ack())
    {
        atk_ms6050_record_i2c_error(1, reg, 2, 0xFF);
        atk_ms6050_iic_stop();
        return ATK_MS6050_EACK;
    }

    atk_ms6050_iic_start();
    atk_ms6050_iic_send_byte((uint8_t)((addr << 1) | 1));
    if (atk_ms6050_iic_wait_ack())
    {
        atk_ms6050_record_i2c_error(1, reg, 4, 0xFF);
        atk_ms6050_iic_stop();
        return ATK_MS6050_EACK;
    }

    while (len)
    {
        *dat = atk_ms6050_iic_read_byte((uint8_t)(len > 1));
        dat++;
        len--;
    }

    atk_ms6050_iic_stop();
    return ATK_MS6050_EOK;
}

uint8_t atk_ms6050_read_byte(uint8_t addr, uint8_t reg, uint8_t *dat)
{
    return atk_ms6050_read(addr, reg, 1, dat);
}

uint8_t atk_ms6050_get_device_id(uint8_t *id)
{
    if (id == 0)
    {
        return ATK_MS6050_EID;
    }

    return atk_ms6050_read_byte(ATK_MS6050_IIC_ADDR, MPU_DEVICE_ID_REG, id);
}

uint8_t atk_ms6050_get_last_device_id(void)
{
    return g_atk_ms6050_last_device_id;
}

void atk_ms6050_clear_last_i2c_error(void)
{
    g_atk_ms6050_last_i2c_error_phase = 0;
    g_atk_ms6050_last_i2c_error_dir = 0;
    g_atk_ms6050_last_i2c_error_reg = 0xFF;
    g_atk_ms6050_last_i2c_error_index = 0xFF;
}

uint8_t atk_ms6050_get_last_i2c_error_phase(void)
{
    return g_atk_ms6050_last_i2c_error_phase;
}

uint8_t atk_ms6050_get_last_i2c_error_dir(void)
{
    return g_atk_ms6050_last_i2c_error_dir;
}

uint8_t atk_ms6050_get_last_i2c_error_reg(void)
{
    return g_atk_ms6050_last_i2c_error_reg;
}

uint8_t atk_ms6050_get_last_i2c_error_index(void)
{
    return g_atk_ms6050_last_i2c_error_index;
}

uint8_t atk_ms6050_device_check(void)
{
    uint8_t id;
    uint8_t ret;

    ret = atk_ms6050_get_device_id(&id);
    if (ret != ATK_MS6050_EOK)
    {
        return ret;
    }

    g_atk_ms6050_last_device_id = id;

    if ((id == ATK_MS6050_DEVICE_ID) || (id == ATK_MS6050_DEVICE_ID_ALT))
    {
        return ATK_MS6050_EOK;
    }

    return ATK_MS6050_EID;
}

void atk_ms6050_sw_reset(void)
{
    atk_ms6050_write_byte(ATK_MS6050_IIC_ADDR, MPU_PWR_MGMT1_REG, 0x80);
    delay_ms(100);
    atk_ms6050_write_byte(ATK_MS6050_IIC_ADDR, MPU_PWR_MGMT1_REG, 0x00);
    delay_ms(10);
}

uint8_t atk_ms6050_set_gyro_fsr(uint8_t fsr)
{
    return atk_ms6050_write_byte(ATK_MS6050_IIC_ADDR, MPU_GYRO_CFG_REG, (uint8_t)(fsr << 3));
}

uint8_t atk_ms6050_set_accel_fsr(uint8_t fsr)
{
    return atk_ms6050_write_byte(ATK_MS6050_IIC_ADDR, MPU_ACCEL_CFG_REG, (uint8_t)(fsr << 3));
}

uint8_t atk_ms6050_set_lpf(uint16_t lpf)
{
    uint8_t dat;

    if (lpf >= 188)
    {
        dat = 1;
    }
    else if (lpf >= 98)
    {
        dat = 2;
    }
    else if (lpf >= 42)
    {
        dat = 3;
    }
    else if (lpf >= 20)
    {
        dat = 4;
    }
    else if (lpf >= 10)
    {
        dat = 5;
    }
    else
    {
        dat = 6;
    }

    return atk_ms6050_write_byte(ATK_MS6050_IIC_ADDR, MPU_CFG_REG, dat);
}

uint8_t atk_ms6050_set_rate(uint16_t rate)
{
    uint8_t ret;
    uint8_t dat;

    if (rate > 1000)
    {
        rate = 1000;
    }
    if (rate < 4)
    {
        rate = 4;
    }

    dat = (uint8_t)(1000 / rate - 1);
    ret = atk_ms6050_write_byte(ATK_MS6050_IIC_ADDR, MPU_SAMPLE_RATE_REG, dat);
    if (ret != ATK_MS6050_EOK)
    {
        return ret;
    }

    return atk_ms6050_set_lpf((uint16_t)(rate >> 1));
}

uint8_t atk_ms6050_get_temperature(int16_t *temp)
{
    uint8_t dat[2];
    uint8_t ret;
    int16_t raw;

    if (temp == 0)
    {
        return ATK_MS6050_EID;
    }

    ret = atk_ms6050_read(ATK_MS6050_IIC_ADDR, MPU_TEMP_OUTH_REG, 2, dat);
    if (ret != ATK_MS6050_EOK)
    {
        return ret;
    }

    raw = (int16_t)(((uint16_t)dat[0] << 8) | dat[1]);
    *temp = (int16_t)(3653 + ((int32_t)raw * 100) / 340);
    return ATK_MS6050_EOK;
}

uint8_t atk_ms6050_get_gyroscope(int16_t *gx, int16_t *gy, int16_t *gz)
{
    uint8_t dat[6];
    uint8_t ret;

    ret = atk_ms6050_read(ATK_MS6050_IIC_ADDR, MPU_GYRO_XOUTH_REG, 6, dat);
    if (ret == ATK_MS6050_EOK)
    {
        *gx = (int16_t)(((uint16_t)dat[0] << 8) | dat[1]);
        *gy = (int16_t)(((uint16_t)dat[2] << 8) | dat[3]);
        *gz = (int16_t)(((uint16_t)dat[4] << 8) | dat[5]);
    }

    return ret;
}

uint8_t atk_ms6050_get_accelerometer(int16_t *ax, int16_t *ay, int16_t *az)
{
    uint8_t dat[6];
    uint8_t ret;

    ret = atk_ms6050_read(ATK_MS6050_IIC_ADDR, MPU_ACCEL_XOUTH_REG, 6, dat);
    if (ret == ATK_MS6050_EOK)
    {
        *ax = (int16_t)(((uint16_t)dat[0] << 8) | dat[1]);
        *ay = (int16_t)(((uint16_t)dat[2] << 8) | dat[3]);
        *az = (int16_t)(((uint16_t)dat[4] << 8) | dat[5]);
    }

    return ret;
}

uint8_t atk_ms6050_init(void)
{
    uint8_t ret;

    atk_ms6050_board_init();
    atk_ms6050_iic_init();
    delay_ms(20);

    ret = atk_ms6050_device_check();
    if (ret != ATK_MS6050_EOK)
    {
        return ret;
    }

    atk_ms6050_sw_reset();

    ret = atk_ms6050_set_gyro_fsr(3);
    if (ret != ATK_MS6050_EOK)
    {
        return ret;
    }

    ret = atk_ms6050_set_accel_fsr(0);
    if (ret != ATK_MS6050_EOK)
    {
        return ret;
    }

    ret = atk_ms6050_set_rate(100);
    if (ret != ATK_MS6050_EOK)
    {
        return ret;
    }

    ret = atk_ms6050_write_byte(ATK_MS6050_IIC_ADDR, MPU_INT_EN_REG, 0x00);
    if (ret != ATK_MS6050_EOK)
    {
        return ret;
    }

    ret = atk_ms6050_write_byte(ATK_MS6050_IIC_ADDR, MPU_USER_CTRL_REG, 0x00);
    if (ret != ATK_MS6050_EOK)
    {
        return ret;
    }

    ret = atk_ms6050_write_byte(ATK_MS6050_IIC_ADDR, MPU_FIFO_EN_REG, 0x00);
    if (ret != ATK_MS6050_EOK)
    {
        return ret;
    }

    ret = atk_ms6050_write_byte(ATK_MS6050_IIC_ADDR, MPU_INTBP_CFG_REG, 0x80);
    if (ret != ATK_MS6050_EOK)
    {
        return ret;
    }

    ret = atk_ms6050_write_byte(ATK_MS6050_IIC_ADDR, MPU_PWR_MGMT1_REG, 0x01);
    if (ret != ATK_MS6050_EOK)
    {
        return ret;
    }

    ret = atk_ms6050_write_byte(ATK_MS6050_IIC_ADDR, MPU_PWR_MGMT2_REG, 0x00);
    if (ret != ATK_MS6050_EOK)
    {
        return ret;
    }

    atk_ms6050_int_exti_clear_flag();
    return ATK_MS6050_EOK;
}
