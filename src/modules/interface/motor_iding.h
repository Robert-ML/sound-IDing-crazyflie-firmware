#ifndef __MOTOR_IDING_H__
#define __MOTOR_IDING_H__

#include <stdbool.h>

/**
 * @brief Initializes the periodic task for generating the ID of the drone
 * using the motors' sound.
*/
void motorIdingInit(void);

/**
 * @brief Tests the motor iding module.
*/
bool motorIdingTest(void);

#endif /* __MOTOR_IDING_H__ */
