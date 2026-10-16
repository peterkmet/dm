/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "engine/record.h"
#include "data/bytes.h"
#include "engine/dungeon.h"
#include "engine/map.h"
#include <string.h>

/* Byte offsets of the fields of every record: the next thing of the list. */
#define THING_NEXT 0x00

/* Junk records that a new junk keeps free. */
#define JUNK_RESERVED_COUNT 3

/* Door record bytes and bits. */
#define DOOR_FLAGS          0x02
#define DOOR_ATTRIBUTES     0x03
#define DOOR_FORCE_BIT      0x01 /* in DOOR_FLAGS */
#define DOOR_TYPE_BIT       0x01 /* in DOOR_ATTRIBUTES */
#define DOOR_ORNAMENT_MASK  0x1e
#define DOOR_VERTICAL_BIT   0x20
#define DOOR_BUTTON_BIT     0x40
#define DOOR_MAGIC_BIT      0x80
/* Bit shifts of the door record. */
#define DOOR_ORNAMENT_SHIFT 1

/* Teleporter record bytes and bits. */
#define TELEPORTER_FLAGS          0x02
#define TELEPORTER_TARGET_X       0x03
#define TELEPORTER_TARGET_Y       0x02 /* word */
#define TELEPORTER_TARGET_MAP     0x04
#define TELEPORTER_ROTATION_MASK  0x0c
#define TELEPORTER_ABSOLUTE_BIT   0x10
#define TELEPORTER_SCOPE_MASK     0x60
#define TELEPORTER_AUDIBLE_BIT    0x80
/* Bit shifts of the teleporter record. */
#define TELEPORTER_ROTATION_SHIFT 2
#define TELEPORTER_SCOPE_SHIFT    5
#define TELEPORTER_TARGET_Y_SHIFT 5

/* Text record bytes and bits. */
#define TEXT_FLAGS        0x03
#define TEXT_OFFSET       0x02 /* word */
#define TEXT_VISIBLE_BIT  0x01
/* Bit shifts of the text record. */
#define TEXT_OFFSET_SHIFT 3

/* Sensor record bytes and bits. */
#define SENSOR_TYPE_AND_DATA      0x02 /* word */
#define SENSOR_FLAGS              0x04 /* byte, and the word of the delay */
#define SENSOR_ACTION             0x05
#define SENSOR_TARGET             0x06 /* word, and the byte of the target y */
#define SENSOR_TARGET_CELL        0x07
#define SENSOR_TYPE_MASK          0x7f
#define SENSOR_LOCAL_BIT          0x08
#define SENSOR_DELAY_MASK         0x0f
#define SENSOR_ONCE_ONLY_BIT      0x04
#define SENSOR_EFFECT_MASK        0x18
#define SENSOR_REVERT_BIT         0x20
#define SENSOR_AUDIBLE_BIT        0x40
#define SENSOR_MULTIPLIER_MASK    0x0f
#define SENSOR_CELL_MASK          0x30
#define SENSOR_CREATURE_MASK      0xff
/* Bit shifts of the sensor record. */
#define SENSOR_DATA_SHIFT         7
#define SENSOR_ORNAMENT_SHIFT     4
#define SENSOR_DELAY_SHIFT        7
#define SENSOR_EFFECT_SHIFT       3
#define SENSOR_REVERT_SHIFT       5
#define SENSOR_TARGET_X_SHIFT     6
#define SENSOR_TARGET_Y_SHIFT     3
#define SENSOR_LOCAL_EFFECT_SHIFT 4
#define SENSOR_MULTIPLIER_SHIFT   4
#define SENSOR_ENERGY_SHIFT       4
#define SENSOR_CELL_SHIFT         4

/* Group record bytes and bits. */
#define GROUP_POSSESSIONS         0x02 /* word */
#define GROUP_CREATURE_TYPE       0x04
#define GROUP_CELLS               0x05
#define GROUP_HEALTH              0x06 /* one word per creature */
#define GROUP_FLAGS               0x0e
#define GROUP_BEHAVIOR            0x0f
#define GROUP_DIRECTION_MASK      0x03 /* in GROUP_FLAGS */
#define GROUP_NO_DISCARD_BIT      0x04 /* in GROUP_FLAGS */
#define GROUP_BEHAVIOR_MASK       0x0f
#define GROUP_LAST_CREATURE_MASK  0x60 /* in GROUP_BEHAVIOR */
/* Bit shifts of the group record. */
#define GROUP_LAST_CREATURE_SHIFT 5

