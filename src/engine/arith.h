/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_ENGINE_ARITH_H
#define DM_ENGINE_ARITH_H
/* Prefixes: min16 max16 abs16 bounded_ scale_ */

/* The 16-bit arithmetic of the original rules: minimum, maximum, bounds, and the scaled product of the 68000. */

#include <stdint.h>

/* The 68000 takes a shift count modulo 64; the mask limits the shift count of LSR. */
#define LSR_MASK 0x3f

/* A larger shift of a 32-bit value leaves only the sign or 0. */
#define LONG_SHIFT_MAXIMUM 31

/* The smaller of two values; b wins when they are equal. */
inline int16_t min16(int16_t a, int16_t b)
{
    return b <= a ? b : a;
}

/* The larger of two values; b wins when they are equal. */
inline int16_t max16(int16_t a, int16_t b)
{
    return a <= b ? b : a;
}

/* The absolute value of a 16-bit value. */
inline int16_t abs16(int16_t number)
{
    return number < 0 ? -number : number;
}

/* number limited to min..max; min wins when min > max. */
inline int16_t bounded_value(int16_t min, int16_t number, int16_t max)
{
    return number <= min ? min : number > max ? max : number;
}

/* amount * factor >> shift. Like the original asr.l of the 32-bit mulu product, the sign bit is the
   product's bit 31 and the shift count is taken modulo 64. */
inline int32_t scale_value(uint16_t amount, int16_t shift, uint16_t factor)
{
    int32_t product = (int32_t)((uint32_t)amount * factor); /* unsigned multiply: no signed overflow */
    shift &= LSR_MASK;
    return shift > LONG_SHIFT_MAXIMUM ? (product < 0 ? -1 : 0) : product >> shift;
}

#endif
