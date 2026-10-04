#include "spells.h"
#include "ftl.h"

#define G_SYMBOL_BASE_MANA_COST 0x103c66       /* uint8_t[4][6] */
#define G_SYMBOL_MANA_COST_MULTIPLIER 0x103c7e /* uint8_t[6], by power symbol */
#define G_SPELLS 0x103c84                      /* Spell[25], 8 bytes each */

#define SPELL_ENTRY_SIZE 8
#define SPELL_WISDOM_BONUS 15      /* added to wisdom for the failure draw */
#define SPELL_WISDOM_LIMIT 115     /* the failure draw limit never goes above this */
#define SPELL_DRAW_RANGE 128

/* Spell table entry with its symbols, required level, skill and attributes. */
Spell spell_get(int16_t index) {
    uint32_t a = G_SPELLS + (uint32_t)index * SPELL_ENTRY_SIZE;
    return (Spell){kaos_u32(a), kaos_u8(a + 4), kaos_u8(a + 5), kaos_u16(a + 6)};
}

uint16_t spell_kind(const Spell *spell) { return spell->attributes & SPELL_KIND_MASK; }
uint16_t spell_type(const Spell *spell) { return spell->attributes >> SPELL_TYPE_SHIFT & SPELL_TYPE_MASK; }
uint16_t spell_disabled_ticks(const Spell *spell) { return spell->attributes >> SPELL_DISABLED_TICKS_SHIFT; }

/* Power of a power symbol, from 0 for the lowest. The original reads the symbol as signed. */
static int16_t power_index(uint8_t power_symbol) { return (int8_t)power_symbol - SPELL_SYMBOL_FIRST; }

/* Spell index for a NUL-terminated symbol string (power symbol first), or -1. One symbol is never a spell. */
int16_t spell_find(const uint8_t *symbols) {
    if (symbols[1] == 0)
        return -1;
    uint32_t packed = 0;
    for (int shift = 24; shift >= 0; shift -= 8, symbols++) {
        packed |= (uint32_t)*symbols << shift;
        if (symbols[1] == 0)
            break;
    }
    for (int16_t i = 0; i < SPELL_COUNT; i++) {
        uint32_t s = kaos_u32(G_SPELLS + (uint32_t)i * SPELL_ENTRY_SIZE);
        /* a spell without a power symbol matches any power */
        if ((s & SPELL_POWER_SYMBOL_MASK) ? packed == s : (packed & ~SPELL_POWER_SYMBOL_MASK) == s)
            return i;
    }
    return -1;
}

/* Symbol character for button symbol_index (0-5) at a step (0-3). */
uint8_t spell_symbol(uint16_t step, int16_t symbol_index) {
    return (uint8_t)(SPELL_SYMBOL_FIRST + step * SPELL_SYMBOLS_PER_STEP + symbol_index);
}

/* Mana cost of a symbol; after the power symbol, the cost is scaled by the power. */
uint16_t spell_symbol_mana_cost(uint16_t step, int16_t symbol_index, uint8_t power_symbol) {
    uint16_t cost = kaos_u8(G_SYMBOL_BASE_MANA_COST + step * SPELL_SYMBOLS_PER_STEP + symbol_index);
    if (step != 0)
        cost = (uint16_t)(cost * kaos_u8(G_SYMBOL_MANA_COST_MULTIPLIER + power_index(power_symbol))) >> 3;
    return cost;
}

/* Skill level that the spell needs at this power. */
uint16_t spell_required_level(uint8_t power_symbol, const Spell *spell) {
    return (uint16_t)(power_index(power_symbol) + 1 + spell->base_required_level);
}

/* ponytail: the original squares a stale D3 from the caller; this uses required_level² as ReDMCSB intends. */
uint16_t spell_experience(uint8_t power_symbol, const Spell *spell, uint16_t random) {
    uint16_t level = spell_required_level(power_symbol, spell);
    return (uint16_t)(level * level + power_index(power_symbol) * spell->base_required_level * 8 + level * 16 +
                      (random & 7));
}

/* One draw per missing skill level; the cast fails on the first draw that returns 1. */
int16_t spell_draw_fails(uint8_t current_wisdom, uint16_t random) {
    uint16_t limit = current_wisdom + SPELL_WISDOM_BONUS < SPELL_WISDOM_LIMIT ? current_wisdom + SPELL_WISDOM_BONUS : SPELL_WISDOM_LIMIT;
    return limit < random % SPELL_DRAW_RANGE;
}

/* Experience on failure: shifted right once per missing level (68000 LSR.W, count modulo 64). */
uint16_t spell_failure_experience(uint16_t experience, uint16_t required_level, uint16_t skill_level) {
    uint16_t n = (uint8_t)(required_level - skill_level) % 64;
    return n < 16 ? experience >> n : 0;
}
