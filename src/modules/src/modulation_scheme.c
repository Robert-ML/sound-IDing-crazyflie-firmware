#include "modulation_shceme.h"

#include <stdbool.h>
#include <stdint.h>


#define divide_and_ceil(dest, x, y) do {                                      \
    if ((x) % (y)) {                                                          \
        (dest) = (x) / (y) + 1;                                               \
    } else {                                                                  \
        (dest) = (x) / (y);                                                   \
    }                                                                         \
} while (0);



static void conv_to_symbols(enum kModSymbols msg[8], const uint8_t m);
static int32_t try_tx_current_symbol(struct ModulationScheme *const ms);

struct ModulationScheme mod_create(const uint32_t center_freq,
        const uint32_t bandwidth, const uint32_t symbol_length_ms,
        const uint32_t task_period_ms, void (*transmit_func)(const uint32_t))
{
    uint32_t low_freq, high_freq, symbol_len;

    low_freq  = center_freq - bandwidth / 2;
    high_freq = center_freq + bandwidth / 2;
    divide_and_ceil(symbol_len, symbol_length_ms, task_period_ms);

    // Initialize the properties
    struct ModProps props = {
        .low_freq          = low_freq,
        .high_freq         = high_freq,
        .symbol_len        = symbol_len,
        .transmit_func     = transmit_func,
    };

    // Ensure important fields in are initialized
    struct ModSendRequest send_req = {
        .is_transmitting = 0,
        .index = 0,
        .counter = 0,
        .curr_freq = 0,
    };

    struct ModulationScheme ret = {
        .props = props,
        ._send_req = send_req,
    };

    return ret;
}

int mod_prepare_and_transmit(struct ModulationScheme *const ms,
        const uint8_t m)
{
    struct ModSendRequest *const sr = &ms->_send_req;

    if (sr->is_transmitting) {
        return -1; // is already sending something
    }

    sr->index = 0;
    // convert the byte to symbols
    conv_to_symbols(sr->msg, m);

    // start transmission
    sr->is_transmitting = 1;

    // transmit
    try_tx_current_symbol(ms);

    return 0;
}

/**
 * Function that takes care of transmitting the symbols and their modulation.
 *
 * After each symbol there is a pause. Implementation wise, the pause is
 * between each symbol, and one more after the last symbol (cooldown time).
 */
int mod_transmit(struct ModulationScheme *const ms)
{
    struct ModSendRequest *const sr = &ms->_send_req;

    if (!sr->is_transmitting) {
        return -1;
    }

    // Tick the counter for how long the symbol was transmitted
    ++sr->counter;

    // Check if we still have to transmit this symbol
    if (sr->counter < ms->props.symbol_len) {
        return 0;
    }

    // If we reached here, we must go to the next symbol
    ++sr->index;

    // no other symbols to transmit
    if (sr->index == SYMBOL_NO_IN_BYTE) {
        ms->props.transmit_func(0);
        sr->is_transmitting = 0;
        return 1;
    }

    // transmit the current symbol pointed by sr->index
    try_tx_current_symbol(ms);


    return 1;
}

bool mod_is_transmitting(const struct ModulationScheme *const ms)
{
    if (ms->_send_req.is_transmitting) {
        return true;
    } else {
        return false;
    }
}

bool mod_force_stop_transmission(struct ModulationScheme *const ms)
{
    if (!mod_is_transmitting(ms)) {
        return false;
    }

    ms->_send_req.is_transmitting = 0;
    ms->props.transmit_func(0); // reset the transmitter

    return true;
}

bool mod_destruct(struct ModulationScheme *const ms)
{
    bool ret = mod_force_stop_transmission(ms);

    // clear the data to make sure the struct is no longer usable
    ms->props.low_freq          = 0;
    ms->props.high_freq         = 0;
    ms->props.symbol_len        = 0;

    return ret;
}


/**
 * Function to convert a byte in symbols from enum kModSymbols.
 *
 * @param[out] msg[8] Is an 8 element array where the symbols used to represent
 * the byte to be transmitted are returned. The order is from most significant
 * bit to least significant bit.
 *
 * @param m The byte to be converted to the symbols.
 *
 * @note A m of value 0b11000110 will be equivalent to msg[8] = {HIGH, HIGH,
 * LOW, LOW, LOW, HIGH, HIGH, LOW}.
 */
static void conv_to_symbols(enum kModSymbols msg[8], const uint8_t m)
{
    int i;
    uint8_t m_mask;

    // use one symbol to represent a bit, symbols_per_byte = 8
    for (i = 0; i < SYMBOL_NO_IN_BYTE; ++i) {
        m_mask = 0x01 << (SYMBOL_NO_IN_BYTE - 1 - i);

        if (m & m_mask) {
            msg[i] = HIGH;
        } else {
            msg[i] = LOW;
        }
    }
    return;
}

/**
 * Tries to transmit the current symbol pointed by `ms->_send_req.index` if in
 * transmission mode and if there are remaining symbols to be transmitted.
 * Resets the `ms->_send_req.counter` to 0.
 *
 * @param[in out] ms A structure ModulationScheme created by using the function
 * mod_create.
 *
 * @return -1 if not in transmission mode, -2 if the symbols to transmit ended
 * and 0 if could transmit the current pointed symbol
*/
int32_t try_tx_current_symbol(struct ModulationScheme *const ms)
{
    struct ModSendRequest *const sr = &ms->_send_req;

    if (!sr->is_transmitting) {
        return -1;
    }

    if (sr->index >= SYMBOL_NO_IN_BYTE) {
        return -2;
    }

    // transmit the symbol
    sr->counter = 0;
    const enum kModSymbols current_symbol = sr->msg[sr->index];
    if (current_symbol == LOW) {
        sr->curr_freq = ms->props.low_freq;
    } else {
        sr->curr_freq = ms->props.high_freq;
    }

    ms->props.transmit_func(sr->curr_freq);

    return 0;
}
