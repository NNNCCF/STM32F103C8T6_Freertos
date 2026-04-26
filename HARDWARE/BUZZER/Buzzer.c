#include "Buzzer.h"
#include "Delay.h"

void Buzzer_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(BUZZER_GPIO_CLK, ENABLE);

    gpio.GPIO_Pin = BUZZER_GPIO_PIN;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(BUZZER_GPIO_PORT, &gpio);

    Buzzer_Off();
}

void Buzzer_On(void)
{
    GPIO_SetBits(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN);
}

void Buzzer_Off(void)
{
    GPIO_ResetBits(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN);
}

void Buzzer_Toggle(void)
{
    if (GPIO_ReadOutputDataBit(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN) == Bit_SET)
    {
        Buzzer_Off();
    }
    else
    {
        Buzzer_On();
    }
}

