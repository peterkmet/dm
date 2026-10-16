/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_ENGINE_MAGIC_H
#define DM_ENGINE_MAGIC_H
/* Prefixes: magic_ */
#include <stdbool.h>
#include <stdint.h>

/* Spell casting: the effect of a spell from the champion symbols (KAOS range 8, MENU.C): docs/rules/creatures.md. */

#define SHIELD_STRONG 50 /* above this shield defense, a new shield or a Ya potion adds a quarter */

/* Results of magic_get_champion_spell_cast_result. */
#define SPELL_CAST_FAILURE             0
#define SPELL_CAST_SUCCESS             1
#define SPELL_CAST_FAILURE_NEEDS_FLASK 3

/* Failure messages of magic_print_spell_failure_message. */
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
int16_t magic_get_champion_spell_cast_result(uint16_t champion_index);
/* Cast button: casts for the magic caster; the symbols clear unless a flask is missing. */
int16_t magic_get_click_on_spell_cast_result(void);
/* Prints "<text> <champion name> <text>" for a failed cast. */
void magic_print_spell_failure_message(uint16_t champion_index, int16_t failure_type, uint16_t skill_index);
/* Spends the mana and shoots the spell; a weak cast with little maximum mana gets a slower step energy. */
bool magic_is_projectile_spell_cast(uint16_t champion_index, uint16_t thing, int16_t kinetic_energy,
                                    uint16_t required_mana);
/* Spell shield (event 77) or fire shield (event 78) for the ticks. With mana, it costs 4 and half the ticks when short. */
bool magic_is_party_spell_or_fire_shield_successful(uint16_t champion_index, bool spell_shield, uint16_t ticks,
                                                    bool use_mana);
/* Light comes now; event 70 takes it back step by step after the ticks. */
void magic_create_light_event(int16_t light_power, int16_t ticks);
/* The action hand is disabled for the ticks; an earlier disable is merged into one event 11. */
void magic_disable_action(uint16_t champion_index, uint16_t ticks);
/* The magic caster adds a rune and pays its mana. Returns false when the mana is short. */
bool magic_add_champion_symbol(int16_t symbol);
/* The magic caster removes the last rune; the mana stays spent. Returns false when it has no rune. */
bool magic_delete_champion_symbol(void);
/* Selects a living champion as the spell caster; -1 clears it. Returns true when the caster changed. */
bool magic_set_caster(int16_t champion_index);
/* CSB: true when a scroll text (or its first line) is the magic map text MAGICMAP. */
bool magic_map_is_text(const char *text);

#endif
