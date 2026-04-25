#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include "stm32f10x.h"

#ifdef __NVIC_PRIO_BITS
    #define configPRIO_BITS                          __NVIC_PRIO_BITS
#else
    #define configPRIO_BITS                          4
#endif

#define configCPU_CLOCK_HZ                           ( ( unsigned long ) 72000000 )
#define configSYSTICK_CLOCK_HZ                       ( configCPU_CLOCK_HZ / 8 )
#define configTICK_RATE_HZ                           1000
#define configMAX_PRIORITIES                         8
#define configMINIMAL_STACK_SIZE                     ( ( unsigned short ) 128 )
#define configTOTAL_HEAP_SIZE                        ( ( size_t ) ( 8 * 1024 ) )
#define configMAX_TASK_NAME_LEN                      16
#define configTICK_TYPE_WIDTH_IN_BITS                TICK_TYPE_WIDTH_32_BITS
#define configUSE_PREEMPTION                         1
#define configUSE_TIME_SLICING                       1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION      1
#define configUSE_TICKLESS_IDLE                      0
#define configIDLE_SHOULD_YIELD                      1
#define configUSE_TASK_NOTIFICATIONS                 1
#define configUSE_MUTEXES                            1
#define configUSE_RECURSIVE_MUTEXES                  1
#define configUSE_COUNTING_SEMAPHORES                1
#define configUSE_QUEUE_SETS                         0
#define configQUEUE_REGISTRY_SIZE                    0
#define configUSE_APPLICATION_TASK_TAG               0
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS      0
#define configUSE_IDLE_HOOK                          0
#define configUSE_TICK_HOOK                          0
#define configUSE_MALLOC_FAILED_HOOK                 0
#define configCHECK_FOR_STACK_OVERFLOW               0
#define configGENERATE_RUN_TIME_STATS                0
#define configUSE_TRACE_FACILITY                     0
#define configUSE_STATS_FORMATTING_FUNCTIONS         0
#define configUSE_CO_ROUTINES                        0
#define configMAX_CO_ROUTINE_PRIORITIES              1
#define configUSE_TIMERS                             0
#define configSUPPORT_DYNAMIC_ALLOCATION             1
#define configSUPPORT_STATIC_ALLOCATION              0
#define configUSE_NEWLIB_REENTRANT                   0
#define configENABLE_BACKWARD_COMPATIBILITY          1
#define configCHECK_HANDLER_INSTALLATION             0

#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY      15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5

#define configKERNEL_INTERRUPT_PRIORITY \
    ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << ( 8 - configPRIO_BITS ) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << ( 8 - configPRIO_BITS ) )
#define configMAX_API_CALL_INTERRUPT_PRIORITY \
    configMAX_SYSCALL_INTERRUPT_PRIORITY

#define vPortSVCHandler                           SVC_Handler
#define xPortPendSVHandler                        PendSV_Handler
#define xPortSysTickHandler                       SysTick_Handler

#define INCLUDE_vTaskPrioritySet                     1
#define INCLUDE_uxTaskPriorityGet                    1
#define INCLUDE_vTaskDelete                          1
#define INCLUDE_vTaskSuspend                         1
#define INCLUDE_vTaskDelay                           1
#define INCLUDE_vTaskDelayUntil                      1
#define INCLUDE_xTaskGetSchedulerState               1
#define INCLUDE_xTaskGetCurrentTaskHandle            1
#define INCLUDE_uxTaskGetStackHighWaterMark          0
#define INCLUDE_xTaskGetIdleTaskHandle               0
#define INCLUDE_eTaskGetState                        0
#define INCLUDE_xTimerPendFunctionCall               0
#define INCLUDE_xTaskAbortDelay                      0
#define INCLUDE_xTaskResumeFromISR                   1

#define configASSERT( x )                            \
    if( ( x ) == 0 )                                \
    {                                               \
        taskDISABLE_INTERRUPTS();                   \
        for( ; ; )                                  \
        {                                           \
        }                                           \
    }

#endif
