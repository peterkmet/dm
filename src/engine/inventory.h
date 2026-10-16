/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_ENGINE_INVENTORY_H
#define DM_ENGINE_INVENTORY_H
/* Prefixes: inventory_ object_ */
#include "data/tables.h"
#include "engine/map.h"
#include <stdbool.h>
#include <stdint.h>

/* Objects, slots, and the leader hand: docs/rules/champions.md. */

/* Object types of the rules: the weapon, potion, and junk type of a record. */
#define WEAPON_TORCH            2
#define WEAPON_DAGGER           8
#define WEAPON_ARROW            27
#define WEAPON_SLAYER           28
#define WEAPON_ROCK             30
#define WEAPON_POISON_DART      31
#define WEAPON_THROWING_STAR    32
#define POTION_VEN_BOMB         3
#define POTION_ROS              6
#define POTION_KU               7
#define POTION_DANE             8
#define POTION_NETA             9
#define POTION_ANTIVENIN        10
#define POTION_MON              11
#define POTION_YA               12
#define POTION_EE               13
#define POTION_VI               14
#define POTION_WATER            15
#define POTION_FUL_BOMB         19
#define POTION_EMPTY_FLASK      20
#define POTION_POWER_PER_LEVEL  40 /* potion power of one power symbol level */
#define JUNK_WATERSKIN          1  /* each charge weighs 0.2 kg */
#define JUNK_BONES              5
#define JUNK_BOULDER            25
#define JUNK_MAGICAL_BOX_BLUE   42
#define JUNK_MAGICAL_BOX_GREEN  43
#define JUNK_ZOKATHRA           51
#define OBJECT_TYPE_ZOKATHRA    197  /* object type of the ZOKATHRA SPELL junk, that DAIN shows */
#define ARMOUR_SHARP_RESISTANCE 0x07 /* ArmourInfo.attributes */
#define ARMOUR_SHIELD           0x80
#define OBJECT_EDIBLE           1 /* ObjectInfo.allowed_slots: the mouth */

/* differs from g_leader_hand_object == THING_NONE while a load restored the hand object and the game has not started */
extern bool g_leader_empty_handed;
extern uint16_t g_open_chest_slots[8]; /* slots SLOT_CHEST_FIRST and up */
extern uint16_t g_open_chest;          /* THING_NONE: no chest is open */

/* The object info record of an object thing, or -1. */
int16_t inventory_get_object_info_index(uint16_t thing);
/* The object type (base icon), or -1 for no thing or a thing that is not an object. */
int16_t object_get_type(uint16_t thing);
/* The icon shows the state: compass direction, lit torch, charges, worn jewels, closed scroll. */
int16_t object_get_icon_index(uint16_t thing);
/* Tenths of a kilogram. A chest weighs 5 kg plus its contents. */
uint16_t inventory_get_object_weight(uint16_t thing);
/* Sharp attacks use the sharp resistance (attributes bits 0-2) of the armour. */
uint16_t inventory_get_armour_defense(const ArmourInfo *info, bool use_sharp_defense);

void inventory_put_object_in_leader_hand(uint16_t thing);
uint16_t inventory_get_object_removed_from_leader_hand(void);
/* Takes an object from a slot: bonuses end, a torch goes out, a scroll closes. */
uint16_t inventory_get_object_removed_from_slot(uint16_t champion_index, uint16_t slot);
/* Puts an object in a slot: bonuses start, a torch in a hand burns, a scroll opens. */
void inventory_add_object_in_slot(uint16_t champion_index, uint16_t thing, uint16_t slot);
/* Slot boxes 0-7: the two hands of champions 0-3. 8 and up: slot (box - 8) of the open inventory.
   Swaps the leader hand object with the slot object when the slot accepts it. */
void inventory_click_on_slot_box(uint16_t slot_box);
/* A flask or a waterskin in the leader hand fills at a fountain; the leader load changes with its weight.
   Returns true when the hand object changed. */
bool inventory_fill_leader_hand_at_fountain(void);
/* Opens the inventory of a champion for the rules; -1 closes it. */
void inventory_set_open_champion(int16_t champion_index);
/* Results of inventory_process_click_on_mouth: nothing happens, the champion swallows, or the object is used up. */
#define MOUTH_REFUSED   0
#define MOUTH_SWALLOWED 1
#define MOUTH_USED_UP   2
/* The inventory champion eats or drinks the leader hand object. Food is used up, a potion
   leaves an empty flask, and a waterskin loses one charge. */
int16_t inventory_process_click_on_mouth(void);
/* Opens a chest in the panel: its first 8 objects go to the chest slots, the other slots are empty. */
void inventory_open_chest(uint16_t chest);
/* Closes the open chest: the objects in the chest slots become its content, in slot order. */
void inventory_close_chest(void);
/* No inventory and no chest are open at the start of a game. */
void inventory_reset(void);

extern uint16_t g_leader_hand_object;

#endif
