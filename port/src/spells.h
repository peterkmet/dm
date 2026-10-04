#ifndef SPELLS_H
#define SPELLS_H
#include <stdint.h>

#define DM_SPELL_COUNT 25 /* CSB has CSB_SPELL_COUNT */
#define SPELL_SYMBOL_FIRST 0x60       /* font character of the first symbol: power 1 at step 0 */
#define SPELL_SYMBOLS_PER_STEP 6
#define SPELL_POWER_SYMBOL_MASK 0xff000000u
#define SPELL_KIND_MASK 15
#define SPELL_TYPE_SHIFT 4
#define SPELL_TYPE_MASK 63
#define SPELL_DISABLED_TICKS_SHIFT 10
enum { SPELL_KIND_POTION = 1, SPELL_KIND_PROJECTILE = 2, SPELL_KIND_OTHER = 3 };

/* One entry of g_spells. */
typedef struct {
    uint32_t symbols;  /* first symbol in bits 24-31; 0 there means the spell has no power symbol */
    uint8_t base_required_level;
    uint8_t skill_index;
    uint16_t attributes;  /* bits 0-3 kind, 4-9 type, 10-15 action disabled ticks */
} Spell;

int16_t spell_count(void);
Spell spell_get(int16_t index);
uint16_t spell_kind(const Spell *spell);
uint16_t spell_type(const Spell *spell);
uint16_t spell_disabled_ticks(const Spell *spell);
int16_t spell_find(const uint8_t *symbols);
uint8_t spell_symbol(uint16_t step, int16_t symbol_index);
uint16_t spell_symbol_mana_cost(uint16_t step, int16_t symbol_index, uint8_t power_symbol);
uint16_t spell_required_level(uint8_t power_symbol, const Spell *spell);
uint16_t spell_experience(uint8_t power_symbol, const Spell *spell, uint16_t random);
int16_t spell_draw_fails(uint8_t current_wisdom, uint16_t random);
uint16_t spell_failure_experience(uint16_t experience, uint16_t required_level, uint16_t skill_level);

#endif
