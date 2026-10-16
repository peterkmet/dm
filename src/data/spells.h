/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_DATA_SPELLS_H
#define DM_DATA_SPELLS_H
/* Prefixes: spell_ */
#include <stdbool.h>
#include <stdint.h>

#define SPELL_BYTES            8
#define SPELL_ENTRY_LEVEL      4 /* byte offsets in an entry: base required level, skill, attributes */
#define SPELL_ENTRY_SKILL      5
#define SPELL_ENTRY_ATTRIBUTES 6

#define DM_SPELL_COUNT                25   /* CSB has CSB_SPELL_COUNT */
#define SPELL_SYMBOL_FIRST            0x60 /* font character of the first symbol: power 1 at step 0 */
#define SPELL_SYMBOLS_PER_STEP        6
#define SPELL_MANA_MULTIPLIER_SHIFT   3 /* the mana cost multiplier of the power symbol counts eighths */
#define SPELL_POWER_SYMBOL_MASK       0xff000000u
#define SPELL_KIND_MASK               0xf
#define SPELL_TYPE_SHIFT              4
#define SPELL_TYPE_MASK               0x3f
#define SPELL_DISABLED_TICKS_SHIFT    10
/* Experience of a cast: level² + (power - 1) * base level * 8 + level * 16 + random(8). */
#define SPELL_EXPERIENCE_POWER_FACTOR 8
#define SPELL_EXPERIENCE_LEVEL_FACTOR 16
#define SPELL_EXPERIENCE_RANDOM_MASK  0x7
#define SPELL_KIND_POTION             1
#define SPELL_KIND_PROJECTILE         2
#define SPELL_KIND_OTHER              3
#define SPELL_KIND_MAGIC_MAP          4

/* The fields of the attributes word of a spell entry. */
static inline uint16_t spell_attributes_kind(uint16_t attributes)
{
    return attributes & SPELL_KIND_MASK;
}
static inline uint16_t spell_attributes_type(uint16_t attributes)
{
    return attributes >> SPELL_TYPE_SHIFT & SPELL_TYPE_MASK;
}
static inline uint16_t spell_attributes_disabled_ticks(uint16_t attributes)
{
    return attributes >> SPELL_DISABLED_TICKS_SHIFT;
}

int16_t spell_count(void);
int16_t spell_find(const uint8_t *symbols);
uint8_t spell_get_symbol(uint16_t step, int16_t symbol_index);
uint16_t spell_get_symbol_mana_cost(uint16_t step, int16_t symbol_index, uint8_t power_symbol);
bool spell_draw_fails(uint8_t current_wisdom, uint16_t random);
uint16_t spell_get_failure_experience(uint16_t experience, uint16_t required_level, uint16_t skill_level);

#endif
