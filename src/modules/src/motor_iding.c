#include "motor_iding.h"

/* Standard includes */
#include <stdbool.h>
#include <stdint.h>

/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "timers.h"
#include "worker.h"
// #include "worker.h"

/* Crazyflie driver includes */
#include "log.h"
#include "motors.h"
#include "param.h"


/* Constants */
#define SND_TASK_INTERVAL 50 // how often to run the task in ms
#define ID_BROADCAST_LENGTH 4 // how long to broadcast the ID counted in how many times the task is run
#define DEFAULT_ID 16000
#define DEFAULT_MOTORS_USED 1U // bitmap 0001 - use just motor 1

/* FreeRTOS task variables */
static xTimerHandle timer; // timer for the periodic task

/* Module variables */
static bool isInit = false; // whether the module has been initialized

static uint32_t frequency = DEFAULT_ID; // frequency of the sound which represents the ID
static uint8_t motorsUsedBitmap = DEFAULT_MOTORS_USED; // motors to be used for broadcast: 0001 - motor 1, 1010 - motor 2 & 4
static bool broadcastId = false; // whether was asked to send the ID

/* Function declarations */
static void motorIdingTask(xTimerHandle timer);
static void updateIdingSound();


static void updateIdingSound()
{
  static bool broadcastingId = false; // whether is currently sending the ID
  static uint8_t broadcastingCount = 0U; // how many task calls has sent the ID
  static uint8_t localMotorsUsed = 0U; // motors that were used when set-up the broadcast

  int i;

  if (!broadcastId && !broadcastingId)
  {
    return;
  }

  // request to start broadcasting the ID
  if (broadcastId && !broadcastingId)
  {
    broadcastId = false;
    broadcastingId = true;

    // configure the motors
    localMotorsUsed = motorsUsedBitmap & 0x0F;
    for (i = 0; i < NBR_OF_MOTORS; ++i)
    {
      if (localMotorsUsed & (1 << i))
      {
        motorsSetFrequency(i, frequency);
      }
    }
  }
  else if (broadcastId && broadcastingId) // ignore a new send request if already sending
  {
    broadcastId = false;
  }

  if (broadcastingId)
  {
    ++broadcastingCount;

    if (broadcastingCount <= ID_BROADCAST_LENGTH)
    {
      return;
    }

    // broadcasted for long enough and now we stop
    broadcastingId = false;
    broadcastingCount = 0U;
    // restore the motors to their usual frequency
    for (i = 0; i < NBR_OF_MOTORS; ++i)
    {
      if (localMotorsUsed & (1 << i))
      {
        motorsSetFrequency(i, 0U);
      }
    }
    localMotorsUsed = 0U;
  }
}

static void motorIdingTask(xTimerHandle timer)
{
  workerSchedule(updateIdingSound, NULL);
}

void motorIdingInit(void)
{
  if (isInit)
  {
    return;
  }

  timer = xTimerCreate("IdingTask", M2T(SND_TASK_INTERVAL), pdTRUE, NULL,
      motorIdingTask);
  isInit = (timer != NULL);
  xTimerStart(timer, 100);
}

bool motorIdingTest(void)
{
  return isInit;
}


// Crazyflie Param & Log API
PARAM_GROUP_START(iding)
PARAM_ADD(PARAM_UINT32, ID, &frequency)
PARAM_ADD(PARAM_UINT8, motors_used_bitmap, &motorsUsedBitmap)
PARAM_ADD(PARAM_UINT8, broadcast_id, &broadcastId)
PARAM_GROUP_STOP(iding)