/* Object record bytes and bits. */
#define OBJECT_FLAGS         0x02 /* cursed, charges, broken, lit, closed */
#define OBJECT_TYPE          0x03 /* weapon, armour and junk type, important */
#define OBJECT_FIRST_THING   0x02 /* word: container first thing, scroll text */
#define POTION_TYPE          0x02
#define POTION_POWER         0x03
#define CONTAINER_FLAGS      0x05
#define OBJECT_TYPE_MASK     0x7f
#define OBJECT_IMPORTANT_BIT 0x80
#define OBJECT_CURSED_BIT    0x01
#define WEAPON_POISONED_BIT  0x02
#define WEAPON_CHARGES_MASK  0x3c
#define WEAPON_BROKEN_BIT    0x40
#define WEAPON_LIT_BIT       0x80
#define ARMOUR_CHARGES_MASK  0x1e
#define ARMOUR_BROKEN_BIT    0x20
#define JUNK_CHARGES_MASK    0xc0
#define SCROLL_CLOSED_MASK   0xfc
#define SCROLL_CLOSED_BIT    0x04
#define CONTAINER_TYPE_MASK  0x06
/* Word masks of the object records. */
#define SCROLL_TEXT_MASK     0x03ff
/* Bit shifts of the object records. */
#define WEAPON_CHARGES_SHIFT 2
#define ARMOUR_CHARGES_SHIFT 1
#define JUNK_CHARGES_SHIFT   6
#define CONTAINER_TYPE_SHIFT 1

/* Projectile record bytes. */
#define PROJECTILE_SLOT   0x02 /* word */
#define PROJECTILE_ENERGY 0x04
#define PROJECTILE_ATTACK 0x05
#define PROJECTILE_EVENT  0x06 /* word */

/* Explosion record bytes and bits. */
#define EXPLOSION_ATTACK       0x02
#define EXPLOSION_FLAGS        0x03
#define EXPLOSION_TYPE_MASK    0x7f
#define EXPLOSION_CENTERED_BIT 0x80

/* Bit shifts of the square byte. */
#define SQUARE_TYPE_SHIFT 5

/* Map descriptor bytes and bits. */
#define MAP_RAW_OFFSET        0x00 /* word */
#define MAP_OFFSET_X          0x06
#define MAP_OFFSET_Y          0x07
#define MAP_LAST_XY           0x08 /* word of the last x, byte of the last y */
#define MAP_LEVEL             0x09
#define MAP_FLOOR_ORNAMENTS   0x0a /* random count in the high nibble, count in the low nibble */
#define MAP_WALL_ORNAMENTS    0x0b /* random count in the high nibble, count in the low nibble */
#define MAP_DIFFICULTY        0x0c
#define MAP_DOOR_ORNAMENTS    0x0d /* creature types in the high nibble, door ornaments in the low nibble */
#define MAP_DOOR_SETS         0x0e /* set 1 in the high nibble, set 0 in the low nibble */
#define MAP_GRAPHIC_SETS      0x0f /* wall set in the high nibble, floor set in the low nibble */
#define MAP_LEVEL_MASK        0x3f
#define MAP_LOW_NIBBLE_MASK   0x0f
/* Bit shifts of the map descriptor. */
#define MAP_LAST_X_SHIFT      6
#define MAP_LAST_Y_SHIFT      3
#define MAP_HIGH_NIBBLE_SHIFT 4
#define MAP_DIFFICULTY_SHIFT  4

/* Dungeon header words. */
#define DUNGEON_ORNAMENT_SEED 0x00
#define DUNGEON_SQUARE_THINGS 0x0a
#define DUNGEON_THING_COUNTS  0x0c /* one word per thing type */

/* The value of the bits mask << shift of word. */
static uint16_t bits(uint16_t word, int16_t shift, uint16_t mask)
{
    return word >> shift & mask;
}

#define REC(thing) rec_write(thing)

/* Records of every thing type */

const uint8_t *rec_read(uint16_t thing)
{
    return rec_write(thing);
}

uint8_t *rec_write(uint16_t thing)
{
    uint16_t type = THING_TYPE(thing);
    return g_thing_data[type] + g_thing_data_byte_count[type] * THING_INDEX(thing);
}

