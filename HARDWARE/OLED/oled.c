#include "oled.h"
#include "oledfont.h"
#include "delay.h"

#define OLED_I2C_INSTANCE      I2C1
#define OLED_I2C_ADDRESS       0x3Cu
#define OLED_I2C_RCC_APB1      RCC_APB1Periph_I2C1
#define OLED_I2C_RCC_APB2_GPIO RCC_APB2Periph_GPIOB
#define OLED_I2C_GPIO_PORT     GPIOB
#define OLED_I2C_SCL_PIN       GPIO_Pin_6
#define OLED_I2C_SDA_PIN       GPIO_Pin_7
#define OLED_I2C_SPEED         400000u
#define OLED_I2C_TIMEOUT       3000u
#define OLED_DISPLAY_WIDTH     128u
#define OLED_PAGE_COUNT        8u

#define OLED_RES_Clr() GPIO_ResetBits(GPIOA, GPIO_Pin_2)
#define OLED_RES_Set() GPIO_SetBits(GPIOA, GPIO_Pin_2)

u8 OLED_GRAM[144][8];

static uint8_t oled_page_buffer[OLED_DISPLAY_WIDTH];

static uint8_t OLED_I2C_WaitEvent(uint32_t event)
{
    uint32_t timeout;

    timeout = OLED_I2C_TIMEOUT;
    while (I2C_CheckEvent(OLED_I2C_INSTANCE, event) == ERROR)
    {
        if (timeout-- == 0u)
        {
            I2C_GenerateSTOP(OLED_I2C_INSTANCE, ENABLE);
            return 1u;
        }
    }

    return 0u;
}

static uint8_t OLED_I2C_Write(uint8_t control, const uint8_t *data, uint16_t len)
{
    uint16_t i;

    if ((data == 0) || (len == 0u))
    {
        return 1u;
    }

    I2C_GenerateSTART(OLED_I2C_INSTANCE, ENABLE);
    if (OLED_I2C_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT)) return 1u;

    I2C_Send7bitAddress(OLED_I2C_INSTANCE, (uint8_t)(OLED_I2C_ADDRESS << 1), I2C_Direction_Transmitter);
    if (OLED_I2C_WaitEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)) return 1u;

    I2C_SendData(OLED_I2C_INSTANCE, control);
    if (OLED_I2C_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) return 1u;

    for (i = 0u; i < len; i++)
    {
        I2C_SendData(OLED_I2C_INSTANCE, data[i]);
        if (OLED_I2C_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) return 1u;
    }

    I2C_GenerateSTOP(OLED_I2C_INSTANCE, ENABLE);
    return 0u;
}

void OLED_ColorTurn(u8 i)
{
    if (i == 0u)
    {
        OLED_WR_Byte(0xA6u, OLED_CMD);
    }
    if (i == 1u)
    {
        OLED_WR_Byte(0xA7u, OLED_CMD);
    }
}

void OLED_DisplayTurn(u8 i)
{
    if (i == 0u)
    {
        OLED_WR_Byte(0xC8u, OLED_CMD);
        OLED_WR_Byte(0xA1u, OLED_CMD);
    }
    if (i == 1u)
    {
        OLED_WR_Byte(0xC0u, OLED_CMD);
        OLED_WR_Byte(0xA0u, OLED_CMD);
    }
}

void OLED_WR_Byte(u8 dat, u8 mode)
{
    uint8_t control;

    control = (mode == OLED_DATA) ? 0x40u : 0x00u;
    (void)OLED_I2C_Write(control, &dat, 1u);
}

void OLED_DisPlay_On(void)
{
    OLED_WR_Byte(0x8Du, OLED_CMD);
    OLED_WR_Byte(0x14u, OLED_CMD);
    OLED_WR_Byte(0xAFu, OLED_CMD);
}

void OLED_DisPlay_Off(void)
{
    OLED_WR_Byte(0x8Du, OLED_CMD);
    OLED_WR_Byte(0x10u, OLED_CMD);
    OLED_WR_Byte(0xAEu, OLED_CMD);
}

void OLED_Refresh(void)
{
    u8 i;
    u8 n;
    uint8_t command[3];

    for (i = 0u; i < OLED_PAGE_COUNT; i++)
    {
        command[0] = (uint8_t)(0xB0u + i);
        command[1] = 0x02u;
        command[2] = 0x10u;
        (void)OLED_I2C_Write(0x00u, command, 3u);

        for (n = 0u; n < OLED_DISPLAY_WIDTH; n++)
        {
            oled_page_buffer[n] = OLED_GRAM[n][i];
        }

        (void)OLED_I2C_Write(0x40u, oled_page_buffer, OLED_DISPLAY_WIDTH);
    }
}

