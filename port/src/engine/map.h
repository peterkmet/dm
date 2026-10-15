#ifndef MAP_H
#define MAP_H
/* Prefixes: door_ square_ thing_ get_ weapon_ armour_ junk_ potion_ container_ rec_ scroll_ teleporter_ map_ set_ is_ link_ unlink_ */
#include <stdbool.h>
#include <stdint.h>
#include "bytes.h"
#include "dungeon.h"

/* Current map, square bytes, thing lists, and the record fields that the movement rules use.
   Coordinates are on the current map. All records keep the big-endian layout of Dungeon.DAT. */

enum { DIRECTION_NORTH, DIRECTION_EAST, DIRECTION_SOUTH, DIRECTION_WEST };
#define DIRECTION_BITS 2
enum { LEVEL_UP = -1, LEVEL_DOWN = 1 };
#define MAP_COORD_BITS 5 /* maps are at most 32 squares wide and high */
#define MAP_COORD_MASK 0x1f

/* Square byte: type in bits 5-7, a thing list in bit 4, type flags in bits 0-3. */
enum { SQUARE_WALL, SQUARE_CORRIDOR, SQUARE_PIT, SQUARE_STAIRS, SQUARE_DOOR, SQUARE_TELEPORTER, SQUARE_FAKE_WALL };
enum { SQUARE_HAS_THINGS = 0x10, SQUARE_RANDOM_ORNAMENT = 8 }; /* random ornament: corridor and open fake wall */
enum { WALL_ORNAMENT_RIGHT = 1, WALL_ORNAMENT_ABOVE = 2, WALL_ORNAMENT_LEFT = 4, WALL_ORNAMENT_BELOW = 8 };
enum { PIT_IMAGINARY = 1, PIT_INVISIBLE = 4, PIT_OPEN = 8 };
enum { STAIRS_UP = 4, STAIRS_NORTH_SOUTH = 8 };
enum { DOOR_STATE_MASK = 7, DOOR_OPEN = 0, DOOR_ALMOST_OPEN = 1, DOOR_CLOSED = 4, DOOR_DESTROYED = 5 };
enum { TELEPORTER_VISIBLE = 4, TELEPORTER_OPEN = 8 };
enum { FAKE_WALL_IMAGINARY = 1, FAKE_WALL_OPEN = 4 };
#define SQUARE_TYPE(square) ((square) >> 5)

static inline int16_t door_state(uint8_t square)
{
    return square & DOOR_STATE_MASK;
}
static inline uint8_t square_with_door_state(uint8_t square, int16_t state)
{
    return (square & ~DOOR_STATE_MASK) | (state & DOOR_STATE_MASK);
}

/* Thing word: index in bits 0-9, type in bits 10-13, cell in bits 14-15. */
enum {
    THING_DOOR,
    THING_TELEPORTER,
    THING_TEXT,
    THING_SENSOR,
    THING_GROUP,
    THING_WEAPON,
    THING_ARMOUR,
    THING_SCROLL,
    THING_POTION,
    THING_CONTAINER,
    THING_JUNK,
    THING_PROJECTILE = 14,
    THING_EXPLOSION = 15
};
#define THING_ENDOFLIST              0xfffe
#define THING_END                    THING_ENDOFLIST
#define THING_NONE                   0xffff
#define THING_TYPE_SHIFT             10
#define THING_TYPE(thing)            ((thing) >> THING_TYPE_SHIFT & 0x0f)
#define THING_MAKE(type, index)      ((uint16_t)((type) << THING_TYPE_SHIFT | (index)))
#define THING_CELL(thing)            ((uint16_t)(thing) >> 14)
#define THING_INDEX(thing)           ((thing) & 0x3ff) /* record index without type and cell */
#define THING_STRIP_CELL(thing)      ((thing) & 0x3fff)
#define THING_WITH_CELL(thing, cell) ((uint16_t)(((cell) & 3) << 14 | THING_STRIP_CELL(thing)))

#define REC(thing) dungeon_get_thing_data(thing)

/* Every record starts with the next thing of its list. 0xffff marks a free record. */
static inline void thing_set_next(uint16_t thing, uint16_t next)
{
    WR16(REC(thing), next);
}
void thing_set_free(uint16_t thing);

