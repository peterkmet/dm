#ifndef INVENTORY_H
#define INVENTORY_H
/* Prefixes: inventory_ object_ process_ chest_ close_ */
#include <stdint.h>
#include "map.h"
#include "tables.h"

/* Objects, slots, and the leader hand: docs/rules-champions.md. */

enum {
    POTION_ROS = 6, POTION_KU, POTION_DANE, POTION_NETA, POTION_ANTIVENIN, POTION_MON, POTION_YA, POTION_EE,
    POTION_VI, POTION_WATER, POTION_EMPTY_FLASK = 20,
};
enum { JUNK_BONES = 5 };
enum { ARMOUR_SHARP_RESISTANCE = 0x07, ARMOUR_SHIELD = 0x80 };   /* ArmourInfo.attributes */
enum { OBJECT_EDIBLE = 1 };               /* ObjectInfo.allowed_slots: the mouth */

extern int16_t g_inventory_champion_ordinal;  /* 0: no inventory open */
extern int16_t g_leader_empty_handed;
extern uint16_t g_open_chest_slots[8];        /* slots SLOT_CHEST_FIRST and up */
extern uint16_t g_open_chest;                 /* THING_NONE: no chest is open */

int16_t inventory_get_object_info_index(uint16_t thing);
int16_t object_get_type(uint16_t thing);
int16_t object_get_icon_index(uint16_t thing);
uint16_t inventory_get_object_weight(uint16_t thing);
uint16_t inventory_get_armour_defense(const ArmourInfo *info, int16_t use_sharp_defense);

void inventory_put_object_in_leader_hand(uint16_t thing);
uint16_t inventory_get_object_removed_from_leader_hand(void);
uint16_t inventory_get_object_removed_from_slot(uint16_t champion_index, uint16_t slot);
void inventory_add_object_in_slot(uint16_t champion_index, uint16_t thing, uint16_t slot);
void inventory_click_on_slot_box(uint16_t slot_box);
/* Results of process_click_on_mouth: nothing happens, the champion swallows, or the object is used up. */
enum { MOUTH_REFUSED, MOUTH_SWALLOWED, MOUTH_USED_UP };
int16_t process_click_on_mouth(void);
void chest_open(uint16_t chest);
void close_chest(void);
void inventory_reset(void);

#endif
