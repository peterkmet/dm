/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_ENGINE_ACTION_H
#define DM_ENGINE_ACTION_H
/* Prefixes: action_ */
#include <stdbool.h>
#include <stdint.h>

/* Champion actions of the action area (KAOS range 8, MENU.C): docs/rules/champions.md#actions. */

/* Action indices (graphic 699 holds the names). Actions without an entry here only train the skill and add the
   action defense. */
#define ACTION_N           0
#define ACTION_BLOCK       1
#define ACTION_CHOP        2
#define ACTION_BLOW_HORN   4
#define ACTION_FLIP        5
#define ACTION_PUNCH       6
#define ACTION_KICK        7
#define ACTION_WAR_CRY     8
#define ACTION_STAB_1      9
#define ACTION_CLIMB_DOWN  10
#define ACTION_FREEZE_LIFE 11
#define ACTION_HIT         12
#define ACTION_SWING       13
#define ACTION_STAB_2      14
#define ACTION_THRUST      15
#define ACTION_JAB         16
#define ACTION_PARRY       17
#define ACTION_HACK        18
#define ACTION_BERZERK     19
#define ACTION_FIREBALL    20
#define ACTION_DISPELL     21
#define ACTION_CONFUSE     22
#define ACTION_LIGHTNING   23
#define ACTION_DISRUPT     24
#define ACTION_MELEE       25
#define ACTION_INVOKE      27
#define ACTION_SLASH       28
#define ACTION_CLEAVE      29
#define ACTION_BASH        30
#define ACTION_STUN        31
#define ACTION_SHOOT       32
#define ACTION_SPELLSHIELD 33
#define ACTION_FIRESHIELD  34
#define ACTION_FLUXCAGE    35
#define ACTION_HEAL        36
#define ACTION_CALM        37
#define ACTION_LIGHT       38
#define ACTION_WINDOW      39
#define ACTION_SPIT        40
#define ACTION_BRANDISH    41
#define ACTION_THROW       42
#define ACTION_FUSE        43

/* Values of g_action_damage other than a damage. */
#define ACTION_DAMAGE_CANT_REACH    (-1)
#define ACTION_DAMAGE_NO_AMMUNITION (-2)

/* MENU.C state of the action menu. */
extern int16_t g_acting_champion_ordinal; /* 0: none */
extern uint8_t g_action_list_indices[3];  /* 0xff: none */
extern int16_t g_action_damage;           /* 0: none, -1 can't reach, -2 need ammo */
extern uint8_t g_action_list_minimum_skill_levels[3];
extern uint16_t g_action_count;

void action_set_action_list(int16_t action_set_index);
/* Makes a champion the acting champion with the actions of its action hand. Returns false when it cannot act. */
bool action_set_acting_champion(uint16_t champion_index);
/* The action damage to show, which is then cleared. */
int16_t action_take_damage(void);
void action_reset(void);

/* Closes the action menu: the action hand and the action area are drawn again. */
void action_clear_acting_champion(void);

/* The group on the square in front of the acting champion, or THING_ENDOFLIST. */
extern uint16_t g_action_target_group_thing;

/* Commands 113-115: runs action list entry 0-2 of the acting champion. Returns true when the action was performed. */
bool action_did_click_trigger_action(int16_t action_list_index);
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
