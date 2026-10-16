#ifndef INVENTORY_H
#define INVENTORY_H
/* Prefixes: inventory_ object_ */
#include "data/tables.h"
#include "engine/map.h"
#include <stdbool.h>
#include <stdint.h>

/* Objects, slots, and the leader hand: docs/rules/champions.md. */

/* Object types of the rules: the weapon, potion, and junk type of a record. */
enum {
    WEAPON_TORCH = 2,
    WEAPON_DAGGER = 8,
    WEAPON_ARROW = 27,
    WEAPON_SLAYER = 28,
    WEAPON_ROCK = 30,
    WEAPON_POISON_DART = 31,
    WEAPON_THROWING_STAR = 32
};
enum {
    POTION_VEN_BOMB = 3,
    POTION_ROS = 6,
    POTION_KU,
    POTION_DANE,
    POTION_NETA,
    POTION_ANTIVENIN,
    POTION_MON,
    POTION_YA,
    POTION_EE,
    POTION_VI,
    POTION_WATER,
    POTION_FUL_BOMB = 19,
    POTION_EMPTY_FLASK = 20,
};
#define POTION_POWER_PER_LEVEL 40 /* potion power of one power symbol level */
enum {
    JUNK_WATERSKIN = 1, /* each charge weighs 0.2 kg */
    JUNK_BONES = 5,
    JUNK_BOULDER = 25,
    JUNK_MAGICAL_BOX_BLUE = 42,
    JUNK_MAGICAL_BOX_GREEN = 43,
    JUNK_ZOKATHRA = 51
};
enum { OBJECT_TYPE_ZOKATHRA = 197 }; /* object type of the ZOKATHRA SPELL junk, that DAIN shows */
enum { ARMOUR_SHARP_RESISTANCE = 0x07, ARMOUR_SHIELD = 0x80 }; /* ArmourInfo.attributes */
enum { OBJECT_EDIBLE = 1 };                                    /* ObjectInfo.allowed_slots: the mouth */

extern int16_t g_inventory_champion_ordinal; /* 0: no inventory open */
extern bool g_leader_empty_handed;
extern uint16_t g_open_chest_slots[8]; /* slots SLOT_CHEST_FIRST and up */
extern uint16_t g_open_chest;          /* THING_NONE: no chest is open */

int16_t inventory_get_object_info_index(uint16_t thing);
int16_t object_get_type(uint16_t thing);
int16_t object_get_icon_index(uint16_t thing);
uint16_t inventory_get_object_weight(uint16_t thing);
uint16_t inventory_get_armour_defense(const ArmourInfo *info, bool use_sharp_defense);

void inventory_put_object_in_leader_hand(uint16_t thing);
uint16_t inventory_get_object_removed_from_leader_hand(void);
uint16_t inventory_get_object_removed_from_slot(uint16_t champion_index, uint16_t slot);
void inventory_add_object_in_slot(uint16_t champion_index, uint16_t thing, uint16_t slot);
void inventory_click_on_slot_box(uint16_t slot_box);
bool inventory_fill_leader_hand_at_fountain(void);
void inventory_set_open_champion(int16_t ordinal);
/* The light amount of the party: light sources in the hands and the light spells. */
int16_t inventory_get_party_light(void);
/* Sets g_dungeon_view_palette_index from the party light amount (magical light plus torches) on map map_index. */
void inventory_set_dungeon_view_palette(int16_t map_index, int16_t light);
void inventory_decrease_torches_light_power(void);
/* Results of inventory_process_click_on_mouth: nothing happens, the champion swallows, or the object is used up. */
enum { MOUTH_REFUSED, MOUTH_SWALLOWED, MOUTH_USED_UP };
int16_t inventory_process_click_on_mouth(void);
void inventory_open_chest(uint16_t chest);
void inventory_close_chest(void);
void inventory_reset(void);

#endif
