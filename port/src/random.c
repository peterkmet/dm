#include "random.h"
#include <time.h>

#define RANDOM_MULTIPLIER 0xBB40E62Du
#define RANDOM_INCREMENT 11

uint32_t g_random_seed;

/* Seeds with the microseconds of the current time, like system_init with IntuitionBase->Micros. */
void random_init(void)
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    g_random_seed = (uint32_t)(ts.tv_nsec / 1000);
}

/* Next value of the linear congruential generator: bits 8-23 of the new seed. */
static uint16_t next(void)
{
    g_random_seed = g_random_seed * RANDOM_MULTIPLIER + RANDOM_INCREMENT;
    return (uint16_t)(g_random_seed >> 8);
}

uint16_t random16(void) { return next(); }
uint16_t random_bit(void) { return next() & 1; }
uint16_t random_2bits(void) { return next() & 3; }
