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

static uint8_t get_num_symbols_per_byte(const bool simplifyed_symbols);
static void conv_to_symbols(enum kModSymbols msg[8], const uint8_t m,
        const bool simplifyed);
static uint16_t calculate_next_frequency(struct ModSendRequest *const sr,
        const struct ModProps props);

struct ModulationScheme mod_create(const uint16_t center_freq,
        const uint16_t bandwidth, const uint16_t symbol_length_ms,
        const uint16_t symbol_pause_ms, const bool simplifyed_symbols,
        const uint16_t task_period, void (*transmit_func)(const uint16_t))
{
    uint16_t low_freq, high_freq, symbol_len, symbol_pause, freq_slope;

    low_freq  = center_freq - bandwidth / 2;
    high_freq = center_freq + bandwidth / 2;
    divide_and_ceil(symbol_len, symbol_length_ms, task_period);
    divide_and_ceil(symbol_pause, symbol_pause_ms, task_period);
    divide_and_ceil(freq_slope, bandwidth, symbol_len);

    // Initialize the properties
    struct ModProps props = {
        .low_freq          = low_freq,
        .high_freq         = high_freq,
        .symbol_len        = symbol_len,
        .symbol_pause      = symbol_pause,
        .simplifyed_symbol = simplifyed_symbols,
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
    sr->counter = 0;
    // convert the byte to symbols
    conv_to_symbols(sr->msg, m, ms->props.simplifyed_symbol);

    // start transmission
    sr->is_transmitting = 1;

    // const enum kModSymbols current_symbol = sr->msg[sr->index];
    // if (current_symbol == LOW || current_symbol == UP) {
    //     sr->curr_freq = ms->props.low_freq;
    // } else {
    //     sr->curr_freq = ms->props.high_freq;
    // }

    sr->curr_freq = ms->props.low_freq;
    ms->props.transmit_func(sr->curr_freq);

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
    uint16_t new_freq;
    int ret = 0;
    struct ModSendRequest *const sr = &ms->_send_req;

    if (!sr->is_transmitting) {
        return -1;
    }

    // Tick the counter for how long the symbol was transmitted
    ++sr->counter;

    // Check if we are in cooldown time (the pause after the last symbol of a
    // transmission)
    // if (sr->index == get_num_symbols_per_byte(ms->props.simplifyed_symbol)) {
    //     if (sr->counter == 0) {
    //         sr->is_transmitting = 0;
    //     }

    //     return 0;
    // }

    // Check if we are still in the pause between symbols
    if (sr->counter < 0) {
        return 0;
    }

    // Calculate next frequency and go to the next symbol if needed
    new_freq = calculate_next_frequency(sr, ms->props);

    if (sr->counter >= ms->props.symbol_len) {
        new_freq = sr->curr_freq + 500;
        sr->counter = 0;
    } else {
        new_freq = sr->curr_freq;
    }

    if (new_freq > ms->props.high_freq) {
        new_freq = 0;
        sr->is_transmitting = 0;
    }

    // if (new_freq == 0) { // We finished a symbol
    //     ret = 1; // One symbol transmitted
    //     ++sr->index;
    //     sr->counter = -ms->props.symbol_pause;
    //     sr->curr_freq = 0;
    // }

    if (new_freq != sr->curr_freq) {
        sr->curr_freq = new_freq;
        ms->props.transmit_func(sr->curr_freq);
    }

    return ret;
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
    ms->props.symbol_pause      = 0;
    ms->props.simplifyed_symbol = 0;
    ms->props.freq_slope        = 0;

    return ret;
}

/**
 * Calculates how many symbols are used to represent a byte.
 *
 * @param[in] simplifyed_symbols If true only 2 symbols are used to represent
 * data (LOW and HIGH)
 *
 * @return The number of symbols used to represent a byte.
 */
static uint8_t get_num_symbols_per_byte(const bool simplifyed_symbols)
{
    if (simplifyed_symbols) {
        return 8;
    } else {
        return 4;
    }
}

/**
 * Function to convert a byte in symbols from enum kModSymbols. It has two
 * variants. The output can be simplifyed to use only LOW and HIGH symbols.
 *
 * @param[out] msg[8] Is an 8 element array where the symbols used to represent
 * the byte to be transmitted are returned. The order is from least significant
 * bit to the most significant.
 *
 * @param m The byte to be converted to the symbols.
 *
 * @param simplifyed If true, the convertor if to use only the LOW and HIGH
 * symbols, else will use all the symbols from kModSymbols.
 *
 * @note A m of value 0b11000110 in simplifyed mode will be equivalent to
 * msg[8] = {HIGH, HIGH, LOW, LOW, LOW, HIGH, HIGH, LOW}. A m of value
 * 0b11000110 in unsimplifyed mode will be equivalent to msg[8] = {DOWN, UP,
 * LOW, HIGH, *, *, *, *}.
 */
static void conv_to_symbols(enum kModSymbols msg[8], const uint8_t m, const bool simplifyed)
{
    static const enum kModSymbols SYMBOLS[] = {LOW, UP, HIGH, DOWN};
    int i, symbol_index;
    uint8_t m_mask, symbol_mask;
    uint8_t symbols_per_byte = get_num_symbols_per_byte(simplifyed);

    // use one symbol to represent a bit, symbols_per_byte = 8
    if (simplifyed) {
        for (i = 0; i < symbols_per_byte; ++i) {
            m_mask = 0x01 << i;

            if (m & m_mask) {
                msg[i] = HIGH;
            } else {
                msg[i] = LOW;
            }
        }
        return;
    }

    // use one symbol to represent 2 bits, symbols_per_byte = 4
    for (i = 0; i < symbols_per_byte; ++i) {
        // Check which symbol is for the current bit pair
        for (symbol_index = 0; symbol_index < sizeof(SYMBOLS); ++symbol_index) {
            m_mask = 0x03 << (2 * i);

            symbol_mask = SYMBOLS[symbol_index];
            symbol_mask = symbol_mask << (2 * i);

            if (((m & m_mask) ^ symbol_mask) == 0) {
                msg[i] = SYMBOLS[symbol_index];
                break;
            }

            if (symbol_index == 3) { // !!! SHOULD NOT REACH !!!
                msg[i] = LOW;
            }
        }
    }
}

/**
 * Modifies the internal state of struct ModSendRequest according to the symbol
 * being transmitted and at which iteration it is in the transmission. Is using
 * sr->counter as a counter for how many task ticks the transmission of the
 * symbol has run. Its value should be >= 0 when entering this funcion.
 *
 * @param sr[in] Represent the internal state of the ModulationScheme object
 * and is modifyed according to the symbol being transmitted.
 *
 * @param props[in] Is the configuration of the ModulationScheme object.
 *
 * @return The new frequency set in the sr->curr_freq field or 0 if the
 * transmission of the current symbol should end.
 *
 * @note When the transmission is on an UP or DOWN symbol, the last tick of
 * transmission can be outside the bound of prop.high_freq and prop.low_freq
 * respectively.
 */
static uint16_t calculate_next_frequency(struct ModSendRequest *const sr,
        const struct ModProps props)
{
    return 0;
    const enum kModSymbols symbol = sr->msg[sr->index];

    // return 0 if transmitted the symbol for long enough
    if (sr->counter == props.symbol_len) {
        sr->curr_freq = 0;
        return sr->curr_freq;
    }

    if (symbol == LOW) {
        sr->curr_freq = props.low_freq;
        return sr->curr_freq;
    } else if (symbol == HIGH) {
        sr->curr_freq = props.high_freq;
        return sr->curr_freq;
    } else if (symbol == UP) {
        sr->curr_freq = props.low_freq
                + (props.freq_slope * sr->counter);
        // !!! loosely bounded, can go over props.high_freq !!!
        return sr->curr_freq;
    } else if (symbol == DOWN) {
        sr->curr_freq = props.high_freq
                - (props.freq_slope * sr->counter);
        // !!! loosely bounded, can go under props.low_freq !!!
        return sr->curr_freq;
    }

    // !!! SHOULD NOT REACH !!!
    return 0;
}
