//*****************************************************************************
//
// stepper_task.c - Stepper motor task.
//
//*****************************************************************************

#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/gpio.h"
#include "driverlib/sysctl.h"
#include "driverlib/rom.h"
#include "drivers/rgb.h"
#include "drivers/buttons.h"
#include "utils/uartstdio.h"
#include "led_task.h"
#include "priorities.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#define S1   GPIO_PIN_4
#define S2   GPIO_PIN_5
#define S3   GPIO_PIN_6
#define S4   GPIO_PIN_7

//*****************************************************************************
//
// The stack size for the task.
//
//*****************************************************************************
#define STEPPERTASKSTACKSIZE        128         // Stack size in words

//*****************************************************************************
//
// The item size and queue size for the STEPPER message queue.
//
//*****************************************************************************
#define STEPPER_ITEM_SIZE           sizeof(unsigned char)
#define STEPPER_QUEUE_SIZE          5

//*****************************************************************************
//
// Default STEPPER toggle delay value. STEPPER toggling frequency is twice this number.
//
//*****************************************************************************
#define STEPPER_TOGGLE_DELAY        1

//*****************************************************************************
//
// The queue that holds messages sent to the STEPPER task.
//
//*****************************************************************************
xQueueHandle g_pSTEPPERQueue;

//
// Speed register.
//
static int g_stepperDelay[3] = { 1, 0, 0 };
static int g_stepperPhase[3] = { 1, 1, 1 };

static const int phaseMax = 8;
static const int phase[] = {
    S1, 
    S1|S2, 
    S2, 
    S2|S3, 
    S3, 
    S3|S4, 
    S4,
    S4|S1,
};

extern xSemaphoreHandle g_pUARTSemaphore;

//*****************************************************************************
//
// This task toggles the user selected STEPPER at a user selected frequency. User
// can make the selections by pressing the left and right buttons.
//
//*****************************************************************************
static void
STEPPERTask(void *pvParameters)
{
    portTickType ulWakeTime;
    unsigned char cMessage;

    //
    // Get the current tick count.
    //
    ulWakeTime = xTaskGetTickCount();

    //
    // Loop forever.
    //
    while(1)
    {
        //
        // Read the next message, if available on queue.
        //
        if(xQueueReceive(g_pSTEPPERQueue, &cMessage, 0) == pdPASS)
        {
            //
            // If left button, update to next STEPPER.
            //
            if(cMessage == LEFT_BUTTON)
            {
                //
                // Update the STEPPER buffer to turn off the currently working.
                //
                g_stepperDelay[0] = -g_stepperDelay[0];

                //
                // Guard UART from concurrent access. Print the currently
                // blinking STEPPER.
                //
                xSemaphoreTake(g_pUARTSemaphore, portMAX_DELAY);
                UARTprintf("Stepper %d is at %d.\n", 0, g_stepperDelay[0]);
                xSemaphoreGive(g_pUARTSemaphore);
            }

            //
            // If right button, update delay time between toggles of led.
            //
            if(cMessage == RIGHT_BUTTON)
            {
                g_stepperDelay[0] *= 2;
                if(g_stepperDelay[0] > 256)
                {
                    g_stepperDelay[0] = STEPPER_TOGGLE_DELAY;
                }

                //
                // Guard UART from concurrent access. Print the currently
                // blinking frequency.
                //
                xSemaphoreTake(g_pUARTSemaphore, portMAX_DELAY);
                UARTprintf("Stepper speed is %d.\n",
                           (g_stepperDelay[0]));
                xSemaphoreGive(g_pUARTSemaphore);
            }
        }

        //
        // Move the STEPPERs.
        //
        GPIOPinWrite(GPIO_PORTC_BASE, S1|S2|S3|S4, phase[g_stepperPhase[0]]);
        //SysCtlDelay(20000);

        if (g_stepperDelay[0] > 0)
        {
            g_stepperPhase[0]++;

            if (g_stepperPhase[0] == phaseMax)
            {
                g_stepperPhase[0] = 0;
            }
        }
        else if (g_stepperDelay[0] < 0)
        {
            if (g_stepperPhase[0] == 0)
            {
                g_stepperPhase[0] = phaseMax;
            }

            g_stepperPhase[0]--;
        }

#if 0
        {
            static unsigned int d = 0;
            if (d++ == 200)
            {
                d = 0;
                UARTprintf(".");
            }
        }
#endif

        //
        // Wait for the required amount of time.
        //
        vTaskDelayUntil(&ulWakeTime, abs(g_stepperDelay[0]) / portTICK_RATE_MS);
    }
}

//*****************************************************************************
//
// Initializes the STEPPER task.
//
//*****************************************************************************
unsigned long
STEPPERTaskInit(void)
{
    //
    // Initialize the GPIOs and Timers that drive the three STEPPERs.
    //
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOC);
    GPIOPinTypeGPIOOutput(GPIO_PORTC_BASE, S1|S2|S3|S4);

   //
    // Create a queue for sending messages to the STEPPER task.
    //
    g_pSTEPPERQueue = xQueueCreate(STEPPER_QUEUE_SIZE, STEPPER_ITEM_SIZE);

    //
    // Create the STEPPER task.
    //
    if(xTaskCreate(STEPPERTask, (signed portCHAR *)"STEPPER", STEPPERTASKSTACKSIZE, NULL,
                   tskIDLE_PRIORITY + PRIORITY_STEPPER_TASK, NULL) != pdTRUE)
    {
        return(1);
    }

    //
    // Print the current loggling STEPPER and frequency.
    //
    UARTprintf("Stepper task initialized\n");

     //
    // Success.
    //
    return(0);
}

