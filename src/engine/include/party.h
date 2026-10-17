/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_ENGINE_PARTY_H
#define DM_ENGINE_PARTY_H
/* Prefixes: party_ */
/* The party as one body: its position, its scents, its light, and its movement delays. */
#include <stdint.h>

/* The party scent list. */
#define PARTY_SCENT_COUNT 24

/* A scent strength flag. */
#define SCENT_KEEP_LARGER 0x8000 /* keep the larger strength instead of adding */

/* The bit positions of a position word (kept map position, party scents): x in bits 11-15, y in bits 6-10. */
#define PARTY_POSITION_X_SHIFT 11
#define PARTY_POSITION_Y_SHIFT 6

/* The masks of a position word: five bits of x or y, six bits of the map. */
#define PARTY_POSITION_XY_MASK  0x1f
#define PARTY_POSITION_MAP_MASK 0x3f

typedef struct {
    int16_t magical_light_amount;
    uint8_t thieves_eye_count, footprints_count;
    int16_t shield_defense, fire_shield_defense, spell_shield_defense;
    uint8_t scent_count, freeze_life_ticks, first_scent_index, last_scent_index;
    uint16_t scents[PARTY_SCENT_COUNT]; /* x bits 11-15, y bits 6-10, map bits 0-5 */
    uint8_t scent_strengths[PARTY_SCENT_COUNT];
    uint8_t invisibility_count;
} Party;

extern Party g_party;
extern int16_t g_party_map_x, g_party_map_y, g_party_direction, g_party_map_index;
extern int32_t g_last_party_movement_time;
extern int16_t g_disabled_movement_ticks, g_projectile_disabled_movement_ticks;
extern int16_t g_last_projectile_disabled_movement_direction;
extern int16_t g_party_light_level; /* 0 (bright) to 5; the view step sets it */

/* A position word. x can be negative (unsigned shift). */
static inline uint16_t party_pack_position(int16_t map, int16_t x, int16_t y)
{
    return (uint16_t)x << PARTY_POSITION_X_SHIFT | (y & PARTY_POSITION_XY_MASK) << PARTY_POSITION_Y_SHIFT |
           (map & PARTY_POSITION_MAP_MASK);
}
static inline int16_t party_position_x(uint16_t word)
{
    return word >> PARTY_POSITION_X_SHIFT;
}
static inline int16_t party_position_y(uint16_t word)
{
    return word >> PARTY_POSITION_Y_SHIFT & PARTY_POSITION_XY_MASK;
}

/* The light amount of the party: magical light plus the burning torches in the hands, the brightest first with a
   falling weight. */
int16_t party_get_light(void);
/* Sets g_party_light_level, the darkness of the party light on map map_index (difficulty 0 is always bright). */
void party_set_light_level(int16_t map_index, int16_t light);
/* Burning torches in the hands lose one charge; a burnt out torch may be discarded later. */
void party_burn_torches(void);
/* Removes the oldest scents first; the footprint range moves with the list. */
void party_delete_scent(uint16_t scent_index);
/* Adds strength to the party scent on a square, or keeps the larger strength. The original compares scent i with
   strength count - 1 - i; the port keeps this. A scent word: x in bits 11-15, y in bits 6-10, map in bits 0-5. */
void party_add_scent_strength(int16_t x, int16_t y, int16_t strength_and_flags);

#endif