uint16_t rec_thing_next(uint16_t thing)
{
    return be16(rec_read(thing) + THING_NEXT);
}
void rec_thing_set_next(uint16_t thing, uint16_t value)
{
    wr16(REC(thing) + THING_NEXT, value);
}
void rec_thing_set_free(uint16_t thing)
{
    wr16(REC(thing) + THING_NEXT, THING_NONE);
}

uint16_t rec_find_unused_thing(uint16_t thing_type)
{
    uint16_t type = thing_type & ~NO_JUNK_RESERVE;
    uint16_t size = g_thing_data_byte_count[type];
    int16_t count = rec_dungeon_thing_count(type);
    if (thing_type == THING_JUNK)
        count -= JUNK_RESERVED_COUNT;
    for (int16_t i = 0; i < count; i++) {
        uint8_t *record = g_thing_data[type] + i * size;
        if (be16(record) == THING_NONE) {
            memset(record, 0, size);
            wr16(record, THING_ENDOFLIST);
            return THING_MAKE(type, i);
        }
    }
    return THING_NONE;
}

/* Door record */

uint16_t rec_door_type(uint16_t thing)
{
    return rec_read(thing)[DOOR_ATTRIBUTES] & DOOR_TYPE_BIT;
}
bool rec_door_is_magic_breakable(uint16_t thing)
{
    return (rec_read(thing)[DOOR_ATTRIBUTES] & DOOR_MAGIC_BIT) != 0;
}
bool rec_door_is_force_breakable(uint16_t thing)
{
    return rec_read(thing)[DOOR_FLAGS] & DOOR_FORCE_BIT;
}
bool rec_door_has_button(uint16_t thing)
{
    return (rec_read(thing)[DOOR_ATTRIBUTES] & DOOR_BUTTON_BIT) != 0;
}
bool rec_door_is_vertical(uint16_t thing)
{
    return (rec_read(thing)[DOOR_ATTRIBUTES] & DOOR_VERTICAL_BIT) != 0;
}
int16_t rec_door_ornament(uint16_t thing)
{
    return (int16_t)((rec_read(thing)[DOOR_ATTRIBUTES] & DOOR_ORNAMENT_MASK) >> DOOR_ORNAMENT_SHIFT) - 1;
}

/* Teleporter record */

int16_t rec_teleporter_target_x(uint16_t thing)
{
    return rec_read(thing)[TELEPORTER_TARGET_X] & MAP_COORD_MASK;
}
int16_t rec_teleporter_target_y(uint16_t thing)
{
    return bits(be16(rec_read(thing) + TELEPORTER_TARGET_Y), TELEPORTER_TARGET_Y_SHIFT, MAP_COORD_MASK);
}
int16_t rec_teleporter_target_map(uint16_t thing)
{
    return rec_read(thing)[TELEPORTER_TARGET_MAP];
}
uint16_t rec_teleporter_scope(uint16_t thing)
{
    return (rec_read(thing)[TELEPORTER_FLAGS] & TELEPORTER_SCOPE_MASK) >> TELEPORTER_SCOPE_SHIFT;
}
uint16_t rec_teleporter_rotation(uint16_t thing)
{
    return (rec_read(thing)[TELEPORTER_FLAGS] & TELEPORTER_ROTATION_MASK) >> TELEPORTER_ROTATION_SHIFT;
}
bool rec_teleporter_is_absolute(uint16_t thing)
{
    return (rec_read(thing)[TELEPORTER_FLAGS] & TELEPORTER_ABSOLUTE_BIT) != 0;
}
bool rec_teleporter_is_audible(uint16_t thing)
{
    return (rec_read(thing)[TELEPORTER_FLAGS] & TELEPORTER_AUDIBLE_BIT) != 0;
}

/* Text record */

bool rec_text_is_visible(uint16_t thing)
{
    return rec_read(thing)[TEXT_FLAGS] & TEXT_VISIBLE_BIT;
}
void rec_text_set_visible(uint16_t thing, bool value)
{
    REC(thing)[TEXT_FLAGS] = (REC(thing)[TEXT_FLAGS] & ~TEXT_VISIBLE_BIT) | (value != 0);
}
uint16_t rec_text_offset(uint16_t thing)
{
    return be16(rec_read(thing) + TEXT_OFFSET) >> TEXT_OFFSET_SHIFT;
}

