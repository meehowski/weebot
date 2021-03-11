//*****************************************************************************
//
// platform.h - Prototypes for the platform code
//
//*****************************************************************************

#ifndef __PLATFORM_H__
#define __PLATFORM_H__

#define PI                              3.14159
#define PLATFORM_STEPPER_R              0
#define PLATFORM_STEPPER_L              1
#define PLATFORM_WHEEL_BASE             0.20 // 0.16
#define PLATFORM_WHEEL_DIAMETER         0.10 // 0.09
#define PLATFORM_WHEEL_CIRCUMFERENCE    (PLATFORM_WHEEL_DIAMETER*PI)
#define PLATFORM_ANGLE_PER_STEP         (5.625 / 64) // 28BYJ48
#define PLATFORM_MAX_STEPPER_SPS        1500

#define PLATFORM_MAX_VELOCITY           ((PLATFORM_MAX_STEPPER_SPS / ( 360 / PLATFORM_ANGLE_PER_STEP)) * PLATFORM_WHEEL_CIRCUMFERENCE)

typedef enum
{
  PLATFORM_ERROR = -1,
  PLATFORM_OK,
  PLATFORM_STOPPED,
  PLATFORM_MOVING,
  PLATFORM_IDLE
} platform_status_t;

//*****************************************************************************
//
// Prototypes for the PLATFORM
//
//*****************************************************************************
int8_t platform_init(void);
int8_t platform_go(float velocity, float acceleration, float angular_velocity, float distance);
int8_t platform_stop(uint8_t hard_stop);
int8_t platform_idle(void);
int8_t platform_status(void);

#endif // __PLATFORM_H__
