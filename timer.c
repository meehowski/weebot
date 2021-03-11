//*****************************************************************************
//
// timer.c
//
//*****************************************************************************

#include "inttypes.h"
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/debug.h"
#include "driverlib/fpu.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/sysctl.h"
#include "driverlib/timer.h"
#include "utils/uartstdio.h"
#include "stepper.h"

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
// Interrupt handlers
//
//*****************************************************************************
void
Timer0AIntHandler(void)
{
    ROM_TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    stepper_tick(0);
}

void
Timer0BIntHandler(void)
{
    ROM_TimerIntClear(TIMER0_BASE, TIMER_TIMB_TIMEOUT);
    stepper_tick(1);
}

void
Timer1AIntHandler(void)
{
    ROM_TimerIntClear(TIMER1_BASE, TIMER_TIMA_TIMEOUT);
    stepper_tick(2);
}

void
Timer1BIntHandler(void)
{
    ROM_TimerIntClear(TIMER1_BASE, TIMER_TIMB_TIMEOUT);
    stepper_tick(3);
}


//*****************************************************************************
//
// This example application demonstrates the use of the timers to generate
// periodic interrupts.
//
//*****************************************************************************
uint8_t
timer_init(void)
{
    //
    // Enable the peripherals used by this example.
    //
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER1);

    //
    // Configure the two 32-bit periodic timers.
    //
    //ROM_TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
    //ROM_TimerLoadSet(TIMER0_BASE, TIMER_A, (ROM_SysCtlClockGet() / 400) - 1);
    //ROM_TimerLoadSet(TIMER0_BASE, TIMER_B, (ROM_SysCtlClockGet() / 4000) - 1);

    ROM_TimerConfigure(TIMER0_BASE, (TIMER_CFG_16_BIT_PAIR | TIMER_CFG_A_PERIODIC | TIMER_CFG_B_PERIODIC));
    ROM_TimerConfigure(TIMER1_BASE, (TIMER_CFG_16_BIT_PAIR | TIMER_CFG_A_PERIODIC | TIMER_CFG_B_PERIODIC));

    //
    // Setup the interrupts for the timer timeouts.
    //
    ROM_IntEnable(INT_TIMER0A);
    ROM_IntEnable(INT_TIMER0B);
    ROM_TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT | TIMER_TIMB_TIMEOUT);

    ROM_IntEnable(INT_TIMER1A);
    ROM_IntEnable(INT_TIMER1B);
    ROM_TimerIntEnable(TIMER1_BASE, TIMER_TIMA_TIMEOUT | TIMER_TIMB_TIMEOUT);

    //
    // Enable the timers.
    //
    ROM_TimerEnable(TIMER0_BASE, TIMER_BOTH);
    ROM_TimerEnable(TIMER1_BASE, TIMER_BOTH);

    UARTprintf("Timers initialized\n");

    return 1;
}
