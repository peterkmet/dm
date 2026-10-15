#ifndef PROJECTILE_H
#define PROJECTILE_H
/* Prefixes: projectile_ explosion_ */
#include <stdbool.h>
#include <stdint.h>
#include "game.h"
#include "map.h"
#include "timeline_process.h"

/* Projectiles, explosions, fluxcages, and launchers (KAOS range 7, PROJEXPL.C): docs/rules-creatures.md. */

/* Explosion types. The explosion thing of a type is 0xff80 + type. */
enum {
    EXPLOSION_FIREBALL = 0,
    EXPLOSION_SLIME = 1,
    EXPLOSION_LIGHTNING_BOLT = 2,
    EXPLOSION_HARM_NON_MATERIAL = 3,
    EXPLOSION_OPEN_DOOR = 4,
    EXPLOSION_POISON_BOLT = 6,
    EXPLOSION_POISON_CLOUD = 7,
    EXPLOSION_SMOKE = 40,
    EXPLOSION_FLUXCAGE = 50,
    EXPLOSION_REBIRTH_STEP_1 = 100,
    EXPLOSION_REBIRTH_STEP_2 = 101
};
#define EXPLOSION_THING_BASE  0xff80
#define EXPLOSION_THING(type) ((uint16_t)(EXPLOSION_THING_BASE + (type)))

/* Impact type of projectile_has_impact_occurred: a square type (wall, stairs, door, fake wall) or these values. */
enum { IMPACT_PARTY = -2, IMPACT_CREATURE = -1 };

/* An explosion cell of 0xff centers the explosion on the square. */
enum { CELL_CENTERED = 0xff };

/* Projectile launcher sensor types. */
enum {
    SENSOR_WALL_LAUNCHER_OBJECT = 7,
    SENSOR_WALL_LAUNCHER_EXPLOSION = 8,
    SENSOR_WALL_LAUNCHER_TWO_OBJECTS = 9,
    SENSOR_WALL_LAUNCHER_TWO_EXPLOSIONS = 10,
    SENSOR_WALL_LAUNCHER_SQUARE_OBJECT = 14,
    SENSOR_WALL_LAUNCHER_TWO_SQUARE_OBJECTS = 15
};
static inline bool projectile_is_launcher_sensor(uint16_t sensor)
{
    uint16_t type = rec_sensor_type(sensor);
    return (type >= SENSOR_WALL_LAUNCHER_OBJECT && type <= SENSOR_WALL_LAUNCHER_TWO_EXPLOSIONS) ||
           type == SENSOR_WALL_LAUNCHER_SQUARE_OBJECT || type == SENSOR_WALL_LAUNCHER_TWO_SQUARE_OBJECTS;
}

/* Projectile record, 8 bytes (thing type 14): next, slot (the flying thing), kinetic energy, attack, event index. */
static inline void projectile_set_slot(uint16_t projectile, uint16_t thing)
{
    WR16(REC(projectile) + 2, thing);
}
static inline uint8_t projectile_attack(uint16_t projectile)
{
    return REC(projectile)[5];
}
static inline void projectile_set_kinetic_energy(uint16_t projectile, uint8_t value)
{
    REC(projectile)[4] = value;
}
static inline void projectile_set_attack(uint16_t projectile, uint8_t value)
{
    REC(projectile)[5] = value;
}
static inline void projectile_set_event_index(uint16_t projectile, uint16_t index)
{
    WR16(REC(projectile) + 6, index);
}

/* Explosion record, 4 bytes (thing type 15): next, attack, type in bits 0-6, centered in bit 7. */
enum { EXPLOSION_RECORD_TYPE_MASK = 0x7f, EXPLOSION_RECORD_CENTERED = 0x80 };
static inline void explosion_set_attack(uint16_t explosion, uint8_t attack)
{
    REC(explosion)[2] = attack;
}
static inline void explosion_set_type(uint16_t explosion, uint16_t type)
{
    REC(explosion)
    [3] = (uint8_t)((REC(explosion)[3] & EXPLOSION_RECORD_CENTERED) | (type & EXPLOSION_RECORD_TYPE_MASK));
}
static inline void explosion_set_centered(uint16_t explosion, int16_t centered)
{
    REC(explosion)
    [3] = (uint8_t)((REC(explosion)[3] & EXPLOSION_RECORD_TYPE_MASK) | (centered ? EXPLOSION_RECORD_CENTERED : 0));
}