/* Sensor record */

uint16_t rec_sensor_type(uint16_t thing)
{
    return be16(rec_read(thing) + SENSOR_TYPE_AND_DATA) & SENSOR_TYPE_MASK;
}
uint16_t rec_sensor_data(uint16_t thing)
{
    return be16(rec_read(thing) + SENSOR_TYPE_AND_DATA) >> SENSOR_DATA_SHIFT;
}
void rec_sensor_set_type(uint16_t thing, uint16_t value)
{
    wr16(REC(thing) + SENSOR_TYPE_AND_DATA, (be16(REC(thing) + SENSOR_TYPE_AND_DATA) & ~SENSOR_TYPE_MASK) | value);
}
void rec_sensor_set_data(uint16_t thing, uint16_t value)
{
    wr16(REC(thing) + SENSOR_TYPE_AND_DATA, value << SENSOR_DATA_SHIFT | rec_sensor_type(thing));
}
bool rec_sensor_is_local(uint16_t thing)
{
    return (rec_read(thing)[SENSOR_FLAGS] & SENSOR_LOCAL_BIT) != 0;
}
uint16_t rec_sensor_delay(uint16_t thing)
{
    return bits(be16(rec_read(thing) + SENSOR_FLAGS), SENSOR_DELAY_SHIFT, SENSOR_DELAY_MASK);
}
bool rec_sensor_is_once_only(uint16_t thing)
{
    return (rec_read(thing)[SENSOR_ACTION] & SENSOR_ONCE_ONLY_BIT) != 0;
}
uint16_t rec_sensor_effect(uint16_t thing)
{
    return (rec_read(thing)[SENSOR_ACTION] & SENSOR_EFFECT_MASK) >> SENSOR_EFFECT_SHIFT;
}
bool rec_sensor_is_reverted(uint16_t thing)
{
    return (rec_read(thing)[SENSOR_ACTION] & SENSOR_REVERT_BIT) >> SENSOR_REVERT_SHIFT;
}
bool rec_sensor_is_audible(uint16_t thing)
{
    return (rec_read(thing)[SENSOR_ACTION] & SENSOR_AUDIBLE_BIT) != 0;
}
int16_t rec_sensor_target_x(uint16_t thing)
{
    return bits(be16(rec_read(thing) + SENSOR_TARGET), SENSOR_TARGET_X_SHIFT, MAP_COORD_MASK);
}
int16_t rec_sensor_target_y(uint16_t thing)
{
    return rec_read(thing)[SENSOR_TARGET] >> SENSOR_TARGET_Y_SHIFT;
}
int16_t rec_sensor_target_cell(uint16_t thing)
{
    return (rec_read(thing)[SENSOR_TARGET_CELL] & SENSOR_CELL_MASK) >> SENSOR_CELL_SHIFT;
}
int16_t rec_sensor_local_effect(uint16_t thing)
{
    return be16(rec_read(thing) + SENSOR_TARGET) >> SENSOR_LOCAL_EFFECT_SHIFT;
}
uint16_t rec_sensor_generator_delay(uint16_t thing)
{
    return rec_read(thing)[SENSOR_TARGET];
}
int16_t rec_sensor_ornament(uint16_t thing)
{
    return (int16_t)(rec_read(thing)[SENSOR_FLAGS] >> SENSOR_ORNAMENT_SHIFT) - 1;
}
uint16_t rec_sensor_creature_type(uint16_t thing)
{
    return rec_sensor_data(thing) & SENSOR_CREATURE_MASK;
}
uint16_t rec_sensor_generator_count(uint16_t thing)
{
    return bits(be16(rec_read(thing) + SENSOR_FLAGS), SENSOR_DELAY_SHIFT, SENSOR_DELAY_MASK);
}
uint16_t rec_sensor_health_multiplier(uint16_t thing)
{
    return bits(be16(rec_read(thing) + SENSOR_TARGET), SENSOR_MULTIPLIER_SHIFT, SENSOR_MULTIPLIER_MASK);
}
uint8_t rec_sensor_kinetic_energy(uint16_t thing)
{
    return be16(rec_read(thing) + SENSOR_TARGET) >> SENSOR_ENERGY_SHIFT;
}
uint8_t rec_sensor_step_energy(uint16_t thing)
{
    return rec_read(thing)[SENSOR_TARGET] >> SENSOR_ENERGY_SHIFT;
}

