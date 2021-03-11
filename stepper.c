//*****************************************************************************
//
// stepper.c
//
//*****************************************************************************

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <inttypes.h>

#include "FreeRTOS.h"
#include "semphr.h"

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
#include "isqrt.h"

typedef struct {
    uint32_t io_peripheral;
    uint32_t io_port;
    uint32_t base_pin;
    uint32_t timer_base;
    uint32_t timer;
    uint32_t interrupt;
} stepper_info_t;

#define STEPPER_MAX 4
#define FLOAT_ERROR 0.00001 // float math leftover error

#define UARTprintf_float(fv) { double i, f; f=modf((fv), &i); UARTprintf("%c%i.%04i", ((fv)<0?'-':'+'), (int32_t)abs(i), (int32_t)(abs(f*10000.0))); }

static const uint8_t g_pin_mask = 0xf;

static const stepper_info_t g_io[STEPPER_MAX] = {
    {SYSCTL_PERIPH_GPIOC, GPIO_PORTC_BASE, 4, TIMER0_BASE, TIMER_A, INT_TIMER0A},
    {SYSCTL_PERIPH_GPIOD, GPIO_PORTD_BASE, 0, TIMER0_BASE, TIMER_B, INT_TIMER0B},
    {SYSCTL_PERIPH_GPIOA, GPIO_PORTA_BASE, 4, TIMER1_BASE, TIMER_A, INT_TIMER1A},
    {SYSCTL_PERIPH_GPIOA, GPIO_PORTA_BASE, 0, TIMER1_BASE, TIMER_B, INT_TIMER1B}
};

typedef struct {
    uint8_t  phase;       // step phase
    float    velocity;    // current velocity, STEPS-PER-SECOND
    uint32_t hwtimer;     // hw timer period
    uint32_t period;      // period duration
    uint32_t tick_count;  // period tick count
    uint32_t step_count;  // step period_count
    uint32_t ramp_th1;    // ramp threshold 1 helper
    uint32_t ramp_th2;    // ramp threshold 2 helper
} stepper_state_t;

typedef struct {
    float    tvelocity;   // target velocity, STEPS-PER-SECOND
    float    accel;       // acceleration
    uint32_t steps;       // total number of steps required
    uint8_t  sem_pending; // use semaphore to signal sequence end
} stepper_config_t;

typedef struct {
    stepper_state_t  state;
    stepper_config_t config;
    stepper_config_t user_config;
    xSemaphoreHandle sem;
    uint32_t         mailbox;
} stepper_t;

static stepper_t g_stepper[STEPPER_MAX];

#define PHASE_MAX 8

static const uint8_t g_phase_bits[] = {
    1, 
    1|2, 
    2, 
    2|4, 
    4, 
    4|8, 
    8,
    8|1
};

#define ABS(a) ((a) > 0 ? (a) : -(a))
#define SIGN(a) ((a) >= 0 ? +1 : -1)
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

// stepper parameter calculator
static inline void stepper_setup_timer(stepper_state_t *state, stepper_config_t *config, const stepper_info_t *io)
{
    float period = 0;

    // stop timer if no longer needed
    if ((config->tvelocity == 0)
     && (ABS(state->velocity) < 1))
    {
      //UARTprintf("stepper_setup_timer: stopped, disabling timer\n");
      ROM_TimerLoadSet(io->timer_base, io->timer, 0); // stopped, timer off
      state->velocity = 0;

      return;
    }

    // velocity jumpstart to 1% if very slow or stopped
    //if (ABS(state->velocity) <= MAX(1, ABS(0.01 * config->tvelocity)))
    if (ABS(state->velocity) <= ABS(0.01 * config->tvelocity))
    {
        // round the other way since we're accellerating/deaccelerating
        // start at 1% of target velocity or 1, whichever is more
        state->velocity  = MAX(1, ABS(0.01 * config->tvelocity)) * SIGN(config->tvelocity - state->velocity);
    } 

    // convert velocity (STEP-PER-SECOND) to period (CLK-PER-PULSE)
    if (ABS(state->velocity) > FLOAT_ERROR) // almost zero
    {
        period = ABS((float)ROM_SysCtlClockGet() / state->velocity);
    } else {
        period = 0;
        state->velocity = 0;
    }

    // get int hwtimer first, can't be negative
    state->hwtimer = MIN(65535, period / ((isqrt(period/65536))));

    // recalculate soft period (float) to minimize error
    state->period = (uint32_t)period;

    ROM_TimerLoadSet(io->timer_base, io->timer, state->hwtimer);

#if 0
    UARTprintf("stepper_setup_timer:\n"
               "    velocity %i SPS, target velocity %i SPS, period %i, hwtimer %i, accel %i, step_count %i\n",
               (int32_t)state->velocity, (int32_t)config->tvelocity, (int32_t)state->period, state->hwtimer, (int32_t)config->accel, state->step_count);
#endif

}

