#include "random.h"
#include <time.h>

#define RANDOM_MULTIPLIER           0xBB40E62Du
#define RANDOM_INCREMENT            11
#define NANOSECONDS_PER_MICROSECOND 1000
#define RANDOM_VALUE_SHIFT          8 /* a value is bits 8-23 of the seed */
#define RANDOM_2BITS_MASK           3

uint32_t g_random_seed;

/* Seeds with the microseconds of the current time, like system_init with IntuitionBase->Micros. */
void random_init(void)
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    g_random_seed = (uint32_t)(ts.tv_nsec / NANOSECONDS_PER_MICROSECOND);
}

/* Next value of the linear congruential generator: bits 8-23 of the new seed. */
static uint16_t next(void)
{
    g_random_seed = g_random_seed * RANDOM_MULTIPLIER + RANDOM_INCREMENT;
    return (uint16_t)(g_random_seed >> RANDOM_VALUE_SHIFT);
}

uint16_t random16(void)
{
    return next();
}
uint16_t random_bit(void)
{
    return next() & 1;
}
uint16_t random_2bits(void)
{
    return next() & RANDOM_2BITS_MASK;
}
