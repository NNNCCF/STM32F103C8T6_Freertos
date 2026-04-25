#include "delay.h"

#if SYSTEM_SUPPORT_OS
#include "includes.h"
#endif

static u8 fac_us = 0;
static u16 fac_ms = 0;

#if SYSTEM_SUPPORT_OS
static BaseType_t delay_os_is_running(void)
{
	return (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED);
}

static BaseType_t delay_os_is_in_irq(void)
{
	return xPortIsInsideInterrupt();
}
#endif

void delay_init(void)
{
	SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK_Div8);
	fac_us = SystemCoreClock / 8000000;
	fac_ms = (u16)(1000 / configTICK_RATE_HZ);

	if (fac_ms == 0)
	{
		fac_ms = 1;
	}
}

static void delay_us_bare(u32 nus)
{
	u32 temp;

	SysTick->LOAD = nus * fac_us;
	SysTick->VAL = 0x00;
	SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;

	do
	{
		temp = SysTick->CTRL;
	} while ((temp & 0x01) && !(temp & (1 << 16)));

	SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
	SysTick->VAL = 0x00;
}

#if SYSTEM_SUPPORT_OS
static void delay_us_rtos(u32 nus)
{
	u32 ticks;
	u32 told;
	u32 tnow;
	u32 tcnt;
	u32 reload;
	BaseType_t scheduler_locked = pdFALSE;

	reload = SysTick->LOAD;
	ticks = nus * fac_us;
	tcnt = 0;

	if (delay_os_is_running() && (delay_os_is_in_irq() == pdFALSE))
	{
		vTaskSuspendAll();
		scheduler_locked = pdTRUE;
	}

	told = SysTick->VAL;
	while (1)
	{
		tnow = SysTick->VAL;
		if (tnow != told)
		{
			if (tnow < told)
			{
				tcnt += told - tnow;
			}
			else
			{
				tcnt += reload - tnow + told;
			}

			told = tnow;
			if (tcnt >= ticks)
			{
				break;
			}
		}
	}

	if (scheduler_locked == pdTRUE)
	{
		xTaskResumeAll();
	}
}
#endif

void delay_us(u32 nus)
{
#if SYSTEM_SUPPORT_OS
	if (delay_os_is_running() == pdTRUE)
	{
		delay_us_rtos(nus);
		return;
	}
#endif

	delay_us_bare(nus);
}

void delay_ms(u16 nms)
{
#if SYSTEM_SUPPORT_OS
	if ((delay_os_is_running() == pdTRUE) && (delay_os_is_in_irq() == pdFALSE))
	{
		if (nms >= fac_ms)
		{
			vTaskDelay(nms / fac_ms);
		}

		nms %= fac_ms;
	}
#endif

	if (nms != 0)
	{
		delay_us((u32)nms * 1000);
	}
}