int8_t stepper_init(uint8_t n)
{
    uint8_t i;

    memset(&g_stepper[0], 0, sizeof(g_stepper)); 

    for (i=0; i<MIN(n, STEPPER_MAX); i++)
    {
        ROM_TimerLoadSet(g_io[i].timer_base, g_io[i].timer, 0); // stopped, timer off

        SysCtlPeripheralEnable(g_io[i].io_peripheral);
        GPIOPinTypeGPIOOutput(g_io[i].io_port, g_pin_mask << g_io[i].base_pin);
        vSemaphoreCreateBinary(g_stepper[i].sem);
        xSemaphoreTake(g_stepper[i].sem, portMAX_DELAY);

        UARTprintf("Stepper driver %i initialized\n", i);
    }

    return(STEPPER_OK);
}

int8_t stepper_tick(uint8_t index)
{
    stepper_config_t *config = &g_stepper[index].config;
    stepper_state_t *state = &g_stepper[index].state;
    uint32_t *mailbox = &g_stepper[index].mailbox;
    xSemaphoreHandle sem = g_stepper[index].sem;

    //UARTprintf("%i", index);

    // copy mailbox data if signalled
    if (*mailbox == 1)
    {
        stepper_config_t *user_config = &g_stepper[index].user_config;

        memcpy(config, user_config, sizeof(*config));
        *mailbox = 0;

        // kick it ...
#if 0
        if ((config->accel == 0)
         || (state->velocity == 0))
        {
            if (config->accel == 0)
            {
                state->velocity = config->tvelocity;
                state->tick_count = state->period;
            }

            // set up timer delays
            stepper_setup_timer(state, config, &g_io[index]);
        }
#else
        stepper_setup_timer(state, config, &g_io[index]);

        state->tick_count = state->period;
#endif

        state->step_count = config->steps;
        state->ramp_th1 = config->steps >> 1; // ramp threshold 1
        state->ramp_th2 = (0.5 * config->tvelocity * config->tvelocity) / config->accel; // ramp threshold 2
#if 0
        UARTprintf("stepper_tick: @MAILBOX:\n"
                   "    id %i, velocity %i, delay %i, tick_count %i, step_count %i\n"
                   "    new values: velocity %i, accel %i, steps %i\n", 
                   index, (int32_t)state->velocity, (int32_t)state->period, (int32_t)state->tick_count, (int32_t)state->step_count, 
                   (int32_t)config->tvelocity, (int32_t)config->accel, (int32_t)config->steps);
#endif
    }

    if (state->period == 0) // idle but not disabled 
    {
        // wait some more
        return(STEPPER_WAITING);
    }

    // count ticks
    state->tick_count += state->hwtimer;

    if (state->tick_count < state->period) // sw period not reached?
    {
        // wait some more
        return(STEPPER_WAITING);
    }

    // reset the delay counter
    state->tick_count -= ABS(state->period);

    // are we velocitying up/slowing down?
    // if so, set up an intermediate velocity until the next cycle
    if ((config->accel != 0)
     && (state->velocity != config->tvelocity))
    {
        int8_t accel_sign = SIGN(config->tvelocity - state->velocity);

        // accelerate
        state->velocity += accel_sign * config->accel * (state->period / ((float)ROM_SysCtlClockGet()));

        if (((accel_sign > 0) && (state->velocity >= config->tvelocity))
         || ((accel_sign < 0) && (state->velocity <= config->tvelocity)))
        {
            // velocity reached
            state->velocity = config->tvelocity;
            //UARTprintf("stepper id %i at target velocity of %i sps\n", index, (int32_t)config->tvelocity);
        }

        // set up timer delays
        stepper_setup_timer(state, config, &g_io[index]);
    }

    if (state->velocity == 0)                        // stopped?
    {
      // wait until we have something to do
      return(STEPPER_STOPPED);
    }

    // do the step counting
    if (state->step_count != 0)
    {
    	state->step_count--;

        // start slowing down at some point?
        if ((state->step_count == state->ramp_th1 && (state->velocity != config->tvelocity)) // ramp threshold 1
         || (state->step_count == state->ramp_th2)) // ramp threshold 2
        {
            // start stopping ...
            //UARTprintf("slowing down\n");
            config->tvelocity = 0;
        }

        if (state->step_count == 0) // stop
        {
            signed portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

            //UARTprintf("stopped\n");
            config->tvelocity = state->velocity = 0;

            // set up timer delays
            stepper_setup_timer(state, config, &g_io[index]); // final nail in the coffin ...

            // signal waiting task, if any
            if ((sem != NULL) && (config->sem_pending == true))
            {
                xSemaphoreGiveFromISR(sem, &xHigherPriorityTaskWoken);
                portEND_SWITCHING_ISR(xHigherPriorityTaskWoken);
            }
        }
    } 

    // Output the bit sequence.
    //
    GPIOPinWrite(g_io[index].io_port, g_pin_mask << g_io[index].base_pin, g_phase_bits[state->phase] << g_io[index].base_pin);
    //UARTprintf(".");

    // Advance phase
    if (state->velocity > 0)
    {
        state->phase++;

        if (state->phase == PHASE_MAX)
        {
            state->phase = 0;
        }
    }
    else if (state->velocity < 0)
    {
        if (state->phase == 0)
        {
            state->phase = PHASE_MAX;
        }

        state->phase--;
    }

    return(STEPPER_MOVING);
}

