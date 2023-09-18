#ifndef __MOTOR_CHIRP_H__
#define __MOTOR_CHIRP_H__

#include <stdbool.h>
#include <stdint.h>

// The motor sound module lets me change the pwm parameters of the motors
// and set up chirp frequencies

typedef struct {
  uint16_t centerFreq;
  uint16_t chirpLen;
  uint16_t chirpSlope;
} MotorChirpParameters;


void motorSoundInit(void);

bool motorChirpTest(void);

const MotorChirpParameters *currentMotorParams();


#endif /* __MOTOR_CHIRP_H__ */
