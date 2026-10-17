/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_ENGINE_MAGIC_H
#define DM_ENGINE_MAGIC_H
/* Prefixes: magic_ */

/* Spell casting: the effect of a spell from the champion symbols (KAOS range 8, MENU.C): docs/rules/creatures.md. */

#include "data/bytes.h"
#include "engine/map.h"
#include "engine/party.h"
#include <stdbool.h>
#include <stdint.h>

/* Champion.magic_map: the DAIN, ROS, and KU spell counts; NETA holds direction + 1 of the kept map; the
   kept position is a big-endian word with x in bits 11-15, y in bits 6-10, and the map in bits 0-5. */
#define MAGIC_MAP_DAIN     0
#define MAGIC_MAP_ROS      1
#define MAGIC_MAP_KU       2
#define MAGIC_MAP_NETA     3
#define MAGIC_MAP_POSITION 4
/* Stores the kept map position of NETA in the magic map bytes. */
static inline void magic_set_map_position(uint8_t *spells, Position position)
{
    wr16(spells + MAGIC_MAP_POSITION, party_pack_position(position.map, position.x, position.y));
}

/* Reads the kept map position of NETA from the magic map bytes. */
static inline Position magic_get_map_position(const uint8_t *spells)
{
    uint16_t word = be16(spells + MAGIC_MAP_POSITION);
    return (Position){word & PARTY_POSITION_MAP_MASK, party_position_x(word), party_position_y(word)};
}

/* Shield defense above which a new shield adds only a quarter. */
#define SHIELD_STRONG 50 /* above this shield defense, a new shield or a Ya potion adds a quarter */

/* Results of magic_cast_champion_spell. */
#define SPELL_CAST_FAILURE             0
#define SPELL_CAST_SUCCESS             1
#define SPELL_CAST_FAILURE_NEEDS_FLASK 3

/* Failure messages of magic_print_spell_failure. */
#define SPELL_FAILURE_NEEDS_MORE_PRACTICE 0
#define SPELL_FAILURE_MEANINGLESS_SPELL   1
#define SPELL_FAILURE_NEEDS_FLASK_IN_HAND 10
#define SPELL_FAILURE_NEEDS_MAGIC_MAP     11

/* Other spell types (spell kind 3). */
#define SPELL_OTHER_LIGHT        0
#define SPELL_OTHER_DARKNESS     1
#define SPELL_OTHER_THIEVES_EYE  2
#define SPELL_OTHER_INVISIBILITY 3
#define SPELL_OTHER_PARTY_SHIELD 4
#define SPELL_OTHER_MAGIC_TORCH  5
#define SPELL_OTHER_FOOTPRINTS   6
#define SPELL_OTHER_ZOKATHRA     7
#define SPELL_OTHER_FIRE_SHIELD  8

/* Casts the spell of the champion symbols. Each missing skill level is a chance to fail against wisdom. */
int16_t magic_cast_champion_spell(int16_t champion_index); /* exported for the tests */
/* Cast button: casts for the magic caster; the symbols clear unless a flask is missing. */
int16_t magic_click_cast_spell(void);
/* Prints "<text> <champion name> <text>" for a failed cast. (exported for the tests) */
void magic_print_spell_failure(int16_t champion_index, int16_t failure_type, int16_t skill_index);
/* Spends the mana and shoots the spell; a weak cast with little maximum mana gets a slower step energy. */
bool magic_is_projectile_spell_cast(int16_t champion_index, uint16_t thing, int16_t kinetic_energy,
                                    uint16_t required_mana);
/* Spell shield (EVENT_SPELL_SHIELD_END) or fire shield (EVENT_FIRE_SHIELD_END) for the ticks. With mana, it costs 4
   and half the ticks when short. */
bool magic_cast_party_spell(int16_t champion_index, bool spell_shield, uint16_t ticks, bool use_mana);
/* Light comes now; EVENT_LIGHT takes it back step by step after the ticks. */
void magic_create_light_event(int16_t light_power, uint16_t ticks);
/* The action hand is disabled for the ticks; an earlier disable is merged into one EVENT_ENABLE_CHAMPION_ACTION. */
void magic_disable_action(int16_t champion_index, uint16_t ticks);
/* The magic caster adds a rune and pays its mana. Returns false when the mana is short. */
bool magic_add_champion_symbol(int16_t symbol);
/* The magic caster removes the last rune; the mana stays spent. Returns false when it has no rune. */
bool magic_delete_champion_symbol(void);
/* Selects a living champion as the spell caster; -1 clears it. Returns true when the caster changed. */
bool magic_set_caster(int16_t champion_index);
/* CSB: true when a scroll text (or its first line) is the magic map text MAGICMAP. */
bool magic_map_is_text(const char *text);

#endif