void OLED_ClearBuffer(void)
{
    u8 i;
    u8 n;

    for (i = 0u; i < OLED_PAGE_COUNT; i++)
    {
        for (n = 0u; n < OLED_DISPLAY_WIDTH; n++)
        {
            OLED_GRAM[n][i] = 0u;
        }
    }
}

void OLED_Clear(void)
{
    OLED_ClearBuffer();
    OLED_Refresh();
}

void OLED_ClearPoint(u8 x, u8 y)
{
    OLED_DrawPoint(x, y, 0u);
}

void OLED_DrawPoint(u8 x, u8 y, u8 t)
{
    u8 i;
    u8 m;
    u8 n;

    i = y / 8u;
    m = y % 8u;
    n = (u8)(1u << m);
    if (t)
    {
        OLED_GRAM[x][i] |= n;
    }
    else
    {
        OLED_GRAM[x][i] = (u8)(~OLED_GRAM[x][i]);
        OLED_GRAM[x][i] |= n;
        OLED_GRAM[x][i] = (u8)(~OLED_GRAM[x][i]);
    }
}

void OLED_DrawLine(u8 x1, u8 y1, u8 x2, u8 y2, u8 mode)
{
    u16 t;
    int xerr;
    int yerr;
    int delta_x;
    int delta_y;
    int distance;
    int incx;
    int incy;
    int uRow;
    int uCol;

    xerr = 0;
    yerr = 0;
    delta_x = x2 - x1;
    delta_y = y2 - y1;
    uRow = x1;
    uCol = y1;
    if (delta_x > 0)
    {
        incx = 1;
    }
    else if (delta_x == 0)
    {
        incx = 0;
    }
    else
    {
        incx = -1;
        delta_x = -delta_x;
    }
    if (delta_y > 0)
    {
        incy = 1;
    }
    else if (delta_y == 0)
    {
        incy = 0;
    }
    else
    {
        incy = -1;
        delta_y = -delta_x;
    }
    if (delta_x > delta_y)
    {
        distance = delta_x;
    }
    else
    {
        distance = delta_y;
    }
    for (t = 0u; t < (u16)(distance + 1); t++)
    {
        OLED_DrawPoint((u8)uRow, (u8)uCol, mode);
        xerr += delta_x;
        yerr += delta_y;
        if (xerr > distance)
        {
            xerr -= distance;
            uRow += incx;
        }
        if (yerr > distance)
        {
            yerr -= distance;
            uCol += incy;
        }
    }
}

void OLED_DrawCircle(u8 x, u8 y, u8 r)
{
    int a;
    int b;
    int num;

    a = 0;
    b = r;
    while (2 * b * b >= r * r)
    {
        OLED_DrawPoint((u8)(x + a), (u8)(y - b), 1u);
        OLED_DrawPoint((u8)(x - a), (u8)(y - b), 1u);
        OLED_DrawPoint((u8)(x - a), (u8)(y + b), 1u);
        OLED_DrawPoint((u8)(x + a), (u8)(y + b), 1u);

        OLED_DrawPoint((u8)(x + b), (u8)(y + a), 1u);
        OLED_DrawPoint((u8)(x + b), (u8)(y - a), 1u);
        OLED_DrawPoint((u8)(x - b), (u8)(y - a), 1u);
        OLED_DrawPoint((u8)(x - b), (u8)(y + a), 1u);

        a++;
        num = (a * a + b * b) - r * r;
        if (num > 0)
        {
            b--;
            a--;
        }
    }
}

void OLED_ShowChar(u8 x, u8 y, u8 chr, u8 size1, u8 mode)
{
    u8 i;
    u8 m;
    u8 temp;
    u8 size2;
    u8 chr1;
    u8 x0;
    u8 y0;

    x0 = x;
    y0 = y;
    if (size1 == 8u)
    {
        size2 = 6u;
    }
    else
    {
        size2 = (u8)((size1 / 8u + ((size1 % 8u) ? 1u : 0u)) * (size1 / 2u));
    }
    chr1 = (u8)(chr - ' ');
    for (i = 0u; i < size2; i++)
    {
        if (size1 == 8u)
        {
            temp = asc2_0806[chr1][i];
        }
        else if (size1 == 12u)
        {
            temp = asc2_1206[chr1][i];
        }
        else if (size1 == 16u)
        {
            temp = asc2_1608[chr1][i];
        }
        else if (size1 == 24u)
        {
            temp = asc2_2412[chr1][i];
        }
        else
        {
            return;
        }
        for (m = 0u; m < 8u; m++)
        {
            if (temp & 0x01u)
            {
                OLED_DrawPoint(x, y, mode);
            }
            else
            {
                OLED_DrawPoint(x, y, (u8)!mode);
            }
            temp >>= 1;
            y++;
        }
        x++;
        if ((size1 != 8u) && ((x - x0) == size1 / 2u))
        {
            x = x0;
            y0 = (u8)(y0 + 8u);
        }
        y = y0;
    }
}

