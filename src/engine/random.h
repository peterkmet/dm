/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_ENGINE_RANDOM_H
#define DM_ENGINE_RANDOM_H
/* Prefixes: random_ random16 */
#include <stdint.h>

extern uint32_t g_random_seed;
/* The state of the game random numbers, to put back after drawing that used them. */
uint32_t random_get_seed(void);
void random_set_seed(uint32_t seed);

/* Seeds with the microseconds of the current time, like system_init with IntuitionBase->Micros. */
void random_init(void);
uint16_t random16(void);
uint16_t random_bit(void);
uint16_t random_2bits(void);

#endif
