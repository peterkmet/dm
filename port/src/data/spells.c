#include <limits.h>
#include "spells.h"
#include "adventure.h"
#include "bytes.h"
#include "csbimage.h"
#include "ftl.h"
#include "kaos_addresses.h"

#define SPELL_FIRST_SYMBOL_SHIFT 24  /* the first symbol is in bits 24-31 of the symbols */
#define LSR_COUNT_MODULO         64  /* the 68000 LSR takes the shift count modulo 64 */
#define SPELL_WISDOM_BONUS       15  /* added to wisdom for the failure draw */
#define SPELL_WISDOM_LIMIT       115 /* the failure draw limit never goes above this */
#define SPELL_DRAW_RANGE         128

/* The spell table of the engine: the CSB table with its 4 magic map spells lies after the KAOS data. */
static uint32_t spells_address(void)
{
    return g_adventure == ADVENTURE_CSB ? CSB_SPELLS : KAOS_SPELLS;
}
int16_t spell_count(void)
{
    return g_adventure == ADVENTURE_CSB ? CSB_SPELL_COUNT : DM_SPELL_COUNT;
}

/* Power of a power symbol, from 0 for the lowest. The original reads the symbol as signed. */
static int16_t power_index(uint8_t power_symbol)
{
    return (int8_t)power_symbol - SPELL_SYMBOL_FIRST;
}

/* Spell index for a NUL-terminated symbol string (power symbol first), or -1. One symbol is never a spell. */
int16_t spell_find(const uint8_t *symbols)
{
    if (symbols[1] == 0)
        return -1;
    uint32_t packed = 0;
    for (int shift = SPELL_FIRST_SYMBOL_SHIFT; shift >= 0; shift -= CHAR_BIT, symbols++) {
        packed |= (uint32_t)*symbols << shift;
        if (symbols[1] == 0)
            break;
    }
    for (int16_t i = 0; i < spell_count(); i++) {
        uint32_t s = kaos_u32(spells_address() + (uint32_t)i * SPELL_BYTES);
        /* a spell without a power symbol matches any power */
        if ((s & SPELL_POWER_SYMBOL_MASK) ? packed == s : (packed & ~SPELL_POWER_SYMBOL_MASK) == s)
            return i;
    }
    return -1;
}

/* Symbol character for button symbol_index (0-5) at a step (0-3). */
uint8_t spell_symbol(uint16_t step, int16_t symbol_index)
{
    return SPELL_SYMBOL_FIRST + step * SPELL_SYMBOLS_PER_STEP + symbol_index;
}

/* Mana cost of a symbol; after the power symbol, the cost is scaled by the power. */
uint16_t spell_symbol_mana_cost(uint16_t step, int16_t symbol_index, uint8_t power_symbol)
{
    uint16_t cost = kaos_u8(KAOS_SYMBOL_BASE_MANA_COST + step * SPELL_SYMBOLS_PER_STEP + symbol_index);
    if (step != 0)
        cost =
            cost * kaos_u8(KAOS_SYMBOL_MANA_COST_MULTIPLIER + power_index(power_symbol)) >> SPELL_MANA_MULTIPLIER_SHIFT;
    return cost;
}

/* One draw per missing skill level; the cast fails on the first draw that returns 1. */
bool spell_draw_fails(uint8_t current_wisdom, uint16_t random)
{
    uint16_t limit = current_wisdom + SPELL_WISDOM_BONUS < SPELL_WISDOM_LIMIT ? current_wisdom + SPELL_WISDOM_BONUS
                                                                              : SPELL_WISDOM_LIMIT;
    return limit < random % SPELL_DRAW_RANGE;
}

/* Experience on failure: shifted right once per missing level (68000 LSR.W, count modulo 64). */
uint16_t spell_failure_experience(uint16_t experience, uint16_t required_level, uint16_t skill_level)
{
    uint16_t n = (uint8_t)(required_level - skill_level) % LSR_COUNT_MODULO;
    return n < WORD_BITS ? experience >> n : 0;
}
