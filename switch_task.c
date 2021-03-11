//*****************************************************************************
//
// switch_task.c - A simple switch task to process the buttons.
//
//*****************************************************************************

#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_gpio.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/rom.h"
#include "drivers/buttons.h"
#include "utils/uartstdio.h"
#include "switch_task.h"
#include "led_task.h"
#include "priorities.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

//*****************************************************************************
//
// The stack size for the display task.
//
//*****************************************************************************
#define SWITCHTASKSTACKSIZE        128         // Stack size in words

extern xQueueHandle g_pLEDQueue;
extern xQueueHandle g_pSTEPPERQueue;
extern xSemaphoreHandle g_pUARTSemaphore;

//*****************************************************************************
//
// This task reads the buttons' state and passes this information to LEDTask.
//
//*****************************************************************************
static void
SwitchTask(void *pvParameters)
{
    portTickType ulLastTime;
    unsigned long ulSwitchDelay = 25;
    unsigned char ucCurButtonState, ucPrevButtonState;
    unsigned char ucMessage;

    ucCurButtonState = ucPrevButtonState = 0;

    //
    // Get the current tick count.
    //
    ulLastTime = xTaskGetTickCount();

    //
    // Loop forever.
    //
    while(1)
    {
        //
        // Poll the debounced state of the buttons.
        //
        ucCurButtonState = ButtonsPoll(0, 0);

        //
        // Check if previous debounced state is equal to the current state.
        //
        if(ucCurButtonState != ucPrevButtonState)
        {
            ucPrevButtonState = ucCurButtonState;

            //
            // Check to make sure the change in state is due to button press
            // and not due to button release.
            //
            if((ucCurButtonState & ALL_BUTTONS) != 0)
            {
                if((ucCurButtonState & ALL_BUTTONS) == LEFT_BUTTON)
                {
                    ucMessage = LEFT_BUTTON;

                    //
                    // Guard UART from concurrent access.
                    //
                    xSemaphoreTake(g_pUARTSemaphore, portMAX_DELAY);
                    UARTprintf("Left Button is pressed.\n");
                    xSemaphoreGive(g_pUARTSemaphore);
                }
                else if((ucCurButtonState & ALL_BUTTONS) == RIGHT_BUTTON)
                {
                    ucMessage = RIGHT_BUTTON;

                    //
                    // Guard UART from concurrent access.
                    //
                    xSemaphoreTake(g_pUARTSemaphore, portMAX_DELAY);
                    UARTprintf("Right Button is pressed.\n");
                    xSemaphoreGive(g_pUARTSemaphore);
                }

#if 0
                //
                // Pass the value of the button pressed to LEDTask.
                //
                if(xQueueSend(g_pLEDQueue, &ucMessage, portMAX_DELAY) !=
                   pdPASS)
                {
                    //
                    // Error. The queue should never be full. If so print the
                    // error message on UART and wait for ever.
                    //
                    UARTprintf("\nQueue full. This should never happen.\n");
                    while(1)
                    {
                    }
                }
#endif
#if 0
                //
                // Pass the value of the button pressed to STEPPERTask.
                //
                if(xQueueSend(g_pSTEPPERQueue, &ucMessage, portMAX_DELAY) !=
                   pdPASS)
                {
                    //
                    // Error. The queue should never be full. If so print the
                    // error message on UART and wait for ever.
                    //
                    UARTprintf("\nQueue full. This should never happen.\n");
                    while(1)
                    {
                    }
                }
#endif
            }
        }

        //
        // Wait for the required amount of time to check back.
        //
        vTaskDelayUntil(&ulLastTime, ulSwitchDelay / portTICK_RATE_MS);
    }
}

//*****************************************************************************
//
// Initializes the switch task.
//
//*****************************************************************************
unsigned long
SwitchTaskInit(void)
{
    //
    // Unlock the GPIO LOCK register for Right button to work.
    //
    HWREG(GPIO_PORTF_BASE + GPIO_O_LOCK) = GPIO_LOCK_KEY_DD;
    HWREG(GPIO_PORTF_BASE + GPIO_O_CR) = 0xFF;

    //
    // Initialize the buttons
    //
    ButtonsInit();

    //
    // Create the switch task.
    //
    if(xTaskCreate(SwitchTask, (signed portCHAR *)"Switch",
                   SWITCHTASKSTACKSIZE, NULL, tskIDLE_PRIORITY +
                   PRIORITY_SWITCH_TASK, NULL) != pdTRUE)
    {
        return(1);
    }

    UARTprintf("Switch task initialized\n");

    //
    // Success.
    //
    return(0);
}
