#include "App_I2C.h"

/* 降低超时：3000 × ~40 cycles / 72 MHz ≈ 1.7 ms，避免中断长时间阻塞。
 * 原值 30000 在中断内超时一次耗费约 16.7 ms，严重挤压主循环帧率。 */
#define APP_I2C_TIMEOUT  3000u

static void APP_I2C_ClearAddrFlag(void)
{
    (void)APP_I2C_INSTANCE->SR1;
    (void)APP_I2C_INSTANCE->SR2;
}

static uint8_t APP_I2C_WaitEvent(uint32_t event)
{
    uint32_t timeout = APP_I2C_TIMEOUT;
    while (I2C_CheckEvent(APP_I2C_INSTANCE, event) == ERROR)
    {
        if (timeout-- == 0u)
        {
            I2C_GenerateSTOP(APP_I2C_INSTANCE, ENABLE);   /* 释放总线，防止死锁 */
            return 1u;
        }
    }
    return 0u;
}

void APP_I2C_Init(void)
{
    GPIO_InitTypeDef gpio;
    I2C_InitTypeDef i2c;

    RCC_APB2PeriphClockCmd(APP_I2C_RCC_APB2_GPIO, ENABLE);
    RCC_APB1PeriphClockCmd(APP_I2C_RCC_APB1, ENABLE);

    gpio.GPIO_Pin = APP_I2C_SCL_PIN | APP_I2C_SDA_PIN;
    gpio.GPIO_Mode = GPIO_Mode_AF_OD;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(APP_I2C_GPIO_PORT, &gpio);

    I2C_DeInit(APP_I2C_INSTANCE);
    i2c.I2C_ClockSpeed = APP_I2C_SPEED;
    i2c.I2C_Mode = I2C_Mode_I2C;
    i2c.I2C_DutyCycle = I2C_DutyCycle_2;
    i2c.I2C_OwnAddress1 = 0x00;
    i2c.I2C_Ack = I2C_Ack_Enable;
    i2c.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_Init(APP_I2C_INSTANCE, &i2c);
    I2C_Cmd(APP_I2C_INSTANCE, ENABLE);
}

uint8_t APP_I2C_Write(uint8_t dev_addr_7bit, const uint8_t *data, uint8_t len)
{
    uint8_t i;

    if ((data == 0) && (len > 0u))
    {
        return 1u;
    }

    I2C_GenerateSTART(APP_I2C_INSTANCE, ENABLE);
    if (APP_I2C_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT)) return 1u;

    I2C_Send7bitAddress(APP_I2C_INSTANCE, (uint8_t)(dev_addr_7bit << 1), I2C_Direction_Transmitter);
    if (APP_I2C_WaitEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)) return 1u;

    for (i = 0u; i < len; i++)
    {
        I2C_SendData(APP_I2C_INSTANCE, data[i]);
        if (APP_I2C_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) return 1u;
    }

    I2C_GenerateSTOP(APP_I2C_INSTANCE, ENABLE);
    return 0u;
}

uint8_t APP_I2C_Read(uint8_t dev_addr_7bit, uint8_t *data, uint8_t len)
{
    uint8_t i;

    if ((data == 0) || (len == 0u))
    {
        return 1u;
    }

    I2C_GenerateSTART(APP_I2C_INSTANCE, ENABLE);
    if (APP_I2C_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT)) return 1u;

    if (len == 1u)
    {
        I2C_AcknowledgeConfig(APP_I2C_INSTANCE, DISABLE);
    }
    else
    {
        I2C_AcknowledgeConfig(APP_I2C_INSTANCE, ENABLE);
    }

    I2C_Send7bitAddress(APP_I2C_INSTANCE, (uint8_t)(dev_addr_7bit << 1), I2C_Direction_Receiver);
    if (APP_I2C_WaitEvent(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED)) return 1u;

    APP_I2C_ClearAddrFlag();

    for (i = 0u; i < len; i++)
    {
        if (i == (uint8_t)(len - 1u))
        {
            I2C_AcknowledgeConfig(APP_I2C_INSTANCE, DISABLE);
            I2C_GenerateSTOP(APP_I2C_INSTANCE, ENABLE);
        }

        if (APP_I2C_WaitEvent(I2C_EVENT_MASTER_BYTE_RECEIVED))
        {
            I2C_AcknowledgeConfig(APP_I2C_INSTANCE, ENABLE);
            return 1u;
        }

        data[i] = I2C_ReceiveData(APP_I2C_INSTANCE);
    }

    I2C_AcknowledgeConfig(APP_I2C_INSTANCE, ENABLE);
    return 0u;
}

uint8_t APP_I2C_WriteBytes(uint8_t dev_addr_7bit, uint8_t reg, const uint8_t *data, uint8_t len)
{
    uint8_t i;

    if ((data == 0) && (len > 0u))
    {
        return 1u;
    }

    I2C_GenerateSTART(APP_I2C_INSTANCE, ENABLE);
    if (APP_I2C_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT)) return 1u;

    I2C_Send7bitAddress(APP_I2C_INSTANCE, (uint8_t)(dev_addr_7bit << 1), I2C_Direction_Transmitter);
    if (APP_I2C_WaitEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)) return 1u;

    I2C_SendData(APP_I2C_INSTANCE, reg);
    if (APP_I2C_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) return 1u;

    for (i = 0u; i < len; i++)
    {
        I2C_SendData(APP_I2C_INSTANCE, data[i]);
        if (APP_I2C_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) return 1u;
    }

    I2C_GenerateSTOP(APP_I2C_INSTANCE, ENABLE);
    return 0u;
}

uint8_t APP_I2C_ReadBytes(uint8_t dev_addr_7bit, uint8_t reg, uint8_t *data, uint8_t len)
{
    if ((data == 0) || (len == 0u))
    {
        return 1u;
    }

    I2C_GenerateSTART(APP_I2C_INSTANCE, ENABLE);
    if (APP_I2C_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT)) return 1u;

    I2C_Send7bitAddress(APP_I2C_INSTANCE, (uint8_t)(dev_addr_7bit << 1), I2C_Direction_Transmitter);
    if (APP_I2C_WaitEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)) return 1u;

    I2C_SendData(APP_I2C_INSTANCE, reg);
    if (APP_I2C_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) return 1u;

    return APP_I2C_Read(dev_addr_7bit, data, len);
}

uint8_t APP_I2C_WriteByte(uint8_t dev_addr_7bit, uint8_t reg, uint8_t data)
{
    return APP_I2C_WriteBytes(dev_addr_7bit, reg, &data, 1u);
}

uint8_t APP_I2C_ReadByte(uint8_t dev_addr_7bit, uint8_t reg, uint8_t *data)
{
    return APP_I2C_ReadBytes(dev_addr_7bit, reg, data, 1u);
}
