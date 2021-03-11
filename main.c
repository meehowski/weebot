//*****************************************************************************
//
// freertos_demo.c - Simple FreeRTOS example.
//
//*****************************************************************************

#include "inttypes.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"
#include "utils/uartstdio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "led_task.h"
#include "shell_task.h"
#include "switch_task.h"
#include "stepper.h"
#include "platform.h"
#include "timer.h"

//*****************************************************************************
//
// The mutex that protects concurrent access of UART from multiple tasks.
//
//*****************************************************************************
xSemaphoreHandle g_pUARTSemaphore;

//*****************************************************************************
//
// The error routine that is called if the driver library encounters an error.
//
//*****************************************************************************
#ifdef DEBUG
void
__error__(char *pcFilename, unsigned long ulLine)
{
}

#endif

//*****************************************************************************
//
// This hook is called by FreeRTOS when an stack overflow error is detected.
//
//*****************************************************************************
void
vApplicationStackOverflowHook(xTaskHandle *pxTask, signed char *pcTaskName)
{
    //
    // This function can not return, so loop forever.  Interrupts are disabled
    // on entry to this function, so no processor interrupts will interrupt
    // this loop.
    //
    while(1)
    {
    }
}

//*****************************************************************************
//
// Initialize FreeRTOS and start the initial set of tasks.
//
//*****************************************************************************
int
main(void)
{
    ROM_FPUEnable();
    ROM_FPUStackingEnable();
    ROM_IntMasterDisable();

    //
    // Set the clocking to run at 66.6 MHz from the PLL.
    //
    ROM_SysCtlClockSet(SYSCTL_SYSDIV_3 | SYSCTL_USE_PLL | SYSCTL_XTAL_16MHZ |
                       SYSCTL_OSC_MAIN);

    //
    // Initialize the UART and configure it for 115,200, 8-N-1 operation.
    //
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    ROM_GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    UARTStdioInit(0);

    //
    // Print demo introduction.
    //
    UARTprintf("\033[2JStellaris EK-LM4F120 FreeRTOS (sysclk=%u)\n\n", ROM_SysCtlClockGet());
#if 0
#if 0
    UARTprintf("            \\\\\\\\\\\\////// \n");
    UARTprintf("             \\\\((()))//  \n");
    UARTprintf("  WOE-      /   \\\\//   \\  \n");
    UARTprintf("   -BOT   _|     \\/     |_ \n"); 
    UARTprintf("         ((| \\___  ___/ |)) \n");
    UARTprintf(" KILLS    \\ > -o-\\/-o- < / \n");
    UARTprintf("   ALL     |     ..     | \n");
    UARTprintf("  HUMANS    )   ____   (  \n");
    UARTprintf(" GOOD    _,'\\          /`._\n");
    UARTprintf("      ,-'    `-.____.-'    `. \n");
    UARTprintf("     /         |    |        \\ \n");
    UARTprintf("    |_|_|_|_|__|____|___|_|_|_| \n");
    UARTprintf("\n");
#else
    UARTprintf("       -+-             .___. \n");
    UARTprintf("     .--+--.         _/__ /|\n");
    UARTprintf("     ||[o o]        |____|||\n");
    UARTprintf("     || ___|         |O O ||\n");
    UARTprintf("   __`-----'_      __|++++|/__\n");
    UARTprintf("  |\\ ________\\    /_________ /|\n");
    UARTprintf("  ||   WOE-  ||   |  nukes   ||\n");
    UARTprintf("  |||  BOT   ||   || humans |||\n");
    UARTprintf("  \\||  v1.0  ||   || good   ||/\n");
    UARTprintf("   VV========VV   VV========VV\n");
    UARTprintf("   ||       |      |   |   ||\n");
    UARTprintf("   ||       |      |   |   ||\n");
    UARTprintf("   \\|___|___|      |___|___|/\n");
    UARTprintf("     \\___\\___\\     /___/___/\n");
    UARTprintf("\n");
#endif
#endif

    //
    // Create a mutex to guard the UART.
    //
    g_pUARTSemaphore = xSemaphoreCreateMutex();


#if 0
    //
    // Create the LED task.
    //
    if( LEDTaskInit() != 0)
    {
        
        while(1)
        {
        }
    }
#endif
#if 0
    //
    // Create the STEPPER task.
    //
    if(STEPPERTaskInit() != 0)
    {
        
        while(1)
        {
        }
    }
#endif

#if 0
    //
    // Create the switch task.
    //
    if(SwitchTaskInit() != 0)
    {
        
        while(1)
        {
        }
    }
#endif

    timer_init();
    stepper_init(4);
    platform_init();

    //UARTprintf("Setting speed!\n");
    //stepper_speed(0, 100.0f);

#if 1
    //
    // Create the SHELL task.
    //
    if( shellTaskInit() != 0)
    {
        
        while(1)
        {
        }
    }
#endif

    ROM_IntMasterDisable();

    UARTprintf("Going multitasking.\n");

    //
    // Start the scheduler.  This should not return.
    //
    vTaskStartScheduler();

    //
    // In case the scheduler returns for some reason, print an error and loop
    // forever.
    //

    while(1)
    {
    }
}