/* Group record */

uint16_t rec_group_possessions(uint16_t thing)
{
    return be16(rec_read(thing) + GROUP_POSSESSIONS);
}
void rec_group_set_possessions(uint16_t thing, uint16_t value)
{
    wr16(REC(thing) + GROUP_POSSESSIONS, value);
}
uint16_t rec_group_creature_type(uint16_t thing)
{
    return rec_read(thing)[GROUP_CREATURE_TYPE];
}
void rec_group_set_creature_type(uint16_t thing, uint16_t value)
{
    REC(thing)[GROUP_CREATURE_TYPE] = value;
}
uint8_t rec_group_cells(uint16_t thing)
{
    return rec_read(thing)[GROUP_CELLS];
}
void rec_group_set_cells(uint16_t thing, uint8_t value)
{
    REC(thing)[GROUP_CELLS] = value;
}
uint16_t rec_group_health(uint16_t group, int16_t creature)
{
    return be16(rec_read(group) + GROUP_HEALTH + 2 * creature);
}
void rec_group_set_health(uint16_t group, int16_t creature, uint16_t health)
{
    wr16(REC(group) + GROUP_HEALTH + 2 * creature, health);
}
uint16_t rec_group_behavior(uint16_t thing)
{
    return rec_read(thing)[GROUP_BEHAVIOR] & GROUP_BEHAVIOR_MASK;
}
void rec_group_set_behavior(uint16_t thing, uint16_t value)
{
    REC(thing)[GROUP_BEHAVIOR] = (REC(thing)[GROUP_BEHAVIOR] & ~GROUP_BEHAVIOR_MASK) | value;
}
uint8_t rec_group_last_creature(uint16_t thing)
{
    return (rec_read(thing)[GROUP_BEHAVIOR] & GROUP_LAST_CREATURE_MASK) >> GROUP_LAST_CREATURE_SHIFT;
}
void rec_group_set_last_creature(uint16_t thing, uint8_t value)
{
    REC(thing)
    [GROUP_BEHAVIOR] = (REC(thing)[GROUP_BEHAVIOR] & ~GROUP_LAST_CREATURE_MASK) |
                       (value << GROUP_LAST_CREATURE_SHIFT & GROUP_LAST_CREATURE_MASK);
}
uint8_t rec_group_direction(uint16_t thing)
{
    return rec_read(thing)[GROUP_FLAGS] & GROUP_DIRECTION_MASK;
}
void rec_group_set_direction(uint16_t thing, uint8_t value)
{
    REC(thing)[GROUP_FLAGS] = (REC(thing)[GROUP_FLAGS] & ~GROUP_DIRECTION_MASK) | (value & GROUP_DIRECTION_MASK);
}
void rec_group_set_no_discard(uint16_t thing, bool value)
{
    uint8_t *d = REC(thing);
    d[GROUP_FLAGS] = value ? d[GROUP_FLAGS] | GROUP_NO_DISCARD_BIT : d[GROUP_FLAGS] & ~GROUP_NO_DISCARD_BIT;
}

/* Object records */

bool rec_object_is_cursed(uint16_t thing)
{
    return rec_read(thing)[OBJECT_FLAGS] & OBJECT_CURSED_BIT;
}
void rec_object_set_cursed(uint16_t thing, bool value)
{
    uint8_t *d = REC(thing);
    d[OBJECT_FLAGS] = (d[OBJECT_FLAGS] & ~OBJECT_CURSED_BIT) | (value != 0);
}
void rec_object_set_important(uint16_t thing, bool important)
{
    uint8_t *d = REC(thing);
    d[OBJECT_TYPE] = important ? d[OBJECT_TYPE] | OBJECT_IMPORTANT_BIT : d[OBJECT_TYPE] & ~OBJECT_IMPORTANT_BIT;
}
void rec_object_set_type(uint16_t thing, uint16_t value)
{
    uint8_t *d = REC(thing);
    d[OBJECT_TYPE] = (d[OBJECT_TYPE] & OBJECT_IMPORTANT_BIT) | (value & OBJECT_TYPE_MASK);
}

/* Weapon record */