/* Object records (types 5-10) and free records. */
#define NO_JUNK_RESERVE 0x8000
uint16_t get_unused_thing(uint16_t thing_type);
uint16_t weapon_type(uint16_t thing);
uint16_t armour_type(uint16_t thing);
uint16_t junk_type(uint16_t thing);
uint16_t potion_type(uint16_t thing);
uint16_t potion_power(uint16_t thing);
uint16_t container_type(uint16_t thing);
uint16_t container_first_thing(uint16_t thing);
bool rec_object_is_cursed(uint16_t thing);
void rec_object_set_important(uint16_t thing);
void rec_object_clear_important(uint16_t thing);
bool weapon_is_lit(uint16_t thing);
uint16_t weapon_charges(uint16_t thing);
uint16_t junk_charges(uint16_t thing);
uint16_t armour_charges(uint16_t thing);
bool scroll_is_closed(uint16_t thing);
bool weapon_is_poisoned(uint16_t thing);
bool weapon_is_broken(uint16_t thing);
bool armour_is_broken(uint16_t thing);
uint16_t scroll_text_index(uint16_t thing); /* text record index of the scroll text */
void container_set_first_thing(uint16_t thing, uint16_t first);
void weapon_set_lit(uint16_t thing, bool lit);
void weapon_set_charges(uint16_t thing, uint16_t charges);
void junk_set_charges(uint16_t thing, uint16_t charges);
void armour_set_charges(uint16_t thing, uint16_t charges);
void junk_set_type(uint16_t thing, uint16_t type);
void potion_set_type(uint16_t thing, uint16_t type);
void potion_set_power(uint16_t thing, uint16_t power);
void rec_object_set_type(uint16_t thing, uint16_t type);
void rec_object_set_cursed(uint16_t thing, bool cursed);
void scroll_set_closed(uint16_t thing, bool closed);

/* Sensor record (thing type 3). */
enum {
    SENSOR_DISABLED = 0,
    SENSOR_FLOOR_ANY = 1,
    SENSOR_FLOOR_PARTY_OR_GROUP = 2,
    SENSOR_FLOOR_PARTY = 3,
    SENSOR_FLOOR_OBJECT = 4,
    SENSOR_FLOOR_PARTY_ON_STAIRS = 5,
    SENSOR_FLOOR_GROUP_GENERATOR = 6,
    SENSOR_FLOOR_GROUP = 7,
    SENSOR_FLOOR_PARTY_POSSESSION = 8,
    SENSOR_FLOOR_VERSION_CHECKER = 9
};
enum {
    SENSOR_WALL_ORNAMENT_CLICK = 1,
    SENSOR_WALL_ANY_OBJECT = 2,
    SENSOR_WALL_OBJECT = 3,
    SENSOR_WALL_OBJECT_REMOVED = 4,
    SENSOR_WALL_AND_OR_GATE = 5,
    SENSOR_WALL_COUNTER = 6,
    SENSOR_WALL_SINGLE_OBJECT_STORAGE = 11,
    SENSOR_WALL_OBJECT_GENERATOR = 12,
    SENSOR_WALL_SINGLE_OBJECT_EXCHANGER = 13,
    SENSOR_WALL_OBJECT_EXCHANGER = 16,
    SENSOR_WALL_OBJECT_REMOVED_SENSOR_REMOVED = 17,
    SENSOR_WALL_ENDGAME = 18,
    SENSOR_WALL_CHAMPION_PORTRAIT = 127
};
enum { SENSOR_TYPE_MASK = 0x7f, SENSOR_DATA_SHIFT = 7 };