/* Projectile move event: b = projectile thing, c = x bits 11-15, y bits 6-10, direction bits 4-5, step energy bits 0-3. */
enum {
    PROJECTILE_EVENT_X_SHIFT = 11,
    PROJECTILE_EVENT_Y_SHIFT = 6,
    PROJECTILE_EVENT_XY_MASK = 31,
    PROJECTILE_EVENT_STEP_ENERGY_MASK = 15
};
static inline int16_t projectile_event_x(const TimelineEvent *event)
{
    return event->c >> PROJECTILE_EVENT_X_SHIFT;
}
static inline int16_t projectile_event_y(const TimelineEvent *event)
{
    return event->c >> PROJECTILE_EVENT_Y_SHIFT & PROJECTILE_EVENT_XY_MASK;
}
static inline uint16_t projectile_event_direction(const TimelineEvent *event)
{
    return event->c >> 4 & 3;
}
static inline uint16_t projectile_event_step_energy(const TimelineEvent *event)
{
    return event->c & PROJECTILE_EVENT_STEP_ENERGY_MASK;
}
static inline uint16_t projectile_event_c(int16_t x, int16_t y, uint16_t direction, uint16_t step_energy)
{
    return (x & PROJECTILE_EVENT_XY_MASK) << PROJECTILE_EVENT_X_SHIFT |
           (y & PROJECTILE_EVENT_XY_MASK) << PROJECTILE_EVENT_Y_SHIFT | (direction & 3) << 4 |
           (step_energy & PROJECTILE_EVENT_STEP_ENERGY_MASK);
}

extern int16_t g_create_launcher_projectile;
extern int16_t g_projectile_poison_attack, g_projectile_attack_type;
extern int16_t g_fuse_sequence_requested; /* the endgame step runs fuse_sequence when it is set */

void projectile_create(uint16_t thing, int16_t x, int16_t y, int16_t cell, int16_t direction, uint8_t kinetic_energy,
                       uint8_t attack, uint8_t step_energy);
void explosion_create(uint16_t explosion_thing, uint16_t attack, uint16_t x_combo, uint16_t y_combo, int16_t cell);
void projectile_delete_event(uint16_t projectile);
void projectile_delete(uint16_t projectile, uint16_t possessing_group, int16_t x, int16_t y);
uint16_t projectile_get_impact_attack(uint16_t projectile, uint16_t thing);
bool projectile_has_impact_occurred(int16_t impact_type, int16_t x_combo, int16_t y_combo, int16_t cell,
                                    uint16_t projectile);
int16_t projectile_get_impact_count(int16_t impact_type, int16_t x, int16_t y, int16_t cell);
void projectile_process_move_event(TimelineEvent *event);
void explosion_process_step_event(TimelineEvent *event);

bool projectile_is_fluxcage_on_square(int16_t x, int16_t y);
uint16_t projectile_get_lord_chaos_on_square(int16_t x, int16_t y);
void projectile_fluxcage_action(int16_t x, int16_t y);
void projectile_fuse_action(int16_t x, int16_t y);
void projectile_reset(void);

bool projectile_is_killed_by_impact(int16_t src_x, int16_t src_y, int16_t dst_x, int16_t dst_y, uint16_t thing);
void projectile_trigger_launcher(uint16_t sensor, const TimelineEvent *event);
uint16_t projectile_get_object_for_launcher_or_generator(int16_t object_info_index);
void projectile_shoot_from_champion(int16_t champion_index, uint16_t thing, uint8_t kinetic_energy, uint8_t attack,
                                    uint8_t step_energy);

#endif