uint16_t rec_weapon_type(uint16_t thing)
{
    return rec_read(thing)[OBJECT_TYPE] & OBJECT_TYPE_MASK;
}
bool rec_weapon_is_lit(uint16_t thing)
{
    return (rec_read(thing)[OBJECT_FLAGS] & WEAPON_LIT_BIT) != 0;
}
void rec_weapon_set_lit(uint16_t thing, bool value)
{
    uint8_t *d = REC(thing);
    d[OBJECT_FLAGS] = value ? d[OBJECT_FLAGS] | WEAPON_LIT_BIT : d[OBJECT_FLAGS] & ~WEAPON_LIT_BIT;
}
uint16_t rec_weapon_charges(uint16_t thing)
{
    return (rec_read(thing)[OBJECT_FLAGS] & WEAPON_CHARGES_MASK) >> WEAPON_CHARGES_SHIFT;
}
void rec_weapon_set_charges(uint16_t thing, uint16_t value)
{
    uint8_t *d = REC(thing);
    d[OBJECT_FLAGS] = (d[OBJECT_FLAGS] & ~WEAPON_CHARGES_MASK) | (value << WEAPON_CHARGES_SHIFT & WEAPON_CHARGES_MASK);
}
bool rec_weapon_is_poisoned(uint16_t thing)
{
    return (rec_read(thing)[OBJECT_FLAGS] & WEAPON_POISONED_BIT) != 0;
}
bool rec_weapon_is_broken(uint16_t thing)
{
    return (rec_read(thing)[OBJECT_FLAGS] & WEAPON_BROKEN_BIT) != 0;
}

/* Armour record */

uint16_t rec_armour_type(uint16_t thing)
{
    return rec_read(thing)[OBJECT_TYPE] & OBJECT_TYPE_MASK;
}
uint16_t rec_armour_charges(uint16_t thing)
{
    return (rec_read(thing)[OBJECT_FLAGS] & ARMOUR_CHARGES_MASK) >> ARMOUR_CHARGES_SHIFT;
}
void rec_armour_set_charges(uint16_t thing, uint16_t value)
{
    uint8_t *d = REC(thing);
    d[OBJECT_FLAGS] = (d[OBJECT_FLAGS] & ~ARMOUR_CHARGES_MASK) | (value << ARMOUR_CHARGES_SHIFT & ARMOUR_CHARGES_MASK);
}
bool rec_armour_is_broken(uint16_t thing)
{
    return (rec_read(thing)[OBJECT_FLAGS] & ARMOUR_BROKEN_BIT) != 0;
}

/* Scroll record */

bool rec_scroll_is_closed(uint16_t thing)
{
    return (rec_read(thing)[OBJECT_FLAGS] & SCROLL_CLOSED_MASK) != 0;
}
void rec_scroll_set_closed(uint16_t thing, bool value)
{
    uint8_t *d = REC(thing);
    d[OBJECT_FLAGS] = (d[OBJECT_FLAGS] & ~SCROLL_CLOSED_MASK) | (value ? SCROLL_CLOSED_BIT : 0);
}
uint16_t rec_scroll_text_index(uint16_t thing)
{
    return be16(rec_read(thing) + OBJECT_FIRST_THING) & SCROLL_TEXT_MASK;
}

/* Potion record */

uint16_t rec_potion_type(uint16_t thing)
{
    return rec_read(thing)[POTION_TYPE] & OBJECT_TYPE_MASK;
}
void rec_potion_set_type(uint16_t thing, uint16_t value)
{
    uint8_t *d = REC(thing);
    d[POTION_TYPE] = (d[POTION_TYPE] & ~OBJECT_TYPE_MASK) | value;
}
uint16_t rec_potion_power(uint16_t thing)
{
    return rec_read(thing)[POTION_POWER];
}
void rec_potion_set_power(uint16_t thing, uint16_t value)
{
    REC(thing)[POTION_POWER] = value;
}

/* Container record */

uint16_t rec_container_type(uint16_t thing)
{
    return (rec_read(thing)[CONTAINER_FLAGS] & CONTAINER_TYPE_MASK) >> CONTAINER_TYPE_SHIFT;
}
uint16_t rec_container_first_thing(uint16_t thing)
{
    return be16(rec_read(thing) + OBJECT_FIRST_THING);
}
void rec_container_set_first_thing(uint16_t thing, uint16_t value)
{
    wr16(REC(thing) + OBJECT_FIRST_THING, value);
}

/* Junk record */

