#include "stm32f10x.h"
#include "delay.h"
#include "usart.h"
#include "includes.h"
#include "led.h"
#include "oled.h"

static void oled_test_task(void *pvParameters)
{
    (void)pvParameters;

    OLED_Init();
    OLED_Clear();

    for (;;)
    {
        OLED_Clear();
        OLED_ShowString(8, 16, (u8 *)"OLED TEST OK", 16, 1);
        OLED_Refresh();

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
    delay_init();
    uart_init(921600);
    led_init();

    printf("Booting OLED test task...\r\n");

    if (xTaskCreate(oled_test_task,
                    "oled_test",
                    256,
                    NULL,
                    tskIDLE_PRIORITY + 1,
                    NULL) != pdPASS)
    {
        printf("Failed to create OLED test task.\r\n");

        while (1)
        {
            led_toggle();
            delay_ms(200);
        }
    }

    vTaskStartScheduler();

    while (1)
    {
        led_toggle();
        delay_ms(500);
    }
}




