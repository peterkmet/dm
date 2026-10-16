/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_ENGINE_SENSOR_H
#define DM_ENGINE_SENSOR_H
/* Prefixes: sensor_ */
#include "engine/map.h"
#include <stdbool.h>
#include <stdint.h>

/* Floor and wall sensors. Coordinates are on the current map. */

#define EFFECT_SET                    0
#define EFFECT_CLEAR                  1
#define EFFECT_TOGGLE                 2
#define EFFECT_HOLD                   3
#define LOCAL_EFFECT_ROTATE_SENSORS_1 1
#define LOCAL_EFFECT_ROTATE_SENSORS   2
#define LOCAL_EFFECT_EXPERIENCE       10
#define ANY_CELL                      (-1)
#define VI_ALTAR_REBIRTH_EFFECT       2 /* event 13 effect byte */

/* Adds a square event on the current map. Clear events come first at equal time, then toggle, then set.
   CSB gives each sensor event priority 0. */
void sensor_add_event(int16_t event_type, Location at, int16_t cell, int16_t effect, uint32_t time);
/* Bones put into a vi altar: the rebirth of their champion starts next tick on the party map. */
void sensor_add_vi_altar_rebirth_event(uint16_t bones, int16_t x, int16_t y, int16_t cell);
/* A press on the button of the door at x, y: a switch sound and the door toggles next tick. */
void sensor_press_door_button(int16_t x, int16_t y);
/* Moves the first sensor of the cell behind the last sensor of the cell. */
void sensor_process_rotation_effect(void);
/* No rotation effect waits at the start of a game. */
void sensor_reset(void);
/* A remote effect becomes a square event on the target after the sensor delay. Only a wall target keeps the target cell. */
void sensor_trigger_effect(uint16_t sensor, int16_t effect, int16_t x, int16_t y, int16_t cell);
/* Click on the front wall cell with the leader hand. Returns true when a sensor triggered. The object
   generator gives no object until projectile_get_object_for_launcher_or_generator exists. */
bool sensor_is_triggered_by_click_on_wall(int16_t x, int16_t y, int16_t cell);
/* thing THING_NONE is the party. A floor square runs its floor sensors. A wall square runs the sensors of the thing cell. */
void sensor_process_thing_addition_or_removal(int16_t x, int16_t y, uint16_t thing, bool party_on_square, bool add);

#endif