uint16_t rec_junk_type(uint16_t thing)
{
    return rec_read(thing)[OBJECT_TYPE] & OBJECT_TYPE_MASK;
}
void rec_junk_set_type(uint16_t thing, uint16_t value)
{
    uint8_t *d = REC(thing);
    d[OBJECT_TYPE] = (d[OBJECT_TYPE] & OBJECT_IMPORTANT_BIT) | value;
}
uint16_t rec_junk_charges(uint16_t thing)
{
    return rec_read(thing)[OBJECT_FLAGS] >> JUNK_CHARGES_SHIFT;
}
void rec_junk_set_charges(uint16_t thing, uint16_t value)
{
    uint8_t *d = REC(thing);
    d[OBJECT_FLAGS] = (d[OBJECT_FLAGS] & ~JUNK_CHARGES_MASK) | value << JUNK_CHARGES_SHIFT;
}

/* Projectile record */

uint16_t rec_projectile_slot(uint16_t thing)
{
    return be16(rec_read(thing) + PROJECTILE_SLOT);
}
void rec_projectile_set_slot(uint16_t thing, uint16_t value)
{
    wr16(REC(thing) + PROJECTILE_SLOT, value);
}
uint8_t rec_projectile_energy(uint16_t thing)
{
    return rec_read(thing)[PROJECTILE_ENERGY];
}
void rec_projectile_set_energy(uint16_t thing, uint8_t value)
{
    REC(thing)[PROJECTILE_ENERGY] = value;
}
uint8_t rec_projectile_attack(uint16_t thing)
{
    return rec_read(thing)[PROJECTILE_ATTACK];
}
void rec_projectile_set_attack(uint16_t thing, uint8_t value)
{
    REC(thing)[PROJECTILE_ATTACK] = value;
}
uint16_t rec_projectile_event_index(uint16_t thing)
{
    return be16(rec_read(thing) + PROJECTILE_EVENT);
}
void rec_projectile_set_event_index(uint16_t thing, uint16_t value)
{
    wr16(REC(thing) + PROJECTILE_EVENT, value);
}

/* Explosion record */

uint8_t rec_explosion_attack(uint16_t thing)
{
    return rec_read(thing)[EXPLOSION_ATTACK];
}
void rec_explosion_set_attack(uint16_t thing, uint8_t value)
{
    REC(thing)[EXPLOSION_ATTACK] = value;
}
uint8_t rec_explosion_type(uint16_t thing)
{
    return rec_read(thing)[EXPLOSION_FLAGS] & EXPLOSION_TYPE_MASK;
}
void rec_explosion_set_type(uint16_t thing, uint8_t value)
{
    REC(thing)
    [EXPLOSION_FLAGS] = (REC(thing)[EXPLOSION_FLAGS] & EXPLOSION_CENTERED_BIT) | (value & EXPLOSION_TYPE_MASK);
}
bool rec_explosion_is_centered(uint16_t thing)
{
    return (rec_read(thing)[EXPLOSION_FLAGS] & EXPLOSION_CENTERED_BIT) != 0;
}
void rec_explosion_set_centered(uint16_t thing, bool value)
{
    REC(thing)
    [EXPLOSION_FLAGS] = (REC(thing)[EXPLOSION_FLAGS] & EXPLOSION_TYPE_MASK) | (value ? EXPLOSION_CENTERED_BIT : 0);
}

/* Squares */

uint8_t rec_square_byte(int16_t map, int16_t x, int16_t y)
{
    return g_dungeon_map_data[map][x][y];
}
void rec_square_set_byte(int16_t map, int16_t x, int16_t y, uint8_t square)
{
    g_dungeon_map_data[map][x][y] = square;
}
int16_t rec_square_type(uint8_t square)
{
    return square >> SQUARE_TYPE_SHIFT;
}
int16_t rec_door_state(uint8_t square)
{
    return square & DOOR_STATE_MASK;
}
void rec_door_set_state(int16_t map, int16_t x, int16_t y, int16_t state)
{
    uint8_t square = rec_square_byte(map, x, y);
    rec_square_set_byte(map, x, y, (square & ~DOOR_STATE_MASK) | (state & DOOR_STATE_MASK));
}

/* Map descriptor */

