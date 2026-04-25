#include "led.h"

static void led_write(uint8_t on)
{
#if LED_ACTIVE_LEVEL
	if (on)
	{
		GPIO_SetBits(LED_GPIO_PORT, LED_GPIO_PIN);
	}
	else
	{
		GPIO_ResetBits(LED_GPIO_PORT, LED_GPIO_PIN);
	}
#else
	if (on)
	{
		GPIO_ResetBits(LED_GPIO_PORT, LED_GPIO_PIN);
	}
	else
	{
		GPIO_SetBits(LED_GPIO_PORT, LED_GPIO_PIN);
	}
#endif
}

void led_init(void)
{
	GPIO_InitTypeDef gpio_init_structure;

	RCC_APB2PeriphClockCmd(LED_GPIO_CLK, ENABLE);

	gpio_init_structure.GPIO_Pin = LED_GPIO_PIN;
	gpio_init_structure.GPIO_Speed = GPIO_Speed_2MHz;
	gpio_init_structure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(LED_GPIO_PORT, &gpio_init_structure);

	led_off();
}

void led_on(void)
{
	led_write(1);
}

void led_off(void)
{
	led_write(0);
}

uint8_t led_is_on(void)
{
	uint8_t pin_state;

	pin_state = GPIO_ReadOutputDataBit(LED_GPIO_PORT, LED_GPIO_PIN);

#if LED_ACTIVE_LEVEL
	return (pin_state == Bit_SET) ? 1U : 0U;
#else
	return (pin_state == Bit_RESET) ? 1U : 0U;
#endif
}

void led_toggle(void)
{
	led_write((uint8_t)!led_is_on());
}
