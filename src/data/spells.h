/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_DATA_SPELLS_H
#define DM_DATA_SPELLS_H
/* Prefixes: spell_ */
#include <stdbool.h>
#include <stdint.h>

#define DM_SPELL_COUNT                25   /* CSB has CSB_SPELL_COUNT */
#define SPELL_SYMBOL_FIRST            0x60 /* font character of the first symbol: power 1 at step 0 */
#define SPELL_SYMBOLS_PER_STEP        6
#define SPELL_MANA_MULTIPLIER_SHIFT   3 /* the mana cost multiplier of the power symbol counts eighths */
#define SPELL_POWER_SYMBOL_MASK       0xff000000u
/* Experience of a cast: level² + (power - 1) * base level * 8 + level * 16 + random(8). */
#define SPELL_EXPERIENCE_POWER_FACTOR 8
#define SPELL_EXPERIENCE_LEVEL_FACTOR 16
#define SPELL_EXPERIENCE_RANDOM_MASK  0x7
#define SPELL_KIND_POTION             1
#define SPELL_KIND_PROJECTILE         2
#define SPELL_KIND_OTHER              3
#define SPELL_KIND_MAGIC_MAP          4

/* Spell index for a NUL-terminated symbol string (power symbol first), or -1. One symbol is never a spell. */
int16_t spell_find(const uint8_t *symbols);
/* Symbol character for button symbol_index (0-5) at a step (0-3). */
uint8_t spell_get_symbol(uint16_t step, int16_t symbol_index);
/* Mana cost of a symbol; after the power symbol, the cost is scaled by the power. */
uint16_t spell_get_symbol_mana_cost(uint16_t step, int16_t symbol_index, uint8_t power_symbol);
/* One draw per missing skill level; the cast fails on the first draw that returns true. */
bool spell_draw_fails(uint8_t current_wisdom, uint16_t random);
/* Experience on failure: shifted right once per missing level (68000 LSR.W, count modulo 64). */
uint16_t spell_get_failure_experience(uint16_t experience, uint16_t required_level, uint16_t skill_level);

#endif
