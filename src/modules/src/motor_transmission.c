#include "motor_transmission.h"

// C includes
#include <stdbool.h>
#include <stdint.h>

// FreeRTOS includes
#include "FreeRTOS.h"
#include "timers.h"

// This project's includes
#include "motors.h"
#include "modulation_shceme.h"
#include "worker.h"
#include "param.h"
#include "log.h"

// Defines
#define TESTING_MODULE

#define CENTER_FREQ 17000
#define BANDWIDTH 1000
#define SYMBOL_LENGTH 1000
// #define SYMBOL_PAUSE 100
#define SYMBOL_PAUSE_COMPENSATION 50 // the mottors continue to resonate 50 ms
#define SIMPLIFIED_MODULATION false

// Static global variables
static const unsigned int TASK_PERIOD = 25;
static xTimerHandle task_timer;

static bool initialized = false;
static struct ModulationScheme transmission_m4;

static uint8_t transmit = 0;
static uint8_t message = 0x55; // 0b01010101

static uint8_t error_code = 0;

// Parameters
#ifdef TESTING_MODULE
static uint16_t center_freq       = CENTER_FREQ;
static uint16_t bandwidth         = BANDWIDTH;
static uint16_t symbol_length     = SYMBOL_LENGTH;
// static uint16_t symbol_pause      = SYMBOL_PAUSE;
static bool simplifyed_modulation = SIMPLIFIED_MODULATION;
static bool update_module_params  = false;
#endif

// Funciton prototypes
/**
 * Periodic FreeRTOS task to schedule a worker to execute the function
 * update_transmission. The worker is a Crazyflie API.
 */
static void periodic_task(xTimerHandle timer);

/**
 * Ticks the transmission object, and if requested and able, starts a new
 * transmission.
 */
static void update_transmission();

#ifdef TESTING_MODULE
/**
 * Updates the parameters of the module.
*/
static void check_update_module_parameters();
#endif

/**
 * Function's pointer is used to transmit commands to motor 4.
 */
static void transmit_motor4(const uint32_t frequency);


void motorTransmissionInit(void)
{
    int res;

    if (initialized) {
        return;
    }

    transmission_m4 = mod_create(CENTER_FREQ, BANDWIDTH, SYMBOL_LENGTH,
            TASK_PERIOD, &transmit_motor4);

    task_timer = xTimerCreate("MotorTransmissionTask", M2T(TASK_PERIOD),
            pdTRUE, NULL, periodic_task);
    if (task_timer == 0) {
        return;
    }

    res = xTimerStart(task_timer, 100);
    if (res == pdFAIL) {
        return;
    }

    initialized = true;
}

bool motorTransmissionTest(void)
{
    return initialized;
}


static void periodic_task(xTimerHandle timer)
{
    workerSchedule(update_transmission, NULL);
}

static void update_transmission()
{
#ifdef TESTING_MODULE
    check_update_module_parameters();
#endif

    mod_transmit(&transmission_m4);

    if (transmit && mod_is_transmitting(&transmission_m4) == false) {
        // transmit = 0;
        error_code = mod_prepare_and_transmit(&transmission_m4, message);
    }

}

static void transmit_motor4(const uint32_t frequency)
{
    static uint32_t last_frequency = 0;
    if (last_frequency != frequency) {
        motorsSetFrequency(MOTOR_M4, (uint16_t)frequency);
        last_frequency = frequency;
    }
}

#ifdef TESTING_MODULE
/**
 * Updates the parameters of the module.
*/
static void check_update_module_parameters()
{
    if (!update_module_params) {
        return;
    }

    update_module_params = false;

    mod_destruct(&transmission_m4);

    transmission_m4 = mod_create(center_freq, bandwidth, symbol_length,
            TASK_PERIOD, &transmit_motor4);
}
#endif



// Crazyflie API for communication
PARAM_GROUP_START(m_comms)
PARAM_ADD(PARAM_UINT8, Transmit, &transmit)
PARAM_ADD(PARAM_UINT8, Message, &message)

#ifdef TESTING_MODULE
PARAM_ADD(PARAM_UINT16, center_freq,   &center_freq)
PARAM_ADD(PARAM_UINT16, bandwidth,     &bandwidth)
PARAM_ADD(PARAM_UINT16, symbol_length, &symbol_length)
// PARAM_ADD(PARAM_UINT16, symbol_pause,  &symbol_pause)
PARAM_ADD(PARAM_UINT8, simple_mod, &simplifyed_modulation)
PARAM_ADD(PARAM_UINT8, update_params,  &update_module_params)
#endif

PARAM_GROUP_STOP(m_comms)

LOG_GROUP_START(m_comms)
LOG_ADD(LOG_UINT8, is_trans, &transmission_m4._send_req.is_transmitting)
LOG_ADD(LOG_UINT8, err_code, &error_code)
LOG_GROUP_STOP(m_comms)
