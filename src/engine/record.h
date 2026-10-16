/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_ENGINE_RECORD_H
#define DM_ENGINE_RECORD_H
/* Prefixes: rec_ */

/* The byte layout of the dungeon records: one accessor per field, no record byte outside this module. */

#include <stdbool.h>
#include <stdint.h>

/* Size of a group record. */
#define GROUP_RECORD_BYTES 16
/* Option of rec_find_unused_thing in the thing type: junk takes a reserve record too. */
#define NO_JUNK_RESERVE    0x8000

/* Records of every thing type */
/* The record bytes of the thing, read only. */
const uint8_t *rec_read(uint16_t thing);
/* The record bytes of the thing, exported for the tests. */
uint8_t *rec_write(uint16_t thing);
uint16_t rec_thing_next(uint16_t thing);
/* Every record starts with the next thing of its list. 0xffff marks a free record. */
void rec_thing_set_next(uint16_t thing, uint16_t value);
void rec_thing_set_free(uint16_t thing);
/* A free record of the type. Junk keeps 3 records in reserve unless NO_JUNK_RESERVE is set.
   ponytail: no get_discarded_thing yet; returns THING_NONE when no record is free. */
uint16_t rec_find_unused_thing(uint16_t thing_type);

/* Door record (thing type 0) */
uint16_t rec_door_type(uint16_t thing);
bool rec_door_is_magic_breakable(uint16_t thing);
bool rec_door_is_force_breakable(uint16_t thing);
bool rec_door_has_button(uint16_t thing);
/* A door that slides down; it hits a creature by its height. */
bool rec_door_is_vertical(uint16_t thing);
/* The stored value: 0 for none, else the door ornament index + 1. */
uint16_t rec_door_ornament(uint16_t thing);

/* Teleporter record (thing type 1). Scope: 1 creatures, 2 party, 3 everything. */
int16_t rec_teleporter_target_x(uint16_t thing);
int16_t rec_teleporter_target_y(uint16_t thing);
int16_t rec_teleporter_target_map(uint16_t thing);
uint16_t rec_teleporter_scope(uint16_t thing);
uint16_t rec_teleporter_rotation(uint16_t thing);
bool rec_teleporter_is_absolute(uint16_t thing);
bool rec_teleporter_is_audible(uint16_t thing);

/* Text record (thing type 2) */
bool rec_text_is_visible(uint16_t thing);
void rec_text_set_visible(uint16_t thing, bool value);
/* The word offset of the text in the text data. */
uint16_t rec_text_offset(uint16_t thing);

/* Sensor record (thing type 3) */
uint16_t rec_sensor_type(uint16_t thing);
uint16_t rec_sensor_data(uint16_t thing);
void rec_sensor_set_type(uint16_t thing, uint16_t value);
void rec_sensor_set_data(uint16_t thing, uint16_t value);
bool rec_sensor_is_local(uint16_t thing);
uint16_t rec_sensor_delay(uint16_t thing);
bool rec_sensor_is_once_only(uint16_t thing);
uint16_t rec_sensor_effect(uint16_t thing);
bool rec_sensor_is_reverted(uint16_t thing);
bool rec_sensor_is_audible(uint16_t thing);
int16_t rec_sensor_target_x(uint16_t thing);
int16_t rec_sensor_target_y(uint16_t thing);
int16_t rec_sensor_target_cell(uint16_t thing);
int16_t rec_sensor_local_effect(uint16_t thing);
uint16_t rec_sensor_generator_delay(uint16_t thing);
/* The stored value: 0 for none, else the wall or floor ornament index + 1. */
uint16_t rec_sensor_ornament(uint16_t thing);
/* Group generator: creature type in the data low byte, count field in word 4 bits 7-10, health multiplier in
   word 6 bits 4-7. */
uint16_t rec_sensor_creature_type(uint16_t thing);
/* The count field of a group generator: the same bits as the delay. */
uint16_t rec_sensor_generator_count(uint16_t thing);
uint16_t rec_sensor_health_multiplier(uint16_t thing);
/* Launcher: kinetic energy in word 6 bits 4-11, step energy in bits 12-15. */
uint8_t rec_sensor_kinetic_energy(uint16_t thing);
uint8_t rec_sensor_step_energy(uint16_t thing);

/* Group record, 16 bytes (thing type 4): next, possessions, type, cells, 4 health words, word 14 flags.
   Byte 14: direction in bits 0-1, do not discard in bit 2. Byte 15: behavior in bits 0-3, last creature index in bits 5-6. */
