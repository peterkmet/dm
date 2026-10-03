#ifndef RANDOM_H
#define RANDOM_H
#include <stdint.h>

extern uint32_t g_random_seed;

void random_init(void);
uint16_t random16(void);
uint16_t random_bit(void);
uint16_t random_2bits(void);

#endif