void OLED_ShowChar6x8(u8 x, u8 y, u8 chr, u8 mode)
{
    OLED_ShowChar(x, y, chr, 8u, mode);
}

void OLED_ShowString(u8 x, u8 y, u8 *chr, u8 size1, u8 mode)
{
    while ((*chr >= ' ') && (*chr <= '~'))
    {
        OLED_ShowChar(x, y, *chr, size1, mode);
        if (size1 == 8u)
        {
            x = (u8)(x + 6u);
        }
        else
        {
            x = (u8)(x + size1 / 2u);
        }
        chr++;
    }
}

u32 OLED_Pow(u8 m, u8 n)
{
    u32 result;

    result = 1u;
    while (n--)
    {
        result *= m;
    }
    return result;
}

void OLED_ShowNum(u8 x, u8 y, u32 num, u8 len, u8 size1, u8 mode)
{
    u8 t;
    u8 temp;
    u8 m;

    m = 0u;
    if (size1 == 8u)
    {
        m = 2u;
    }
    for (t = 0u; t < len; t++)
    {
        temp = (u8)((num / OLED_Pow(10u, (u8)(len - t - 1u))) % 10u);
        if (temp == 0u)
        {
            OLED_ShowChar((u8)(x + (size1 / 2u + m) * t), y, '0', size1, mode);
        }
        else
        {
            OLED_ShowChar((u8)(x + (size1 / 2u + m) * t), y, (u8)(temp + '0'), size1, mode);
        }
    }
}

void OLED_ShowChinese(u8 x, u8 y, u8 num, u8 size1, u8 mode)
{
    u8 m;
    u8 temp;
    u8 x0;
    u8 y0;
    u16 i;
    u16 size3;

    x0 = x;
    y0 = y;
    size3 = (u16)((size1 / 8u + ((size1 % 8u) ? 1u : 0u)) * size1);
    for (i = 0u; i < size3; i++)
    {
        if (size1 == 8u)
        {
            temp = Chinese8x8[num][i];
        }
        else if (size1 == 12u)
        {
            temp = Chinese12x12[num][i];
        }
        else if (size1 == 16u)
        {
            temp = Chinese16x16[num][i];
        }
        else if (size1 == 24u)
        {
            temp = Chinese24x24[num][i];
        }
        else if (size1 == 32u)
        {
            temp = Chinese32x32[num][i];
        }
        else if (size1 == 64u)
        {
            temp = Chinese64x64[num][i];
        }
        else
        {
            return;
        }
        for (m = 0u; m < 8u; m++)
        {
            if (temp & 0x01u)
            {
                OLED_DrawPoint(x, y, mode);
            }
            else
            {
                OLED_DrawPoint(x, y, (u8)!mode);
            }
            temp >>= 1;
            y++;
        }
        x++;
        if ((x - x0) == size1)
        {
            x = x0;
            y0 = (u8)(y0 + 8u);
        }
        y = y0;
    }
}

void OLED_ScrollDisplay(u8 num, u8 space, u8 mode)
{
    u8 i;
    u8 n;
    u8 t;
    u8 m;
    u8 r;

    t = 0u;
    m = 0u;
    while (1)
    {
        if (m == 0u)
        {
            OLED_ShowChinese(128u, 24u, t, 16u, mode);
            t++;
        }
        if (t == num)
        {
            for (r = 0u; r < (u8)(16u * space); r++)
            {
                for (i = 1u; i < 144u; i++)
                {
                    for (n = 0u; n < 8u; n++)
                    {
                        OLED_GRAM[i - 1u][n] = OLED_GRAM[i][n];
                    }
                }
                OLED_Refresh();
            }
            t = 0u;
        }
        m++;
        if (m == 16u)
        {
            m = 0u;
        }
        for (i = 1u; i < 144u; i++)
        {
            for (n = 0u; n < 8u; n++)
            {
                OLED_GRAM[i - 1u][n] = OLED_GRAM[i][n];
            }
        }
        OLED_Refresh();
    }
}