static inline uint16_t rec_sensor_type(uint16_t thing)
{
    return BE16(REC(thing) + 2) & SENSOR_TYPE_MASK;
}
static inline uint16_t rec_sensor_data(uint16_t thing)
{
    return BE16(REC(thing) + 2) >> SENSOR_DATA_SHIFT;
}
static inline void rec_sensor_set_type(uint16_t thing, uint16_t type)
{
    WR16(REC(thing) + 2, (BE16(REC(thing) + 2) & ~SENSOR_TYPE_MASK) | type);
}
static inline void rec_sensor_set_data(uint16_t thing, uint16_t data)
{
    WR16(REC(thing) + 2, data << SENSOR_DATA_SHIFT | rec_sensor_type(thing));
}
void rec_sensor_disable(uint16_t thing);
static inline bool rec_sensor_is_local(uint16_t thing)
{
    return (REC(thing)[4] & 0x08) != 0;
}
static inline uint16_t rec_sensor_delay(uint16_t thing)
{
    return BE16(REC(thing) + 4) >> 7 & 0x0f;
}
static inline bool rec_sensor_is_once_only(uint16_t thing)
{
    return (REC(thing)[5] & 0x04) != 0;
}
static inline uint16_t rec_sensor_effect(uint16_t thing)
{
    return (REC(thing)[5] & 0x18) >> 3;
}
static inline uint16_t rec_sensor_revert(uint16_t thing)
{
    return (REC(thing)[5] & 0x20) >> 5;
}
static inline bool rec_sensor_is_audible(uint16_t thing)
{
    return (REC(thing)[5] & 0x40) != 0;
}
static inline int16_t rec_sensor_target_x(uint16_t thing)
{
    return BE16(REC(thing) + 6) >> 6 & MAP_COORD_MASK;
}
static inline int16_t rec_sensor_target_y(uint16_t thing)
{
    return REC(thing)[6] >> 3;
}
static inline int16_t rec_sensor_target_cell(uint16_t thing)
{
    return (REC(thing)[7] & 0x30) >> 4;
}
static inline int16_t rec_sensor_local_effect(uint16_t thing)
{
    return BE16(REC(thing) + 6) >> 4;
}
static inline uint16_t rec_sensor_generator_delay(uint16_t thing)
{
    return REC(thing)[6];
}
static inline uint16_t rec_sensor_ornament_ordinal(uint16_t thing)
{
    return REC(thing)[4] >> 4;
}
/* Group generator: creature type in the data low byte, count field in word 4 bits 7-10, health multiplier in
   word 6 bits 4-7. */
static inline uint16_t rec_sensor_generator_creature_type(uint16_t thing)
{
    return rec_sensor_data(thing) & 0xff;
}
static inline uint16_t rec_sensor_generator_count_field(uint16_t thing)
{
    return BE16(REC(thing) + 4) >> 7 & 0x0f;
}
static inline uint16_t rec_sensor_generator_health_multiplier(uint16_t thing)
{
    return BE16(REC(thing) + 6) >> 4 & 0x0f;
}
/* Launcher: kinetic energy in word 6 bits 4-11, step energy in bits 12-15. */
static inline uint8_t rec_sensor_launcher_kinetic_energy(uint16_t thing)
{
    return BE16(REC(thing) + 6) >> 4;
}
static inline uint8_t rec_sensor_launcher_step_energy(uint16_t thing)
{
    return REC(thing)[6] >> 4;
}

/* Teleporter record (thing type 1). Scope: 1 creatures, 2 party, 3 everything. */
enum { TELEPORTER_SCOPE_CREATURES = 1, TELEPORTER_SCOPE_PARTY = 2, TELEPORTER_SCOPE_ALL = 3 };
static inline int16_t teleporter_target_x(uint16_t thing)
{
    return REC(thing)[3] & MAP_COORD_MASK;
}
static inline int16_t teleporter_target_y(uint16_t thing)
{
    return BE16(REC(thing) + 2) >> 5 & MAP_COORD_MASK;
}
static inline int16_t teleporter_target_map(uint16_t thing)
{
    return REC(thing)[4];
}
static inline uint16_t teleporter_scope(uint16_t thing)
{
    return (REC(thing)[2] & 0x60) >> 5;
}
static inline uint16_t teleporter_rotation(uint16_t thing)
{
    return (REC(thing)[2] & 0x0c) >> 2;
}
static inline bool teleporter_is_rotation_absolute(uint16_t thing)
{
    return (REC(thing)[2] & 0x10) != 0;
}
static inline bool teleporter_is_audible(uint16_t thing)
{
    return (REC(thing)[2] & 0x80) != 0;
}

/* Door record (thing type 0). */
static inline int16_t door_type(uint16_t thing)
{
    return REC(thing)[3] & 1;
}
static inline int16_t door_breaks_by_magic(uint16_t thing)
{
    return (REC(thing)[3] & 0x80) != 0;
}
static inline int16_t door_breaks_by_force(uint16_t thing)
{
    return REC(thing)[2] & 1;
}
static inline int16_t door_button_present(uint16_t thing)
{
    return (REC(thing)[3] & 0x40) != 0;
}
/* A door that slides down; it hits a creature by its height. */
static inline bool door_is_vertical(uint16_t thing)
{
    return (REC(thing)[3] & 0x20) != 0;
}
static inline uint16_t door_ornament_ordinal(uint16_t thing)
{
    return (REC(thing)[3] & 0x1e) >> 1;
}
static inline int16_t door_info_defense(uint16_t door_info)
{
    return door_info & 0xff;
}

/* Text record (thing type 2). */
static inline bool rec_text_is_visible(uint16_t thing)
{
    return REC(thing)[3] & 1;
}
static inline void rec_text_set_visible(uint16_t thing, bool visible)
{
    REC(thing)[3] = (REC(thing)[3] & ~1) | (visible != 0);
}

