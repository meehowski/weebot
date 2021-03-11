//*****************************************************************************
//
// platform.c
//
//*****************************************************************************

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <inttypes.h>

#include "FreeRTOS.h"
//#include "semphr.h"

#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/debug.h"
#include "driverlib/rom.h"
#include "driverlib/sysctl.h"
#include "driverlib/timer.h"
#include "utils/uartstdio.h"
#include "stepper.h"
#include "platform.h"

#define ABS(a) ((a) > 0 ? (a) : -(a))
#define SIGN(a) ((a) >= 0 ? +1 : -1)
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

int8_t platform_init(void)
{
  UARTprintf("Platform driver initialized (steppers: right=%i, left=%i)\n", PLATFORM_STEPPER_R, PLATFORM_STEPPER_L);

  return(PLATFORM_OK);
}

#define UARTprintf_float(fv) { double i, f; f=modf((fv), &i); UARTprintf("%c%i.%04i", ((fv)<0?'-':'+'), (int32_t)abs(i), (int32_t)(abs(f*10000.0))); }

int8_t platform_go(float velocity, float acceleration, float angular_velocity, float distance)
{
  // equations borrowed from:
  // http://rossum.sourceforge.net/papers/CalculationsForRobotics/DifferentialWheelVelocity/index.htm
  // http://www.cs.columbia.edu/~allen/F11/NOTES/icckinematics.pdf

  float velocity_r, velocity_l;
  float sps_r, sps_l;
  int32_t numsteps_r, numsteps_l;
  float acceleration_r, acceleration_l;
  float radius = angular_velocity;
  float ratio_r, ratio_l;

#if 0
  if (radius != 0)
  {
    angular_velocity = velocity / (radius * 2 * PI);
  }
#endif

  // derive wheel velocities
  //velocity_r = velocity + angular_velocity * (radius + (PLATFORM_WHEEL_BASE / 2));
  //velocity_r = velocity - angular_velocity * radius + angular_velocity * (radius + (PLATFORM_WHEEL_BASE / 2));
  velocity_r = velocity + 2 * PI * angular_velocity * (PLATFORM_WHEEL_BASE / 2);

  //velocity_l = velocity + angular_velocity * (radius - (PLATFORM_WHEEL_BASE / 2));
  //velocity_l = velocity - angular_velocity * radius + angular_velocity * (radius - (PLATFORM_WHEEL_BASE / 2));
  velocity_l = velocity - 2 * PI * angular_velocity * (PLATFORM_WHEEL_BASE / 2);

  // max velocity limiters
  if ((velocity_r > +PLATFORM_MAX_VELOCITY)
    ||(velocity_r < -PLATFORM_MAX_VELOCITY))
  {
    float vlimiter = PLATFORM_MAX_VELOCITY / ABS(velocity_r);

    UARTprintf("vlimiter_r "); UARTprintf_float(vlimiter); UARTprintf("\n");

    velocity_r = velocity_r * vlimiter;
    velocity_l = velocity_l * vlimiter;
  }

  if ((velocity_l > +PLATFORM_MAX_VELOCITY)
    ||(velocity_l < -PLATFORM_MAX_VELOCITY))
  {
    float vlimiter = PLATFORM_MAX_VELOCITY / ABS(velocity_l);
    UARTprintf("vlimiter_l "); UARTprintf_float(vlimiter); UARTprintf("\n");

    velocity_r = velocity_r * vlimiter;
    velocity_l = velocity_l * vlimiter;
  }

  // last chance ...
  //velocity_r = MIN(velocity_r, +PLATFORM_MAX_VELOCITY);
  //velocity_r = MAX(velocity_r, -PLATFORM_MAX_VELOCITY);
  //velocity_l = MIN(velocity_l, +PLATFORM_MAX_VELOCITY);
  //velocity_l = MAX(velocity_l, -PLATFORM_MAX_VELOCITY);

  // calc radius
  radius = velocity / (angular_velocity * 2 * PI);

  // convert to motor parameters
  sps_r = (360 / PLATFORM_ANGLE_PER_STEP) * (velocity_r / PLATFORM_WHEEL_CIRCUMFERENCE);
  sps_l = (360 / PLATFORM_ANGLE_PER_STEP) * (velocity_l / PLATFORM_WHEEL_CIRCUMFERENCE);

  //sps_r = MIN(sps_r, +PLATFORM_MAX_STEPPER_SPS);
  //sps_r = MAX(sps_r, -PLATFORM_MAX_STEPPER_SPS);
  //sps_l = MIN(sps_l, +PLATFORM_MAX_STEPPER_SPS);
  //sps_l = MAX(sps_l, -PLATFORM_MAX_STEPPER_SPS);

  ratio_r = velocity_r / velocity;
  ratio_l = velocity_l / velocity;

  acceleration_r = acceleration * ratio_r;
  acceleration_r = (360 / PLATFORM_ANGLE_PER_STEP) * (acceleration_r / PLATFORM_WHEEL_CIRCUMFERENCE);

  acceleration_l = acceleration * ratio_l;
  acceleration_l = (360 / PLATFORM_ANGLE_PER_STEP) * (acceleration_l / PLATFORM_WHEEL_CIRCUMFERENCE);

  numsteps_r = (360 / PLATFORM_ANGLE_PER_STEP) * ((distance * ratio_r) / PLATFORM_WHEEL_CIRCUMFERENCE);
  numsteps_l = (360 / PLATFORM_ANGLE_PER_STEP) * ((distance * ratio_l) / PLATFORM_WHEEL_CIRCUMFERENCE);

  // debug output
  UARTprintf("platform_go:\n");
  UARTprintf("velocity "); UARTprintf_float(velocity);
  UARTprintf(", velocity_r "); UARTprintf_float(velocity_r);
  UARTprintf(", velocity_l "); UARTprintf_float(velocity_l);
  UARTprintf(", angular_velocity "); UARTprintf_float(angular_velocity);
  UARTprintf(", radius "); UARTprintf_float(radius);
  UARTprintf("\nsps_r "); UARTprintf_float(sps_r);
  UARTprintf(", acceleration_r "); UARTprintf_float(acceleration_r);
  UARTprintf(", numsteps_r %i", numsteps_r);
  UARTprintf("\nsps_l "); UARTprintf_float(sps_l);
  UARTprintf(", acceleration_l "); UARTprintf_float(acceleration_l);
  UARTprintf(", numsteps_l %i", numsteps_l);
  UARTprintf("\n\n");

  // execute
  stepper_go(PLATFORM_STEPPER_R, sps_r, acceleration_r, numsteps_r);
  stepper_go(PLATFORM_STEPPER_L, sps_l, acceleration_l, numsteps_l);

  stepper_waitfor(PLATFORM_STEPPER_R);
  stepper_waitfor(PLATFORM_STEPPER_L);

  return(PLATFORM_OK);
}

int8_t platform_stop(uint8_t hard_stop)
{
  stepper_stop(PLATFORM_STEPPER_R, hard_stop);
  stepper_stop(PLATFORM_STEPPER_L, hard_stop);

  return(PLATFORM_OK);
}

int8_t platform_idle()
{
  stepper_idle(PLATFORM_STEPPER_R);
  stepper_idle(PLATFORM_STEPPER_L);

  return(PLATFORM_OK);
}

int8_t platform_status()
{
  return(PLATFORM_OK);
}