const uint8_t *rec_map_descriptor(int16_t map)
{
    return g_dungeon_maps + map * MAP_BYTES;
}
uint16_t rec_map_raw_offset(int16_t map)
{
    return be16(rec_map_descriptor(map) + MAP_RAW_OFFSET);
}
int16_t rec_map_offset_x(int16_t map)
{
    return rec_map_descriptor(map)[MAP_OFFSET_X];
}
int16_t rec_map_offset_y(int16_t map)
{
    return rec_map_descriptor(map)[MAP_OFFSET_Y];
}
uint16_t rec_map_level(int16_t map)
{
    return rec_map_descriptor(map)[MAP_LEVEL] & MAP_LEVEL_MASK;
}
int16_t rec_map_last_x(int16_t map)
{
    return bits(be16(rec_map_descriptor(map) + MAP_LAST_XY), MAP_LAST_X_SHIFT, MAP_COORD_MASK);
}
int16_t rec_map_last_y(int16_t map)
{
    return rec_map_descriptor(map)[MAP_LAST_XY] >> MAP_LAST_Y_SHIFT;
}
int16_t rec_map_door_set(int16_t map, int16_t door_type)
{
    uint8_t sets = rec_map_descriptor(map)[MAP_DOOR_SETS];
    return door_type ? sets >> MAP_HIGH_NIBBLE_SHIFT : sets & MAP_LOW_NIBBLE_MASK;
}
int16_t rec_map_random_floor_ornaments(int16_t map)
{
    return rec_map_descriptor(map)[MAP_FLOOR_ORNAMENTS] >> MAP_HIGH_NIBBLE_SHIFT;
}
int16_t rec_map_floor_ornaments(int16_t map)
{
    return rec_map_descriptor(map)[MAP_FLOOR_ORNAMENTS] & MAP_LOW_NIBBLE_MASK;
}
int16_t rec_map_random_wall_ornaments(int16_t map)
{
    return rec_map_descriptor(map)[MAP_WALL_ORNAMENTS] >> MAP_HIGH_NIBBLE_SHIFT;
}
int16_t rec_map_wall_ornaments(int16_t map)
{
    return rec_map_descriptor(map)[MAP_WALL_ORNAMENTS] & MAP_LOW_NIBBLE_MASK;
}
int16_t rec_map_difficulty(int16_t map)
{
    return rec_map_descriptor(map)[MAP_DIFFICULTY] >> MAP_DIFFICULTY_SHIFT;
}
int16_t rec_map_door_ornaments(int16_t map)
{
    return rec_map_descriptor(map)[MAP_DOOR_ORNAMENTS] & MAP_LOW_NIBBLE_MASK;
}
int16_t rec_map_creature_types(int16_t map)
{
    return rec_map_descriptor(map)[MAP_DOOR_ORNAMENTS] >> MAP_HIGH_NIBBLE_SHIFT;
}
int16_t rec_map_floor_set(int16_t map)
{
    return rec_map_descriptor(map)[MAP_GRAPHIC_SETS] & MAP_LOW_NIBBLE_MASK;
}
int16_t rec_map_wall_set(int16_t map)
{
    return rec_map_descriptor(map)[MAP_GRAPHIC_SETS] >> MAP_HIGH_NIBBLE_SHIFT;
}
const uint8_t *rec_map_type_lists(int16_t map)
{
    return &g_dungeon_map_data[map][rec_map_last_x(map)][rec_map_last_y(map) + 1];
}
uint16_t rec_current_map_difficulty(void)
{
    return g_current_map ? g_current_map[MAP_DIFFICULTY] >> MAP_DIFFICULTY_SHIFT : 0;
}

/* Dungeon header */

uint16_t rec_dungeon_ornament_seed(void)
{
    return be16(g_dungeon_header + DUNGEON_ORNAMENT_SEED);
}
uint16_t rec_dungeon_square_things(void)
{
    return be16(g_dungeon_header + DUNGEON_SQUARE_THINGS);
}
void rec_dungeon_set_square_things(uint16_t count)
{
    wr16(g_dungeon_header + DUNGEON_SQUARE_THINGS, count);
}
uint16_t rec_dungeon_thing_count(int16_t type)
{
    return be16(g_dungeon_header + DUNGEON_THING_COUNTS + type * 2);
}
void rec_dungeon_set_thing_count(int16_t type, uint16_t count)
{
    wr16(g_dungeon_header + DUNGEON_THING_COUNTS + type * 2, count);
}
