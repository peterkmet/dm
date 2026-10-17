/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_ENGINE_PROJECTILE_H
#define DM_ENGINE_PROJECTILE_H
/* Prefixes: projectile_ explosion_ */

/* Projectiles, explosions, fluxcages, and launchers (KAOS range 7, PROJEXPL.C): docs/rules/creatures.md. */

#include "engine/group.h"
#include "engine/map.h"
#include "engine/record.h"
#include "engine/timeline.h"
#include <stdbool.h>
#include <stdint.h>

/* Explosion types. The explosion thing of a type is 0xff80 + type. */
#define EXPLOSION_FIREBALL          0
#define EXPLOSION_SLIME             1
#define EXPLOSION_LIGHTNING_BOLT    2
#define EXPLOSION_HARM_NON_MATERIAL 3
#define EXPLOSION_OPEN_DOOR         4
#define EXPLOSION_POISON_BOLT       6
#define EXPLOSION_POISON_CLOUD      7
#define EXPLOSION_SMOKE             40
#define EXPLOSION_FLUXCAGE          50
#define EXPLOSION_REBIRTH_STEP_1    100
#define EXPLOSION_REBIRTH_STEP_2    101

/* First thing value of an explosion. */
#define EXPLOSION_THING_BASE  0xff80
#define EXPLOSION_THING(type) ((uint16_t)(EXPLOSION_THING_BASE + (type)))

/* Impact type of an impact test: a square type (wall, stairs, door, fake wall) or these values. */
#define IMPACT_PARTY    (-2)
#define IMPACT_CREATURE (-1)

/* Kinetic energy of a projectile: one byte. */
#define KINETIC_ENERGY_MAXIMUM       255
/* Attack of the harm non-material explosion of the Fuse action, and of the last endgame fuse explosion. */
#define FUSE_ACTION_ATTACK           255
/* A shot or a throw stops the party movement for this many ticks. */
#define SHOT_DISABLED_MOVEMENT_TICKS 4

/* Projectile launcher sensor types. */
#define SENSOR_WALL_LAUNCHER_OBJECT             7
#define SENSOR_WALL_LAUNCHER_EXPLOSION          8
#define SENSOR_WALL_LAUNCHER_TWO_EXPLOSIONS     10
#define SENSOR_WALL_LAUNCHER_SQUARE_OBJECT      14
#define SENSOR_WALL_LAUNCHER_TWO_SQUARE_OBJECTS 15
static inline bool projectile_is_launcher_sensor(uint16_t sensor)
{
    uint16_t type = rec_sensor_type(sensor);
    return (type >= SENSOR_WALL_LAUNCHER_OBJECT && type <= SENSOR_WALL_LAUNCHER_TWO_EXPLOSIONS) ||
           type == SENSOR_WALL_LAUNCHER_SQUARE_OBJECT || type == SENSOR_WALL_LAUNCHER_TWO_SQUARE_OBJECTS;
}

/* Projectile move event: b = projectile thing, c = x bits 11-15, y bits 6-10, direction bits 4-5, step energy bits 0-3. */
#define PROJECTILE_EVENT_X_SHIFT         11
#define PROJECTILE_EVENT_Y_SHIFT         6
#define PROJECTILE_EVENT_DIRECTION_SHIFT 4

/* Bit masks of the fields of a projectile event word. */
#define PROJECTILE_EVENT_XY_MASK          0x1f
#define PROJECTILE_EVENT_STEP_ENERGY_MASK 0x0f
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
    return event->c >> PROJECTILE_EVENT_DIRECTION_SHIFT & 3;
}
static inline uint16_t projectile_event_step_energy(const TimelineEvent *event)
{
    return event->c & PROJECTILE_EVENT_STEP_ENERGY_MASK;
}
static inline uint16_t projectile_event_c(int16_t x, int16_t y, int16_t direction, uint16_t step_energy)
{
    return (x & PROJECTILE_EVENT_XY_MASK) << PROJECTILE_EVENT_X_SHIFT |
           (y & PROJECTILE_EVENT_XY_MASK) << PROJECTILE_EVENT_Y_SHIFT |
           (direction & 3) << PROJECTILE_EVENT_DIRECTION_SHIFT | (step_energy & PROJECTILE_EVENT_STEP_ENERGY_MASK);
}

