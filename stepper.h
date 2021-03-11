//*****************************************************************************
//
// stepper.h - Prototypes for the stepper code
//
//*****************************************************************************

#ifndef __STEPPER_H__
#define __STEPPER_H__

typedef enum
{
  STEPPER_WAITING = -2,
  STEPPER_ERROR = -1,
  STEPPER_OK,
  STEPPER_STOPPED,
  STEPPER_MOVING,
  STEPPER_IDLE
} stepper_status_t;

//*****************************************************************************
//
// Prototypes for the STEPPER
//
//*****************************************************************************
int8_t stepper_init(uint8_t n);
int8_t stepper_tick(uint8_t index);
int8_t stepper_go(uint8_t index, float velocity, float acceleration, int32_t steps);
int8_t stepper_stop(uint8_t index, uint8_t hard_stop);
int8_t stepper_idle(uint8_t index);
int8_t stepper_waitfor(uint8_t index);
int8_t stepper_status(uint8_t index);
void stepper_scan(uint8_t index, float velocity, float acceleration, int32_t steps);

#endif // __STEPPER_H__