/* Group record (thing type 4) and creature attributes. */
enum { CREATURE_LEVITATES = 0x20, CREATURE_NON_MATERIAL = 0x40 };
static inline uint16_t rec_group_creature_type(uint16_t thing)
{
    return REC(thing)[4];
}
static inline int16_t rec_creature_height(uint16_t attributes)
{
    return attributes >> 7 & 3;
}

/* Projectile record (thing type 14): the timeline event that moves it. */
static inline uint16_t rec_projectile_event_index(uint16_t thing)
{
    return BE16(REC(thing) + 6);
}

/* Map descriptor, 16 bytes. */
#define MAP_BYTES 16
enum {
    MAP_OFFSET_X_BYTE = 6,
    MAP_OFFSET_Y_BYTE,
    MAP_LAST_XY_BYTE,
    MAP_LEVEL_BYTE,
    MAP_FLOOR_ORNAMENTS_BYTE,
    MAP_WALL_ORNAMENTS_BYTE,
    MAP_DIFFICULTY_BYTE,
    MAP_DOOR_ORNAMENTS_CREATURES_BYTE,
    MAP_DOOR_SETS_BYTE,
    MAP_GRAPHIC_SETS_BYTE
};
#define MAP_DESCRIPTOR(map) (g_dungeon_maps + (map) * MAP_BYTES)
static inline int16_t map_offset_x(int16_t map)
{
    return MAP_DESCRIPTOR(map)[MAP_OFFSET_X_BYTE];
}
static inline int16_t map_offset_y(int16_t map)
{
    return MAP_DESCRIPTOR(map)[MAP_OFFSET_Y_BYTE];
}
static inline uint16_t map_level(int16_t map)
{
    return MAP_DESCRIPTOR(map)[MAP_LEVEL_BYTE] & 0x3f;
}
static inline int16_t map_last_x(int16_t map)
{
    return BE16(MAP_DESCRIPTOR(map) + MAP_LAST_XY_BYTE) >> 6 & MAP_COORD_MASK;
}
static inline int16_t map_last_y(int16_t map)
{
    return MAP_DESCRIPTOR(map)[MAP_LAST_XY_BYTE] >> 3;
}
static inline int16_t map_door_set(int16_t map, int16_t door_type)
{
    return door_type ? MAP_DESCRIPTOR(map)[MAP_DOOR_SETS_BYTE] >> 4 : MAP_DESCRIPTOR(map)[MAP_DOOR_SETS_BYTE] & 0x0f;
}

/* Dungeon header words: the size of the square first thing table, and the record count of each thing type. */
#define DUNGEON_HEADER_SQUARE_THINGS 10
#define DUNGEON_HEADER_THING_COUNTS  12
static inline uint16_t rec_dungeon_square_first_thing_count(void)
{
    return BE16(g_dungeon_header + DUNGEON_HEADER_SQUARE_THINGS);
}

extern int16_t g_current_map_index, g_current_map_width, g_current_map_height;
extern uint8_t **g_current_map_data;
extern uint8_t *g_current_map;              /* map descriptor, 16 bytes */
extern uint16_t g_current_map_door_info[2]; /* high byte attributes, low byte defense */

/* Door attributes (high byte of the door info of the current map): the door is see-through, projectiles pass. */
enum { DOOR_ATTRIBUTES_SHIFT = 8, DOOR_SEE_THROUGH = 1, DOOR_PROJECTILES_PASS = 2 };
static inline uint16_t door_attributes(uint16_t door)
{
    return g_current_map_door_info[door_type(door)] >> DOOR_ATTRIBUTES_SHIFT;
}
static inline bool door_is_see_through(uint16_t door)
{
    return door_attributes(door) & DOOR_SEE_THROUGH;
}

/* Map descriptor byte 12: difficulty in bits 4-7. */
enum { MAP_DIFFICULTY_SHIFT = 4 };
/* Difficulty of the current map, 0 when no map is loaded. */
static inline uint16_t map_current_difficulty(void)
{
    return g_current_map ? g_current_map[MAP_DIFFICULTY_BYTE] >> MAP_DIFFICULTY_SHIFT : 0;
}

void map_reset(void);
void set_current_map(int16_t map);
void set_current_map_and_party_map(int16_t map);

uint8_t get_square(int16_t x, int16_t y);
int16_t get_square_type(int16_t x, int16_t y);
bool is_square_open(int16_t x, int16_t y);
int16_t get_location_after_level_change(int16_t map, int16_t level_delta, int16_t *x, int16_t *y);
int16_t get_stairs_exit_direction(int16_t x, int16_t y);

