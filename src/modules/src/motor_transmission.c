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

#define CENTER_FREQ 10000
#define BANDWIDTH 500
#define SYMBOL_LENGTH 100
#define SYMBOL_PAUSE 400
// #define SYMBOL_PAUSE_COMPENSATION 50 // the mottors continue to resonate 50 ms
#define SIMPLIFIED_MODULATION false

// Static global variables
static const unsigned int TASK_PERIOD = 25;
static xTimerHandle task_timer;

static bool initialized = false;
static struct ModulationScheme transmission_m123;
static struct ModulationScheme transmission_m4;

static uint8_t transmit = 0;
static uint8_t message = 0x55; // 0b01010101

static uint8_t error_code = 0;

// Parameters
#ifdef TESTING_MODULE
static uint16_t center_freq_a     = CENTER_FREQ;
static uint16_t center_freq_b     = CENTER_FREQ;
static uint16_t bandwidth         = BANDWIDTH;
static uint16_t symbol_length     = SYMBOL_LENGTH;
static uint16_t symbol_pause      = SYMBOL_PAUSE;
static bool update_module_params  = false;

// static bool use_m123 = true;
// static bool use_m4   = true;

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
 * Functions' pointer is used to transmit commands to the motors.
 */
static void transmit_motor4(const uint32_t frequency);
static void transmit_motor123(const uint32_t frequency);


void motorTransmissionInit(void)
{
    int res;

    if (initialized) {
        return;
    }

    transmission_m4 = mod_create(CENTER_FREQ, BANDWIDTH, SYMBOL_LENGTH,
            TASK_PERIOD, &transmit_motor4);
    transmission_m123 = mod_create(CENTER_FREQ, BANDWIDTH, SYMBOL_LENGTH,
            TASK_PERIOD, &transmit_motor123);

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

// static uint8_t stream[] = {0x00, 0x55, 0x45, 0x0C, 0xA9, 0xFF};
// Hello world!
// static uint8_t stream[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x57, 0x6F, 0x72, 0x6C, 0x64, 0x21};

static uint8_t hello[] = "Hello world! :) \n";
static uint8_t chosen_char_a = 0U;
static uint8_t chosen_char_b = 1U;
// static uint8_t stream[] = {0x48};
// static uint8_t index_a = 0xFF;
static bool transmitting_stream = false;

static void update_transmission()
{
    static int pause = 0;

#ifdef TESTING_MODULE
    check_update_module_parameters();
#endif

    mod_transmit(&transmission_m123);
    mod_transmit(&transmission_m4);

    if (transmit == 1 && !transmitting_stream) {
        pause ++;
        if (pause >= symbol_pause / TASK_PERIOD) {
            pause = 0;
        } else {
            return;
        }
    }


    if (transmit && !transmitting_stream) {
        // transmit = 0;
        transmitting_stream = true;
        error_code = mod_prepare_and_transmit(&transmission_m123, hello[chosen_char_a]);
        error_code = mod_prepare_and_transmit(&transmission_m4, hello[chosen_char_b]);
        return;
    }

    if (transmit == 0) {
        transmitting_stream = false;
    }

    if (transmitting_stream && mod_is_transmitting(&transmission_m123) == false) {
        transmitting_stream = false;
    }

    if (transmitting_stream && mod_is_transmitting(&transmission_m4) == false) {
        transmitting_stream = false;
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

static void transmit_motor123(const uint32_t frequency)
{
    static uint32_t last_frequency = 0;
    if (last_frequency != frequency) {

        motorsSetFrequency(MOTOR_M1, (uint16_t)frequency);
        motorsSetFrequency(MOTOR_M2, (uint16_t)frequency);
        motorsSetFrequency(MOTOR_M3, (uint16_t)frequency);

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

    mod_destruct(&transmission_m123);
    transmission_m123 = mod_create(center_freq_a, bandwidth, symbol_length,
            TASK_PERIOD, &transmit_motor123);

    mod_destruct(&transmission_m4);
    transmission_m4 = mod_create(center_freq_b, bandwidth, symbol_length,
            TASK_PERIOD, &transmit_motor4);
}
#endif



// Crazyflie API for communication
PARAM_GROUP_START(m_comms)
PARAM_ADD(PARAM_UINT8, Transmit, &transmit)
PARAM_ADD(PARAM_UINT8, Message, &message)
PARAM_ADD(PARAM_UINT8, ChosenChar_a, &chosen_char_a)
PARAM_ADD(PARAM_UINT8, ChosenChar_b, &chosen_char_b)

PARAM_ADD(PARAM_UINT16, PausedTime_ms, &symbol_pause)

#ifdef TESTING_MODULE
PARAM_ADD(PARAM_UINT16, center_freq_a,   &center_freq_a)
PARAM_ADD(PARAM_UINT16, center_freq_b,   &center_freq_b)
PARAM_ADD(PARAM_UINT16, bandwidth,     &bandwidth)
PARAM_ADD(PARAM_UINT16, symbol_length, &symbol_length)
PARAM_ADD(PARAM_UINT8, update_params,  &update_module_params)
// PARAM_ADD(PARAM_UINT8, use_m123, &use_m123)
// PARAM_ADD(PARAM_UINT8, use_m4,   &use_m4)
#endif

PARAM_GROUP_STOP(m_comms)

LOG_GROUP_START(m_comms)
LOG_ADD(LOG_UINT8, is_trans, &transmission_m4._send_req.is_transmitting)
LOG_ADD(LOG_UINT8, err_code, &error_code)
LOG_GROUP_STOP(m_comms)
