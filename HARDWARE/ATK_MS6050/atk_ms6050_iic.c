#include "atk_ms6050_iic.h"
#include "delay.h"

static void atk_ms6050_iic_delay(void)
{
    delay_us(4);
}

static void atk_ms6050_iic_sda_out(void)
{
    GPIO_InitTypeDef gpio_init_struct;

    gpio_init_struct.GPIO_Pin = ATK_MS6050_IIC_SDA_GPIO_PIN;
    gpio_init_struct.GPIO_Speed = GPIO_Speed_50MHz;
    gpio_init_struct.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_Init(ATK_MS6050_IIC_SDA_GPIO_PORT, &gpio_init_struct);
}

static void atk_ms6050_iic_sda_in(void)
{
    GPIO_InitTypeDef gpio_init_struct;

    gpio_init_struct.GPIO_Pin = ATK_MS6050_IIC_SDA_GPIO_PIN;
    gpio_init_struct.GPIO_Speed = GPIO_Speed_50MHz;
    gpio_init_struct.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(ATK_MS6050_IIC_SDA_GPIO_PORT, &gpio_init_struct);
    ATK_MS6050_IIC_SDA(1);
}

static void atk_ms6050_iic_bus_recover(void)
{
    uint8_t i;

    atk_ms6050_iic_sda_in();
    ATK_MS6050_IIC_SDA(1);

    for (i = 0; i < 9; i++)
    {
        if (ATK_MS6050_IIC_READ_SDA() == Bit_SET)
        {
            break;
        }

        ATK_MS6050_IIC_SCL(1);
        atk_ms6050_iic_delay();
        ATK_MS6050_IIC_SCL(0);
        atk_ms6050_iic_delay();
    }

    atk_ms6050_iic_sda_out();
}

void atk_ms6050_iic_start(void)
{
    atk_ms6050_iic_sda_out();
    ATK_MS6050_IIC_SDA(1);
    ATK_MS6050_IIC_SCL(1);
    atk_ms6050_iic_delay();
    ATK_MS6050_IIC_SDA(0);
    atk_ms6050_iic_delay();
    ATK_MS6050_IIC_SCL(0);
}

void atk_ms6050_iic_stop(void)
{
    atk_ms6050_iic_sda_out();
    ATK_MS6050_IIC_SCL(0);
    ATK_MS6050_IIC_SDA(0);
    atk_ms6050_iic_delay();
    ATK_MS6050_IIC_SCL(1);
    atk_ms6050_iic_delay();
    ATK_MS6050_IIC_SDA(1);
    atk_ms6050_iic_delay();
}

uint8_t atk_ms6050_iic_wait_ack(void)
{
    uint8_t wait_time = 0;

    atk_ms6050_iic_sda_in();
    ATK_MS6050_IIC_SDA(1);
    atk_ms6050_iic_delay();
    ATK_MS6050_IIC_SCL(1);
    atk_ms6050_iic_delay();

    while (ATK_MS6050_IIC_READ_SDA())
    {
        wait_time++;
        if (wait_time > 250)
        {
            ATK_MS6050_IIC_SCL(0);
            atk_ms6050_iic_sda_out();
            atk_ms6050_iic_stop();
            return 1;
        }
    }

    ATK_MS6050_IIC_SCL(0);
    atk_ms6050_iic_delay();
    atk_ms6050_iic_sda_out();
    return 0;
}

void atk_ms6050_iic_ack(void)
{
    atk_ms6050_iic_sda_out();
    ATK_MS6050_IIC_SDA(0);
    atk_ms6050_iic_delay();
    ATK_MS6050_IIC_SCL(1);
    atk_ms6050_iic_delay();
    ATK_MS6050_IIC_SCL(0);
    ATK_MS6050_IIC_SDA(1);
    atk_ms6050_iic_delay();
}

void atk_ms6050_iic_nack(void)
{
    atk_ms6050_iic_sda_out();
    ATK_MS6050_IIC_SDA(1);
    atk_ms6050_iic_delay();
    ATK_MS6050_IIC_SCL(1);
    atk_ms6050_iic_delay();
    ATK_MS6050_IIC_SCL(0);
    atk_ms6050_iic_delay();
}

void atk_ms6050_iic_send_byte(uint8_t dat)
{
    uint8_t i;

    atk_ms6050_iic_sda_out();
    for (i = 0; i < 8; i++)
    {
        ATK_MS6050_IIC_SDA((dat & 0x80u) != 0u);
        atk_ms6050_iic_delay();
        ATK_MS6050_IIC_SCL(1);
        atk_ms6050_iic_delay();
        ATK_MS6050_IIC_SCL(0);
        dat <<= 1;
    }
    ATK_MS6050_IIC_SDA(1);
}

uint8_t atk_ms6050_iic_read_byte(uint8_t ack)
{
    uint8_t i;
    uint8_t dat = 0;

    atk_ms6050_iic_sda_in();
    for (i = 0; i < 8; i++)
    {
        dat <<= 1;
        ATK_MS6050_IIC_SCL(1);
        atk_ms6050_iic_delay();

        if (ATK_MS6050_IIC_READ_SDA())
        {
            dat++;
        }

        ATK_MS6050_IIC_SCL(0);
        atk_ms6050_iic_delay();
    }

    atk_ms6050_iic_sda_out();
    if (ack)
    {
        atk_ms6050_iic_ack();
    }
    else
    {
        atk_ms6050_iic_nack();
    }

    return dat;
}

void atk_ms6050_iic_init(void)
{
    GPIO_InitTypeDef gpio_init_struct;

    RCC_APB2PeriphClockCmd(ATK_MS6050_IIC_SCL_GPIO_CLK | ATK_MS6050_IIC_SDA_GPIO_CLK, ENABLE);

    gpio_init_struct.GPIO_Pin = ATK_MS6050_IIC_SCL_GPIO_PIN | ATK_MS6050_IIC_SDA_GPIO_PIN;
    gpio_init_struct.GPIO_Speed = GPIO_Speed_50MHz;
    gpio_init_struct.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_Init(ATK_MS6050_IIC_SCL_GPIO_PORT, &gpio_init_struct);

    ATK_MS6050_IIC_SCL(1);
    ATK_MS6050_IIC_SDA(1);
    atk_ms6050_iic_bus_recover();
    atk_ms6050_iic_stop();
}
