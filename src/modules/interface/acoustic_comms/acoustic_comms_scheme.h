/**
 * Acoustic communication scheme interface.
*/

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Create it and initialize
 *
 * Modify the parameters on any demand
 *
 * Is transmission happening
 *
 * Get number of bytes remaining to be transmitted (including any currently transmitted byte)
 *
 * Transmit
 *
 * Finished transmission callback
 *
 * Update internal state (will be called with a periodicity)
 *
 * Stop transmission (forcefull and clears the internal queue)
 *
 * Destroy it
*/
struct AcousticModulationScheme {

    int32_t period_ms;

    bool use_precise_scheduling;

// pointer to initialization function
    /**
     * Initialization function for the modulator
    */
    int32_t (*create)(const void*);

// Modify the parameters on any demand
    int32_t (*configure)(const void*);

// Get number of bytes remaining rounded up
    int32_t (*get_queue_lenght)(void);

// Transmit
    int32_t (*tx)(const void*, const uint32_t);

// Transmitted a byte callback
    void (*byte_sent_callback)(void);

// Update internal state (will be called with a periodicity)
    int32_t (*_update)(void);

// Stop transmission (forcefull and clears the internal queue)
    int32_t (*stop)(void);

// Destroy it
    int32_t (*destroy)(void);
};

/**
 * Public functions to be used by the user space
*/
int32_t acousticCommsInit(const struct AcousticModulationScheme scheme, const bool use_precise_timming);

int32_t acousticCommsUpdateScheme(const struct AcousticModulationScheme scheme);

struct AcousticModulationScheme acousticCommsGetScheme(void);


int32_t acousticCommsGetQueueSize(void);

int32_t acousticCommsSend(const void* buf, const uint32_t len);

int32_t acousticCommsUpdateCallback(void (*byte_sent_callback)(void));

int32_t acousticCommsStop(void);

int32_t acousticCommsDeinit(void);