uint16_t rec_group_possessions(uint16_t thing);
void rec_group_set_possessions(uint16_t thing, uint16_t value);
uint16_t rec_group_creature_type(uint16_t thing);
void rec_group_set_creature_type(uint16_t thing, uint16_t value);
/* 2 bits per creature, 0xff for one centered creature; on the party map the active group index. */
uint8_t rec_group_cells(uint16_t thing);
void rec_group_set_cells(uint16_t thing, uint8_t value);
uint16_t rec_group_health(uint16_t group, int16_t creature);
void rec_group_set_health(uint16_t group, int16_t creature, uint16_t health);
uint16_t rec_group_behavior(uint16_t thing);
void rec_group_set_behavior(uint16_t thing, uint16_t value);
uint8_t rec_group_last_creature(uint16_t thing);
void rec_group_set_last_creature(uint16_t thing, uint8_t value);
uint8_t rec_group_direction(uint16_t thing);
void rec_group_set_direction(uint16_t thing, uint8_t value);
void rec_group_set_no_discard(uint16_t thing, bool value);

/* Object records (thing types 5-10) */
bool rec_object_is_cursed(uint16_t thing);
void rec_object_set_cursed(uint16_t thing, bool value);
void rec_object_set_important(uint16_t thing, bool important);
/* Weapon, armour, and junk: the type in word 2 bits 0-6; the important bit stays. */
void rec_object_set_type(uint16_t thing, uint16_t value);

/* Weapon record (thing type 5) */
uint16_t rec_weapon_type(uint16_t thing);
bool rec_weapon_is_lit(uint16_t thing);
void rec_weapon_set_lit(uint16_t thing, bool value);
uint16_t rec_weapon_charges(uint16_t thing);
void rec_weapon_set_charges(uint16_t thing, uint16_t value);
bool rec_weapon_is_poisoned(uint16_t thing);
bool rec_weapon_is_broken(uint16_t thing);

/* Armour record (thing type 6) */
uint16_t rec_armour_type(uint16_t thing);
uint16_t rec_armour_charges(uint16_t thing);
void rec_armour_set_charges(uint16_t thing, uint16_t value);
bool rec_armour_is_broken(uint16_t thing);

/* Scroll record (thing type 7) */
bool rec_scroll_is_closed(uint16_t thing);
/* The closed bits are 10-15; bits 8-9 stay. */
void rec_scroll_set_closed(uint16_t thing, bool value);
/* Text record index of the scroll text. */
uint16_t rec_scroll_text_index(uint16_t thing);

/* Potion record (thing type 8) */
uint16_t rec_potion_type(uint16_t thing);
void rec_potion_set_type(uint16_t thing, uint16_t value);
uint16_t rec_potion_power(uint16_t thing);
void rec_potion_set_power(uint16_t thing, uint16_t value);

/* Container record (thing type 9) */
uint16_t rec_container_type(uint16_t thing);
uint16_t rec_container_first_thing(uint16_t thing);
void rec_container_set_first_thing(uint16_t thing, uint16_t value);

/* Junk record (thing type 10) */
uint16_t rec_junk_type(uint16_t thing);
void rec_junk_set_type(uint16_t thing, uint16_t value);
uint16_t rec_junk_charges(uint16_t thing);
void rec_junk_set_charges(uint16_t thing, uint16_t value);

/* Projectile record, 8 bytes (thing type 14): next, slot (the flying thing), kinetic energy, attack, event index. */
uint16_t rec_projectile_slot(uint16_t thing);
void rec_projectile_set_slot(uint16_t thing, uint16_t value);
uint8_t rec_projectile_energy(uint16_t thing);
void rec_projectile_set_energy(uint16_t thing, uint8_t value);
uint8_t rec_projectile_attack(uint16_t thing);
void rec_projectile_set_attack(uint16_t thing, uint8_t value);
/* The timeline event that moves the projectile. */
uint16_t rec_projectile_event_index(uint16_t thing);
void rec_projectile_set_event_index(uint16_t thing, uint16_t value);

/* Explosion record, 4 bytes (thing type 15): next, attack, type in bits 0-6, centered in bit 7. */
uint8_t rec_explosion_attack(uint16_t thing);
void rec_explosion_set_attack(uint16_t thing, uint8_t value);
uint8_t rec_explosion_type(uint16_t thing);
void rec_explosion_set_type(uint16_t thing, uint8_t value);
bool rec_explosion_is_centered(uint16_t thing);
void rec_explosion_set_centered(uint16_t thing, bool value);

#endif
