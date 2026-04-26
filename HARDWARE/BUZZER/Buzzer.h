#ifndef __BUZZER_H
#define __BUZZER_H

#include "stm32f10x.h"

#define BUZZER_GPIO_PORT       GPIOB
#define BUZZER_GPIO_CLK        RCC_APB2Periph_GPIOB
#define BUZZER_GPIO_PIN        GPIO_Pin_0

void Buzzer_Init(void);
void Buzzer_On(void);
void Buzzer_Off(void);
void Buzzer_Toggle(void);
void Buzzer_BeepMs(uint16_t ms);
void Buzzer_BeepTimes(uint16_t on_ms, uint16_t off_ms, uint8_t times);

#endif
