#ifndef MAGIC_H
#define MAGIC_H
#include <stdint.h>

/* Spell casting: the effect of a spell from the champion symbols (KAOS range 8, MENU.C): docs/rules-creatures.md. */

/* Results of menus_get_champion_spell_cast_result. */
enum { SPELL_CAST_FAILURE = 0, SPELL_CAST_SUCCESS = 1, SPELL_CAST_FAILURE_NEEDS_FLASK = 3 };

/* Failure messages of menus_print_spell_failure_message. */
enum { SPELL_FAILURE_NEEDS_MORE_PRACTICE = 0, SPELL_FAILURE_MEANINGLESS_SPELL = 1, SPELL_FAILURE_NEEDS_FLASK_IN_HAND = 10 };

/* Other spell types (spell kind 3). */
enum {
    SPELL_OTHER_LIGHT = 0, SPELL_OTHER_DARKNESS = 1, SPELL_OTHER_THIEVES_EYE = 2, SPELL_OTHER_INVISIBILITY = 3,
    SPELL_OTHER_PARTY_SHIELD = 4, SPELL_OTHER_MAGIC_TORCH = 5, SPELL_OTHER_FOOTPRINTS = 6, SPELL_OTHER_ZOKATHRA = 7,
    SPELL_OTHER_FIRE_SHIELD = 8
};

int16_t menus_get_champion_spell_cast_result(uint16_t champion_index);
int16_t menus_get_click_on_spell_cast_result(void);
void menus_print_spell_failure_message(uint16_t champion_index, int16_t failure_type, uint16_t skill_index);
int16_t champion_is_projectile_spell_cast(uint16_t champion_index, uint16_t thing, int16_t kinetic_energy, uint16_t required_mana);
int16_t menus_is_party_spell_or_fire_shield_successful(uint16_t champion_index, int16_t spell_shield, uint16_t ticks, int16_t use_mana);
void menus_create_event70_light(int16_t light_power, int16_t ticks);
void champion_disable_action(uint16_t champion_index, uint16_t ticks);

#endif
