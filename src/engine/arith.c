/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "engine/arith.h"

extern inline int16_t min16(int16_t a, int16_t b);
extern inline int16_t max16(int16_t a, int16_t b);
extern inline int16_t abs16(int16_t value);
extern inline int16_t bounded_value(int16_t min, int16_t value, int16_t max);
extern inline int32_t scale_value(uint16_t a, int16_t shift, uint16_t b);
