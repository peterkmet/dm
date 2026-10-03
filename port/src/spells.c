#include "spells.h"
#include "ftl.h"

#define G_SYMBOL_BASE_MANA_COST 0x103c66       /* uint8_t[4][6] */
#define G_SYMBOL_MANA_COST_MULTIPLIER 0x103c7e /* uint8_t[6], by power symbol */
#define G_SPELLS 0x103c84                      /* Spell[25], 8 bytes each */

Spell spell_get(int16_t index) {
    uint32_t a = G_SPELLS + (uint32_t)index * 8;
    return (Spell){kaos_u32(a), kaos_u8(a + 4), kaos_u8(a + 5), kaos_u16(a + 6)};
}

uint16_t spell_kind(const Spell *spell) { return spell->attributes & 0xf; }
uint16_t spell_type(const Spell *spell) { return spell->attributes >> 4 & 0x3f; }
uint16_t spell_disabled_ticks(const Spell *spell) { return spell->attributes >> 10; }

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
        uint32_t s = kaos_u32(G_SPELLS + (uint32_t)i * 8);
        if ((s & 0xff000000) ? packed == s : (packed & 0xffffff) == s)
            return i;
    }
    return -1;
}

/* Symbol character for button symbol_index (0-5) at a step (0-3). */
uint8_t spell_symbol(uint16_t step, int16_t symbol_index) { return (uint8_t)(0x60 + step * 6 + symbol_index); }

uint16_t spell_symbol_mana_cost(uint16_t step, int16_t symbol_index, uint8_t power_symbol) {
    uint16_t cost = kaos_u8(G_SYMBOL_BASE_MANA_COST + step * 6 + symbol_index);
    if (step != 0)
        cost = (uint16_t)(cost * kaos_u8(G_SYMBOL_MANA_COST_MULTIPLIER + (int8_t)power_symbol - 0x60)) >> 3;
    return cost;
}

uint16_t spell_required_level(uint8_t power_symbol, const Spell *spell) {
    return (uint16_t)((int8_t)power_symbol - 0x5f + spell->base_required_level);
}

/* ponytail: the original squares a stale D3 from the caller; this uses required_level² as ReDMCSB intends. */
uint16_t spell_experience(uint8_t power_symbol, const Spell *spell, uint16_t random) {
    uint16_t level = spell_required_level(power_symbol, spell);
    return (uint16_t)(level * level + ((int8_t)power_symbol - 0x60) * spell->base_required_level * 8 + level * 16 +
                      (random & 7));
}

/* One draw per missing skill level; the cast fails on the first draw that returns 1. */
int16_t spell_draw_fails(uint8_t current_wisdom, uint16_t random) {
    uint16_t limit = current_wisdom + 15 < 115 ? current_wisdom + 15 : 115;
    return limit < (random & 0x7f);
}

/* Experience on failure: shifted right once per missing level (68000 LSR.W, count modulo 64). */
uint16_t spell_failure_experience(uint16_t experience, uint16_t required_level, uint16_t skill_level) {
    uint16_t n = (uint8_t)(required_level - skill_level) & 0x3f;
    return n < 16 ? experience >> n : 0;
}
