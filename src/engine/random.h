/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_ENGINE_RANDOM_H
#define DM_ENGINE_RANDOM_H
/* Prefixes: random_ random16 */

/* The game random numbers: the seed and the random draws. */

#include <stdint.h>

extern uint32_t g_random_seed;
/* The state of the game random numbers, to put back after drawing that used them. */
uint32_t random_get_seed(void);
/* Puts back a state that random_get_seed returned. */
void random_set_seed(uint32_t seed);

/* Seeds with the microseconds of the current time, like system_init with IntuitionBase->Micros. */
void random_init(void);
/* The next random 16-bit value. */
uint16_t random16(void);
/* A random value, 0 or 1. */
uint16_t random_bit(void);
/* A random value, 0-3. */
uint16_t random_2bits(void);

#endif
