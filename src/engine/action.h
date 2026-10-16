/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_ENGINE_ACTION_H
#define DM_ENGINE_ACTION_H
/* Prefixes: action_ */
#include "data/tables.h"
#include "engine/map.h"
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

/* Values of g_session.action_damage other than a damage. */
#define ACTION_DAMAGE_CANT_REACH    (-1)
#define ACTION_DAMAGE_NO_AMMUNITION (-2)

/* The actions of the acting champion's action hand: up to ACTION_LIST_COUNT, ACTION_NONE for an empty line. */
typedef struct {
    uint8_t actions[ACTION_LIST_COUNT];
    uint8_t minimum_skill_levels[ACTION_LIST_COUNT];
    uint16_t count;
} ActionList;
extern ActionList g_action_list;

/* Builds the action menu of the acting champion: the first action always; the other two when the
   champion has the skill level and, when the action needs them, charges. */
void action_set_action_list(int16_t action_set_index);
/* Makes a champion the acting champion with the actions of its action hand. Returns false when it cannot act. */
bool action_set_acting_champion(int16_t champion_index);
/* The action damage to show, which is then cleared. */
int16_t action_take_damage(void);
/* No acting champion, no action list, no action damage, and no redraw request at the start of a game. */
void action_reset(void);

/* Closes the action menu: the action hand and the action area are drawn again. */
void action_clear_acting_champion(void);

/* Commands 113-115: runs action list entry 0-2 of the acting champion. Returns true when the action was performed. */
bool action_did_click_trigger_action(int16_t action_list_index);
/* Runs one action of a champion against the square in front of the champion. After the action the action hand is
   disabled for the action ticks, the champion loses stamina, and the action skill gains experience. A failed
   melee action halves the ticks and the experience. Returns false when the action fails. The function is long: one
   short case for each action of the action table. */
bool action_is_action_performed(int16_t champion_index, int16_t action);
/* A melee hit on target_group, the group in front (THING_ENDOFLIST: none). A champion in a back cell reaches only past an empty front cell ("can't
   reach"). Disrupt hits only non-material creatures. Returns false when the champion hits nothing. */
bool action_is_melee_action_performed(int16_t champion_index, int16_t action, Location target_square,
                                      int16_t skill_index, uint16_t target_group);
/* War cry, blow horn, confuse, calm, brandish: the group in front flees when the influence roll beats its fear
   resistance. The champion gains influence experience, half of it when the group stays. */
bool action_is_group_frightened_by_action(int16_t champion_index, int16_t action, int16_t x, int16_t y,
                                          uint16_t target_group);
/* The object in the action hand loses one charge: weapons, armour, and junk keep their charges in different bits. */
void action_decrement_charges(int16_t champion_index);
/* Prints "<text>" in the action color; "@p" in the text becomes the champion name. */
void action_print_action_message(int16_t champion_index, const char *text);

/* Throwing: slot SLOT_HAND_ACTION, or the leader hand object with action_is_leader_hand_object_thrown. side 1 throws
   from the right side of the party. */
bool action_is_object_thrown(int16_t champion_index, int16_t slot, bool side);
/* The leader throws the object in the leader hand (a click in the upper dungeon view). */
bool action_is_leader_hand_object_thrown(bool side);
/* Stamina cost of a throw: half the weight, 1 to 10, plus half of each 10 above. */
int16_t action_get_throwing_stamina_cost(uint16_t thing);

/* Ammunition in the quiver for the launcher in the weapon slot. */
bool action_is_ammunition_compatible_with_weapon(int16_t champion_index, uint16_t weapon_slot,
                                                 uint16_t ammunition_slot);
/* After a throw: the next weapon of the quiver goes to the empty slot (quiver line 1 first, then line 2). */
void action_move_weapon_from_quiver_to_slot(int16_t champion_index, int16_t slot);

#endif
