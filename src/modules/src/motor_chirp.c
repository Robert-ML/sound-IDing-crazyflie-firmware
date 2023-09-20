#include "motor_chirp.h"

/**
 * Copied from sound_motors_cf2.c with heavy modification
 **/
#include <stdbool.h>

/* FreeRtos includes */
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "timers.h"
#include "worker.h"

/* Crazyflie driver includes */
#include "config.h"
#include "param.h"
#include "log.h"
#include "motors.h"
// TODO: convert all variables to camelCase

#define MSG_LENGTH 8
// how many task ticks to wait between chirp symbols
#define PAUSE_LENGTH 2
// how many task ticks to broadcast the ID
#define BROADCAST_LENGTH 100
static struct {
  uint16_t topF;
  uint16_t bottomF;
  uint16_t dF;
  uint8_t message;
  uint8_t msgCounter;
} _msgParams[4];

MotorChirpParameters _chirpParams[4] = {{13000, 500, 2000}, {15000, 500, 2000}, {13000, 500, 2000}, {15000, 500, 2000}};

static bool isInit=false;
static uint8_t message[4] = {0xaa, 0xaa, 0xaa, 0xaa};

static bool requestChirp = false;
static uint16_t startFreq[4] = {0, 0, 0, 0};
static uint16_t endFreq[4]   = {0, 0, 0, 0};
static int16_t fStep[4]      = {0, 0, 0, 0};
static uint16_t lastFreq[4]  = {0, 0, 0, 0};
static uint16_t motorFreq[4] = {0, 0, 0, 0};

static bool inPause[4] = {false, false, false, false};
static uint8_t pauseTicks[4] = {0, 0, 0, 0};

#define SND_TASK_INTERVAL 50

static bool doingMsg[4] = {false, false, false, false};

static xTimerHandle timer;

static void updateChirpFreq(const uint32_t id);

static void setupNextChirp(const uint32_t id)
{
  // we don't check msgCounter here........
  pauseTicks[id] = 0;
  bool isDownChirp = !(_msgParams[id].message & (1 << _msgParams[id].msgCounter));
  if (isDownChirp){
    startFreq[id] = _msgParams[id].topF;
    endFreq[id] = _msgParams[id].bottomF;
    fStep[id] = -_msgParams[id].dF;
  } else {
    // upchirp
    startFreq[id] = _msgParams[id].bottomF;
    endFreq[id] = _msgParams[id].topF;
    fStep[id] = _msgParams[id].dF;
  }
  motorFreq[id] = startFreq[id];
  _msgParams[id].msgCounter += 1;
}

static void setupMessage(const uint32_t id)
{
  int16_t totalFChange = (int16_t)((float)_chirpParams[id].chirpSlope * _chirpParams[id].chirpLen / 1000.0f);
  _msgParams[id].dF = (int16_t)((float)_chirpParams[id].chirpSlope * SND_TASK_INTERVAL / 1000.0f);
  _msgParams[id].bottomF = _chirpParams[id].centerFreq - totalFChange/2;
  _msgParams[id].topF = _chirpParams[id].centerFreq + totalFChange/2;
  _msgParams[id].message = message[id];
  _msgParams[id].msgCounter = 0;
  inPause[id] = false;
}

static void updateAllChirpsFreq()
{
  uint32_t i;

  for (i = 0; i < NBR_OF_MOTORS; ++i)
  {
    updateChirpFreq(i);
  }
}

static void updateChirpFreq(const uint32_t id)
{

  if (!doingMsg[id] && requestChirp)
  {
    doingMsg[id] = true;
    if (id == 3)
    {
      requestChirp = false;
    }

    setupMessage(id);
    setupNextChirp(id);
  }

  if (motorFreq[id] != lastFreq[id])
  {
    motorsSetFrequency(id, motorFreq[id]);
    lastFreq[id] = motorFreq[id];
}

  if (doingMsg[id])
  {
    if (inPause[id])
    {
      pauseTicks[id] += 1;
      if (pauseTicks[id] > PAUSE_LENGTH)
      {
        inPause[id] = false;
	      setupNextChirp(id);
      }
      else
      {
        return; // don't do anything if we were pausing
      }
    }
    // we were not pausing, see if we're done with this symbol
    bool finished = (fStep[id] < 0 && motorFreq[id] < endFreq[id]) || (fStep[id] > 0 && motorFreq[id] > endFreq[id]);
    if (finished)
    { // symbol done
      motorFreq[id] = 0;
      if (_msgParams[id].msgCounter >= MSG_LENGTH)
      {
        // we have finished all the symbols in the message
        doingMsg[id] = false;
      }
      else
      {
        // we just have to wait a few ticks to the next symbol
        inPause[id] = true;
      }
    }
    else
    {
      // we weren't done with the symbol
      motorFreq[id] += fStep[id];
    }
  }
}

static void motorSoundTimer(xTimerHandle timer)
{
  workerSchedule(updateAllChirpsFreq, NULL);
}

void motorSoundInit(void)
{
  if (isInit) {
    return;
  }

  timer = xTimerCreate("ChpTask", M2T(SND_TASK_INTERVAL), pdTRUE, NULL, motorSoundTimer);
  isInit = (timer != 0);
  xTimerStart(timer, 100);
}

bool motorChirpTest(void)
{
  return isInit;
}

const MotorChirpParameters *currentMotorParams()
{
  return &(_chirpParams[0]);
}
PARAM_GROUP_START(chirp)
PARAM_ADD(PARAM_UINT16, center_0, &(_chirpParams[0].centerFreq))
PARAM_ADD(PARAM_UINT16, center_1, &(_chirpParams[1].centerFreq))
PARAM_ADD(PARAM_UINT16, center_2, &(_chirpParams[2].centerFreq))
PARAM_ADD(PARAM_UINT16, center_3, &(_chirpParams[3].centerFreq))
PARAM_ADD(PARAM_UINT8, message_1, &(message[0]))
PARAM_ADD(PARAM_UINT8, message_2, &(message[1]))
PARAM_ADD(PARAM_UINT8, message_3, &(message[2]))
PARAM_ADD(PARAM_UINT8, message_4, &(message[3]))
PARAM_ADD(PARAM_UINT8, goChirp,   &requestChirp)
PARAM_GROUP_STOP(chirp)

LOG_GROUP_START(chirp)
LOG_ADD(LOG_UINT16, freq, &motorFreq)
LOG_ADD(LOG_INT16, dF, &fStep)
LOG_GROUP_STOP(chirp)
