#ifndef ACTION_H
#define ACTION_H
/* Prefixes: action_ */
#include <stdbool.h>
#include <stdint.h>

/* Champion actions of the action area (KAOS range 8, MENU.C): docs/rules-champions.md#actions. */

/* Action indices (graphic 699 holds the names). Actions without an entry here only train the skill and add the
   action defense. */
enum {
    ACTION_N = 0,
    ACTION_BLOCK = 1,
    ACTION_CHOP = 2,
    ACTION_BLOW_HORN = 4,
    ACTION_FLIP = 5,
    ACTION_PUNCH = 6,
    ACTION_KICK = 7,
    ACTION_WAR_CRY = 8,
    ACTION_STAB_1 = 9,
    ACTION_CLIMB_DOWN = 10,
    ACTION_FREEZE_LIFE = 11,
    ACTION_HIT = 12,
    ACTION_SWING = 13,
    ACTION_STAB_2 = 14,
    ACTION_THRUST = 15,
    ACTION_JAB = 16,
    ACTION_PARRY = 17,
    ACTION_HACK = 18,
    ACTION_BERZERK = 19,
    ACTION_FIREBALL = 20,
    ACTION_DISPELL = 21,
    ACTION_CONFUSE = 22,
    ACTION_LIGHTNING = 23,
    ACTION_DISRUPT = 24,
    ACTION_MELEE = 25,
    ACTION_INVOKE = 27,
    ACTION_SLASH = 28,
    ACTION_CLEAVE = 29,
    ACTION_BASH = 30,
    ACTION_STUN = 31,
    ACTION_SHOOT = 32,
    ACTION_SPELLSHIELD = 33,
    ACTION_FIRESHIELD = 34,
    ACTION_FLUXCAGE = 35,
    ACTION_HEAL = 36,
    ACTION_CALM = 37,
    ACTION_LIGHT = 38,
    ACTION_WINDOW = 39,
    ACTION_SPIT = 40,
    ACTION_BRANDISH = 41,
    ACTION_THROW = 42,
    ACTION_FUSE = 43
};

/* Values of g_action_damage other than a damage. */
enum { ACTION_DAMAGE_CANT_REACH = -1, ACTION_DAMAGE_NO_AMMUNITION = -2 };

/* MENU.C state of the action menu. */
extern int16_t g_acting_champion_ordinal; /* 0: none */
extern uint8_t g_action_list_indices[3];  /* 0xff: none */
extern int16_t g_action_damage;           /* 0: none, -1 can't reach, -2 need ammo */

/* Closes the action menu: the action hand and the action area are drawn again. */
void action_clear_acting_champion(void);

/* The group on the square in front of the acting champion, or THING_ENDOFLIST. */
extern uint16_t g_action_target_group_thing;

/* Commands 113-115: runs action list entry 0-2 of the acting champion. Returns 1 when the action was performed. */
int16_t action_did_click_trigger_action(int16_t action_list_index);
bool action_is_action_performed(uint16_t champion_index, int16_t action);
bool action_is_melee_action_performed(uint16_t champion_index, int16_t action, int16_t x, int16_t y,
                                      int16_t skill_index);
bool action_is_group_frightened_by_action(uint16_t champion_index, int16_t action, int16_t x, int16_t y);
void action_decrement_charges(uint16_t champion_index);
void action_print_action_message(uint16_t champion_index, const char *text);

/* Throwing: slot SLOT_HAND_ACTION, or the leader hand object with action_is_leader_hand_object_thrown. side 1 throws
   from the right side of the party. */
bool action_is_object_thrown(uint16_t champion_index, int16_t slot, int16_t side);
bool action_is_leader_hand_object_thrown(int16_t side);
int16_t action_get_throwing_stamina_cost(uint16_t thing);

/* Ammunition in the quiver for the launcher in the weapon slot. */
bool action_is_ammunition_compatible_with_weapon(uint16_t champion_index, uint16_t weapon_slot,
                                                 uint16_t ammunition_slot);
void action_move_weapon_from_quiver_to_slot(uint16_t champion_index, uint16_t slot);

#endif
