#ifndef THINGS_H
#define THINGS_H
#include <stdint.h>
#include "map.h"

/* Record fields that the dungeon view reads, in addition to map.h (big-endian, see
   docs/symbols/range-4-6.md and range-7-9.md). */

/* Weapon record: word 2 bits 0-6 weapon type. */
static inline uint16_t thing_weapon_type(uint16_t weapon) { return REC(weapon)[3] & 0x7f; }

/* Group record, 16 bytes. */
/* 2 bits per creature, 0xff for one centered creature; on the party map the active group index. */
static inline uint8_t group_cells_byte(uint16_t group) { return REC(group)[5]; }
static inline uint8_t group_last_creature_index(uint16_t group) { return (REC(group)[15] & 0x60) >> 5; }
static inline uint8_t group_direction(uint16_t group) { return REC(group)[14] & 3; }

/* Projectile record, 8 bytes. */
static inline uint16_t projectile_slot(uint16_t projectile) { return BE16(REC(projectile) + 2); }
static inline uint8_t projectile_kinetic_energy(uint16_t projectile) { return REC(projectile)[4]; }

/* Explosion record, 4 bytes. */
static inline uint8_t explosion_attack(uint16_t explosion) { return REC(explosion)[2]; }
static inline uint8_t explosion_type(uint16_t explosion) { return REC(explosion)[3] & 0x7f; }
static inline int explosion_is_centered(uint16_t explosion) { return REC(explosion)[3] >> 7; }

/* Map descriptor bytes of the view: ornament counts, difficulty, creature types, graphic sets. */
static inline int16_t map_random_floor_ornament_count(int16_t map) { return MAP_DESCRIPTOR(map)[10] >> 4; }
static inline int16_t map_floor_ornament_count(int16_t map) { return MAP_DESCRIPTOR(map)[10] & 15; }
static inline int16_t map_random_wall_ornament_count(int16_t map) { return MAP_DESCRIPTOR(map)[11] >> 4; }
static inline int16_t map_wall_ornament_count(int16_t map) { return MAP_DESCRIPTOR(map)[11] & 15; }
static inline int16_t map_difficulty(int16_t map) { return MAP_DESCRIPTOR(map)[12] >> 4; }
static inline int16_t map_door_ornament_count(int16_t map) { return MAP_DESCRIPTOR(map)[13] & 15; }
static inline int16_t map_creature_type_count(int16_t map) { return MAP_DESCRIPTOR(map)[13] >> 4; }
static inline int16_t map_floor_set(int16_t map) { return MAP_DESCRIPTOR(map)[15] & 15; }
static inline int16_t map_wall_set(int16_t map) { return MAP_DESCRIPTOR(map)[15] >> 4; }

/* Dungeon header word 0: the seed of the random wall and floor ornaments. */
static inline uint16_t dungeon_ornament_seed(void) { return BE16(g_dungeon_header); }

#endif