// stepper cmd from user
int8_t stepper_go(uint8_t index, float velocity, float acceleration, int32_t steps)
{
    stepper_config_t *config;
    uint32_t *mailbox;
    const stepper_info_t *io;

    if (index >= STEPPER_MAX)
    {
        return(STEPPER_ERROR);
    }

    config = &g_stepper[index].user_config;
    mailbox = &g_stepper[index].mailbox;

   // negative steps => flip velocity
   if (steps < 0)
   {
       steps = -steps;
       velocity = -velocity;
   }

    // save parameters
    config->steps = steps;
    config->tvelocity = velocity;      // STEP-PER-SECOND
    config->accel = ABS(acceleration); // SPS^2

    // use semaphore if moving a number of steps
    if (config->steps != 0)
    {
        config->sem_pending = true;
    } else {
        config->sem_pending = false;
    }

    UARTprintf("stepper_go:\n"
               "    id %i, velocity %i, accel %i, steps %i, sem_pending %i\n", 
               index, (int32_t)config->tvelocity, (int32_t)config->accel, config->steps,
               config->sem_pending);

    // kick it if stopped or stopping
    io = &g_io[index];

    // kickstart ...
    if (ROM_TimerLoadGet(g_io[index].timer_base, g_io[index].timer) == 0)
    {
        // latch in the config
        *mailbox = 1;

        IntPendSet(io->interrupt);
    } else {
        // just latch in the config
        *mailbox = 1;
    }

    return(STEPPER_OK);
}

// stop
int8_t stepper_stop(uint8_t index, uint8_t hard_stop)
{
    if (index >= STEPPER_MAX)
    {
        return(STEPPER_ERROR);
    }

    if (hard_stop == 1)
    {
      stepper_go(index, 0, 0, 0);
    }
    else
    {
      stepper_go(index, 0, g_stepper[index].config.accel, 0);
    }

    UARTprintf("stepper_stop: index %i\n", index);
    return(STEPPER_OK);
}

// turn off stepper drive/park
int8_t stepper_idle(uint8_t index)
{
    if (index >= STEPPER_MAX)
    {
        return(STEPPER_ERROR);
    }

    stepper_stop(index, 1); // stop first
    GPIOPinWrite(g_io[index].io_port, g_pin_mask << g_io[index].base_pin, 0); // turn off stepper drive

    UARTprintf("stepper_idle: index %i\n", index);
    return(STEPPER_OK);
}

// print out status
int8_t stepper_status(uint8_t index)
{
    stepper_config_t config;
    stepper_state_t state;

    if (index >= STEPPER_MAX)
    {
        return(STEPPER_ERROR);
    }

    config = g_stepper[index].config;
    state = g_stepper[index].state;

    UARTprintf("stepper_status: velocity %i SPS, target velocity %i SPS, delay %i, hwtimer %i, accel %i, steps %i, step_count %i, tick_count %i, mailbox %i\n",
    		(int32_t)state.velocity, (int32_t)config.tvelocity, (int32_t)state.period, state.hwtimer, (int32_t)config.accel, config.steps, state.step_count, 
                (int32_t)state.tick_count, g_stepper[index].mailbox);

    return(STEPPER_OK);
}

// wait for completion of step sequence
int8_t stepper_waitfor(uint8_t index)
{
    stepper_state_t *state;
    stepper_config_t *config;
    xSemaphoreHandle sem;

    if (index >= STEPPER_MAX)
    {
        return(STEPPER_ERROR);
    }

    state = &g_stepper[index].state;
    config = &g_stepper[index].config;
    sem = g_stepper[index].sem;

    if (config->sem_pending == true)
    {
        UARTprintf("stepper_waitfor: waiting for stepper %i\n", index);
        xSemaphoreTake(sem, portMAX_DELAY);
    }
    else
    {
        UARTprintf("stepper_waitfor: NOT waiting for stepper %i\n", index);
        return(STEPPER_ERROR); // nothing to wait on
    }

    return(STEPPER_OK);
}

void stepper_scan(uint8_t index, float velocity, float acceleration, int32_t steps)
{
    stepper_go(index, velocity, 0, 0);

    while (1)
    {
        stepper_go(index, -velocity, acceleration, steps);
        stepper_go(index, +velocity, acceleration, steps);
    }
}
