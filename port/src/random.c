#include "random.h"
#include <time.h>

uint32_t g_random_seed;

/* system_init seeds with IntuitionBase->Micros (offset 0x4c): microseconds of the current time. */
void random_init(void)
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    g_random_seed = (uint32_t)(ts.tv_nsec / 1000);
}

static uint16_t next(void)
{
    g_random_seed = g_random_seed * 0xBB40E62Du + 11;
    return (uint16_t)(g_random_seed >> 8);
}

uint16_t random16(void) { return next(); }
uint16_t random_bit(void) { return next() & 1; }
uint16_t random_2bits(void) { return next() & 3; }
