#ifndef SENSOR_H
#define SENSOR_H
/* Prefixes: sensor_ */
#include <stdbool.h>
#include <stdint.h>

/* Floor and wall sensors. Coordinates are on the current map. */

enum { EFFECT_SET, EFFECT_CLEAR, EFFECT_TOGGLE, EFFECT_HOLD };
enum { LOCAL_EFFECT_ROTATE_SENSORS_1 = 1, LOCAL_EFFECT_ROTATE_SENSORS = 2, LOCAL_EFFECT_EXPERIENCE = 10 };
enum { SENSOR_TEXT_COLOR = 15 }; /* white */
#define ANY_CELL (-1)

extern int16_t g_sensor_rotation_effect, g_sensor_rotation_effect_map_x, g_sensor_rotation_effect_map_y,
    g_sensor_rotation_effect_cell;

void sensor_add_event(int16_t event_type, int16_t x, int16_t y, int16_t cell, int16_t effect, uint32_t time);
void sensor_trigger_local_effect(int16_t local_effect, int16_t x, int16_t y, int16_t cell);
void sensor_process_rotation_effect(void);
void sensor_reset(void);
void sensor_trigger_effect(uint16_t sensor, int16_t effect, int16_t x, int16_t y, int16_t cell);
uint16_t sensor_get_object_of_type_in_cell(int16_t x, int16_t y, int16_t cell, int16_t object_type);
bool sensor_is_object_in_party_possession(int16_t object_type);
bool sensor_is_triggered_by_click_on_wall(int16_t x, int16_t y, uint16_t cell);
void sensor_process_thing_addition_or_removal(int16_t x, int16_t y, uint16_t thing, bool party_on_square, bool add);

#endif
