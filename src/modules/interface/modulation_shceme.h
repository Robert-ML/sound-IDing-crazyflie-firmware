#ifndef __MODULATION_SCHEME_H__
#define __MODULATION_SCHEME_H__

#include <stdbool.h>
#include <stdint.h>

enum kModSymbols {
    LOW  = 0x00, // 0 - Constant low freqeuncy signal
    HIGH = 0x01, // 1 - Constant high frequency signal
};

#define SYMBOL_NO_IN_BYTE 8

struct ModSendRequest {
    uint8_t is_transmitting; // 0 if NOT transmitting, else nonzero

    enum kModSymbols msg[8]; // Message split in 8 symbols to be transmitted
    uint8_t index; // Index of the symbol to be sent
    int8_t counter; // The iteration in transmission the current symbol is at

    uint32_t curr_freq; // Frequency currently being used
};

struct ModProps { // Modulation properties
    uint32_t low_freq;      // Low limit of the frequency
    uint32_t high_freq;     // High limit of the used frequency
    uint16_t symbol_len;    // How long in task ticks to send a symbol

    // The function used to transmit, 0 as argument means stopping the transmission
    void (*transmit_func)(const uint32_t);
};

struct ModulationScheme {
    struct ModProps props; // Properties

    // Private
    struct ModSendRequest _send_req; // Message to be sent
};

/**
 * Constructor of struct ModulationScheme that initializes the internal data.
 *
 * @param[in] center_freq The center frequency of the signal, used to determine
 * low_freq and high_freq in struct ModProps.
 *
 * @param[in] bandwidth The distance in frequency between low_freq and
 * high_freq in struct ModProps. It is used to calculate them. TODO: change to
 * frequency slope
 *
 * @param[in] symbol_length_ms How many milliseconds to take to send a symbol.
 * Used to calculate symbol_len in struct ModProps in terms of how many times
 * the task (task ticks) was called.
 *
 * @param[in] task_period_ms At what interval in milliseconds the task is called.
 * Used to calculate symbol_len in struct ModProps in terms of how many times
 * the task was called.
 *
 * @param[in] transmit_func The function used to change the frequency of the
 * motor/s's PWM to transmit information. When ending the transmission, 0 will
 * be passed to the function as argument and appropiate action must be taken.
 *
 * @return The newly constructed structure to be used for transmission.
 */
struct ModulationScheme mod_create(const uint32_t center_freq,
        const uint32_t bandwidth, const uint32_t symbol_length_ms,
        const uint32_t task_period_ms, void (*transmit_func)(const uint32_t));

/**
 * Function to prepare a new message for sending.
 *
 * @param[in out] ms A structure ModulationScheme created by using the function
 * mod_create.
 *
 * @param[in] m A message to send.
 *
 * @return 0 on success, negative number on error.
 */
int mod_prepare_and_transmit(struct ModulationScheme *const ms,
        const uint8_t m);

/**
 * Transmit a message. It must be called at each call of the
 * task to ensure it updates internally if there is a message prepared to be
 * transmitted. This function must NOT be called after preparing a message to
 * be sent with mod_prepare_and_transmit.
 *
 * @param[in out] ms A structure ModulationScheme created by using the function
 * mod_create.
 *
 * @return -1 if no message is to be sent, n symbols were sent (most probably
 * just 1). Does not take into account the pause between symbols.
 */
int mod_transmit(struct ModulationScheme *const ms);

/**
 * Check if a message is being transmitted.
 *
 * @param[in] ms A structure ModulationScheme created by using the function
 * mod_create.
 *
 * @return TRUE if now transmitting, FALSE otherwise
 */
bool mod_is_transmitting(const struct ModulationScheme *const ms);

/**
 * Stop any transmission forcefully.
 *
 * @param[in out] ms A structure ModulationScheme created by using the function
 * mod_create.
 *
 * @return TRUE if it was transmitting, FALSE otherwise
*/
bool mod_force_stop_transmission(struct ModulationScheme *const ms);

/**
 * Stops any transmission (resets the transmitter also) and clears the struct.
 * It is intended to be used when creating a new struct ModulationScheme
 * instance to make sure stuff is initialized correctly.
 *
 * @param[in out] ms  A structure ModulationScheme created by using the function
 * mod_create.
 *
 * @return TRUE if it was transmitting, FALSE otherwise
 */
bool mod_destruct(struct ModulationScheme *const ms);

#endif /* __MODULATION_SCHEME_H__ */