void OLED_ShowPicture(u8 x, u8 y, u8 sizex, u8 sizey, u8 BMP[], u8 mode)
{
    u16 j;
    u8 i;
    u8 n;
    u8 temp;
    u8 m;
    u8 x0;
    u8 y0;

    j = 0u;
    x0 = x;
    y0 = y;
    sizey = (u8)(sizey / 8u + ((sizey % 8u) ? 1u : 0u));
    for (n = 0u; n < sizey; n++)
    {
        for (i = 0u; i < sizex; i++)
        {
            temp = BMP[j];
            j++;
            for (m = 0u; m < 8u; m++)
            {
                if (temp & 0x01u)
                {
                    OLED_DrawPoint(x, y, mode);
                }
                else
                {
                    OLED_DrawPoint(x, y, (u8)!mode);
                }
                temp >>= 1;
                y++;
            }
            x++;
            if ((x - x0) == sizex)
            {
                x = x0;
                y0 = (u8)(y0 + 8u);
            }
            y = y0;
        }
    }
}

void OLED_Init(void)
{
    GPIO_InitTypeDef gpio_init_structure;
    I2C_InitTypeDef i2c_init_structure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | OLED_I2C_RCC_APB2_GPIO, ENABLE);
    RCC_APB1PeriphClockCmd(OLED_I2C_RCC_APB1, ENABLE);

    gpio_init_structure.GPIO_Pin = OLED_I2C_SCL_PIN | OLED_I2C_SDA_PIN;
    gpio_init_structure.GPIO_Mode = GPIO_Mode_AF_OD;
    gpio_init_structure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(OLED_I2C_GPIO_PORT, &gpio_init_structure);

    gpio_init_structure.GPIO_Pin = GPIO_Pin_2;
    gpio_init_structure.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init_structure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio_init_structure);
    GPIO_SetBits(GPIOA, GPIO_Pin_2);

    I2C_DeInit(OLED_I2C_INSTANCE);
    i2c_init_structure.I2C_ClockSpeed = OLED_I2C_SPEED;
    i2c_init_structure.I2C_Mode = I2C_Mode_I2C;
    i2c_init_structure.I2C_DutyCycle = I2C_DutyCycle_2;
    i2c_init_structure.I2C_OwnAddress1 = 0x00;
    i2c_init_structure.I2C_Ack = I2C_Ack_Enable;
    i2c_init_structure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_Init(OLED_I2C_INSTANCE, &i2c_init_structure);
    I2C_Cmd(OLED_I2C_INSTANCE, ENABLE);

    OLED_RES_Clr();
    delay_ms(20);
    OLED_RES_Set();
    delay_ms(20);

    OLED_WR_Byte(0xAEu, OLED_CMD);
    OLED_WR_Byte(0x02u, OLED_CMD);
    OLED_WR_Byte(0x10u, OLED_CMD);
    OLED_WR_Byte(0x40u, OLED_CMD);
    OLED_WR_Byte(0xB0u, OLED_CMD);
    OLED_WR_Byte(0x81u, OLED_CMD);
    OLED_WR_Byte(0xCFu, OLED_CMD);
    OLED_WR_Byte(0xA1u, OLED_CMD);
    OLED_WR_Byte(0xA6u, OLED_CMD);
    OLED_WR_Byte(0xA8u, OLED_CMD);
    OLED_WR_Byte(0x3Fu, OLED_CMD);
    OLED_WR_Byte(0xADu, OLED_CMD);
    OLED_WR_Byte(0x8Bu, OLED_CMD);
    OLED_WR_Byte(0x33u, OLED_CMD);
    OLED_WR_Byte(0xC8u, OLED_CMD);
    OLED_WR_Byte(0xD3u, OLED_CMD);
    OLED_WR_Byte(0x00u, OLED_CMD);
    OLED_WR_Byte(0xD5u, OLED_CMD);
    OLED_WR_Byte(0x80u, OLED_CMD);
    OLED_WR_Byte(0xD9u, OLED_CMD);
    OLED_WR_Byte(0x1Fu, OLED_CMD);
    OLED_WR_Byte(0xDAu, OLED_CMD);
    OLED_WR_Byte(0x12u, OLED_CMD);
    OLED_WR_Byte(0xDBu, OLED_CMD);
    OLED_WR_Byte(0x40u, OLED_CMD);
    OLED_Clear();
    OLED_WR_Byte(0xAFu, OLED_CMD);
}
