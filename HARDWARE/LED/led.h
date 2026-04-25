#ifndef __LED_H
#define __LED_H

#include "stm32f10x.h"

/*
 * Board-level LED configuration.
 * You can override these macros before including this header if needed.
 */
#define LED_LEVEL_LOW        0
#define LED_LEVEL_HIGH       1

#ifndef LED_GPIO_PORT
#define LED_GPIO_PORT        GPIOC
#endif

#ifndef LED_GPIO_CLK
#define LED_GPIO_CLK         RCC_APB2Periph_GPIOC
#endif

#ifndef LED_GPIO_PIN
#define LED_GPIO_PIN         GPIO_Pin_13
#endif

#ifndef LED_ACTIVE_LEVEL
#define LED_ACTIVE_LEVEL     LED_LEVEL_LOW
#endif

void led_init(void);
void led_on(void);
void led_off(void);
void led_toggle(void);
uint8_t led_is_on(void);

#endif
