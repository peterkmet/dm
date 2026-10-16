/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "data/spells.h"
#include "data/bytes.h"
#include "data/tables.h"
#include <limits.h>

#define SPELL_FIRST_SYMBOL_SHIFT 24  /* the first symbol is in bits 24-31 of the symbols */
#define LSR_COUNT_MODULO         64  /* the 68000 LSR takes the shift count modulo 64 */
#define SPELL_WISDOM_BONUS       15  /* added to wisdom for the failure draw */
#define SPELL_WISDOM_LIMIT       115 /* the failure draw limit never goes above this */
#define SPELL_DRAW_RANGE         128

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
    const Spell *spell;
    for (int16_t i = 0; (spell = tables_get_spell(i)); i++)
        /* a spell without a power symbol matches any power */
        if ((spell->symbols & SPELL_POWER_SYMBOL_MASK) ? packed == spell->symbols
                                                       : (packed & ~SPELL_POWER_SYMBOL_MASK) == spell->symbols)
            return i;
    return -1;
}

uint8_t spell_get_symbol(uint16_t step, int16_t symbol_index)
{
    return SPELL_SYMBOL_FIRST + step * SPELL_SYMBOLS_PER_STEP + symbol_index;
}

uint16_t spell_get_symbol_mana_cost(uint16_t step, int16_t symbol_index, uint8_t power_symbol)
{
    uint16_t cost = tables_get_symbol_base_mana_cost(step, symbol_index);
    if (step) /* 16-bit product as in the original */
        cost = (uint16_t)(cost * tables_get_symbol_mana_cost_multiplier(power_symbol - SPELL_SYMBOL_FIRST)) >>
               SPELL_MANA_MULTIPLIER_SHIFT;
    return cost;
}

bool spell_draw_fails(uint8_t current_wisdom, uint16_t random)
{
    uint16_t limit = current_wisdom + SPELL_WISDOM_BONUS < SPELL_WISDOM_LIMIT ? current_wisdom + SPELL_WISDOM_BONUS
                                                                              : SPELL_WISDOM_LIMIT;
    return limit < random % SPELL_DRAW_RANGE;
}

uint16_t spell_get_failure_experience(uint16_t experience, uint16_t required_level, uint16_t skill_level)
{
    uint16_t n = (uint8_t)(required_level - skill_level) % LSR_COUNT_MODULO;
    return n < WORD_BITS ? experience >> n : 0;
}
