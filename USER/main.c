#include <stdio.h>
#include "stm32f10x.h"
#include "delay.h"
#include "usart.h"
#include "includes.h"
#include "led.h"
#include "app_env_monitor.h"

static void board_init(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
    delay_init();
    uart_init(921600);
    led_init();
}

static void fatal_blink_forever(u16 interval_ms)
{
    while (1)
    {
        led_toggle();
        delay_ms(interval_ms);
    }
}

int main(void)
{
    board_init();

    printf("Booting env monitor task...\r\n");

    if (app_env_monitor_start(tskIDLE_PRIORITY + 1) != pdPASS)
    {
        printf("Failed to create env monitor task.\r\n");
        fatal_blink_forever(200);
    }

    vTaskStartScheduler();
    fatal_blink_forever(500);
}