uint16_t get_next_thing(uint16_t thing);
int16_t get_square_first_thing_index(int16_t x, int16_t y);
uint16_t get_square_first_thing(int16_t x, int16_t y);
uint16_t get_square_first_object(int16_t x, int16_t y);
void link_thing_to_list(uint16_t thing, uint16_t list, int16_t x, int16_t y);
void unlink_thing_from_list(uint16_t thing, uint16_t list, int16_t x, int16_t y);
uint16_t get_square_group(int16_t x, int16_t y);

uint16_t get_creature_attributes(uint16_t group_thing);

/* Record fields that the dungeon view reads (big-endian, see
   docs/symbols/range-4-6.md and range-7-9.md). */

/* Weapon record: word 2 bits 0-6 weapon type. */
static inline uint16_t thing_weapon_type(uint16_t weapon)
{
    return REC(weapon)[3] & 0x7f;
}

/* Group record, 16 bytes. */
#define GROUP_RECORD_BYTES 16
/* 2 bits per creature, 0xff for one centered creature; on the party map the active group index. */
static inline uint8_t rec_group_cells_byte(uint16_t group)
{
    return REC(group)[5];
}
enum { GROUP_DIRECTION_BYTE = 14, GROUP_BEHAVIOR_BYTE = 15 }; /* behavior and last creature index */
static inline uint8_t rec_group_last_creature_index(uint16_t group)
{
    return (REC(group)[GROUP_BEHAVIOR_BYTE] & 0x60) >> 5;
}
static inline uint8_t rec_group_direction(uint16_t group)
{
    return REC(group)[GROUP_DIRECTION_BYTE] & 3;
}

/* Projectile record, 8 bytes. */
static inline uint16_t rec_projectile_slot(uint16_t projectile)
{
    return BE16(REC(projectile) + 2);
}
static inline uint8_t rec_projectile_kinetic_energy(uint16_t projectile)
{
    return REC(projectile)[4];
}

/* Explosion record, 4 bytes. */
static inline uint8_t rec_explosion_attack(uint16_t explosion)
{
    return REC(explosion)[2];
}
static inline uint8_t rec_explosion_type(uint16_t explosion)
{
    return REC(explosion)[3] & 0x7f;
}
static inline bool rec_explosion_is_centered(uint16_t explosion)
{
    return REC(explosion)[3] >> 7;
}

/* Map descriptor bytes of the view: ornament counts, difficulty, creature types, graphic sets. */
static inline int16_t map_random_floor_ornament_count(int16_t map)
{
    return MAP_DESCRIPTOR(map)[MAP_FLOOR_ORNAMENTS_BYTE] >> 4;
}
static inline int16_t map_floor_ornament_count(int16_t map)
{
    return MAP_DESCRIPTOR(map)[MAP_FLOOR_ORNAMENTS_BYTE] & 0x0f;
}
static inline int16_t map_random_wall_ornament_count(int16_t map)
{
    return MAP_DESCRIPTOR(map)[MAP_WALL_ORNAMENTS_BYTE] >> 4;
}
static inline int16_t map_wall_ornament_count(int16_t map)
{
    return MAP_DESCRIPTOR(map)[MAP_WALL_ORNAMENTS_BYTE] & 0x0f;
}
static inline int16_t map_difficulty(int16_t map)
{
    return MAP_DESCRIPTOR(map)[MAP_DIFFICULTY_BYTE] >> MAP_DIFFICULTY_SHIFT;
}
static inline int16_t map_door_ornament_count(int16_t map)
{
    return MAP_DESCRIPTOR(map)[MAP_DOOR_ORNAMENTS_CREATURES_BYTE] & 0x0f;
}
static inline int16_t map_creature_type_count(int16_t map)
{
    return MAP_DESCRIPTOR(map)[MAP_DOOR_ORNAMENTS_CREATURES_BYTE] >> 4;
}
static inline int16_t map_floor_set(int16_t map)
{
    return MAP_DESCRIPTOR(map)[MAP_GRAPHIC_SETS_BYTE] & 0x0f;
}
static inline int16_t map_wall_set(int16_t map)
{
    return MAP_DESCRIPTOR(map)[MAP_GRAPHIC_SETS_BYTE] >> 4;
}

/* Dungeon header word 0: the seed of the random wall and floor ornaments. */
static inline uint16_t rec_dungeon_ornament_seed(void)
{
    return BE16(g_dungeon_header);
}

#endif