/* The endgame step runs startend_run_fuse_sequence when it is set (exported for the tests). */
extern bool g_fuse_sequence_requested;
/* No fuse request carries into a new game. */
void projectile_reset(void);
/* True once when the fuse action or sensor asked for the fuse sequence; the request is then cleared. */
bool projectile_take_fuse_request(void);

/* A new projectile: the flying thing, its square, cell and direction, its energies, and whether a launcher
   made it (the first move then ignores impacts). */
typedef struct {
    uint16_t thing;
    Location at;
    int16_t cell, direction;
    uint8_t kinetic_energy, attack, step_energy;
    bool from_launcher;
} ProjectileLaunch;
/* The attack of a projectile hit: damage, attack type and poison. */
typedef struct {
    uint16_t attack;
    int16_t type; /* ATTACK_* */
    int16_t poison;
} Impact;

/* A flying thing enters a square. Without a free projectile record, the object drops to the square. */
void projectile_create(const ProjectileLaunch *launch);
/* An explosion appears at a square; fireball and lightning damage aim at the target square (the same square
   for a plain explosion). */
void explosion_create(uint16_t explosion_thing, int16_t attack, Location at, Location target, int16_t cell);
/* The attack of a flying thing when it hits. */
Impact projectile_get_impact_attack(uint16_t projectile, uint16_t thing); /* exported for the tests */
/* The flight direction of a projectile (from its move event). */
int16_t projectile_get_flight_direction(uint16_t projectile);
/* The projectile ends: the flying object drops to the square, or joins the possessions of the group that caught it
   (possessing_group is THING_NONE for the square). */
void projectile_delete(uint16_t projectile, uint16_t possessing_group, int16_t x, int16_t y);
/* The move event of the projectile leaves the timeline. */
void projectile_delete_event(uint16_t projectile);
/* The square and cell that an impact test looks at. */
typedef struct {
    int16_t type;
    Location at;
    Location target;
    int16_t cell;
} ImpactSite;
/* The hits of every projectile in a cell; a creature impact stops when the whole group died. */
typedef struct {
    int16_t count;
    DamageOutcome outcome;
} ImpactCount;
/* Every projectile in a cell of a square hits the impact target; returns the hit count and the damage outcome. */
ImpactCount projectile_get_impact_count(int16_t impact_type, int16_t x, int16_t y, int16_t cell);
/* Moves a projectile one cell forward: within the square, or into the next square. Each step outside the first one
   tests the party and the creatures on the square and costs kinetic energy and attack. */
void projectile_process_move_event(TimelineEvent *event);
/* One explosion step: poison clouds and smoke fade over several events, a rebirth explosion becomes its second
   step, harm non-material hurts non-material creatures, fireball and lightning break doors. Then it disappears. */
void explosion_process_step_event(TimelineEvent *event);

/* A fluxcage for 100 ticks. When it closes the third side around an adjacent Lord Chaos, he senses danger. */
void projectile_fluxcage_action(int16_t x, int16_t y);
/* Fuse: a harm non-material explosion. Lord Chaos in 4 fluxcages starts the fuse sequence; otherwise he escapes
   through a free side to an open square. */
void projectile_fuse_action(int16_t x, int16_t y);

/* Projectiles in the cells of the party (thing 0xffff) or of a group hit before it moves. A move to an adjacent
   square also tests the projectiles on the destination that the movers cross. Returns true when the group died. */
bool projectile_is_killed_by_impact(int16_t src_x, int16_t src_y, int16_t dst_x, int16_t dst_y, uint16_t thing);
/* A wall launcher (sensor 7-10, 14, 15) shoots one or two objects or explosions from the square in front of it. */
void projectile_trigger_launcher(uint16_t sensor, const TimelineEvent *event);
/* A new weapon or junk object for an object info index: torches, daggers, arrows, slayers, rocks, poison darts,
   throwing stars, boulders. THING_NONE for other indices. */
uint16_t projectile_get_launch_object(int16_t object_info_index); /* exported for the tests */
/* A champion throws or shoots from the front cell of his side; the party cannot move into it for 4 ticks. */
void projectile_shoot_from_champion(int16_t champion_index, uint16_t thing, int16_t kinetic_energy, int16_t attack,
                                    uint16_t step_energy);

#endif
