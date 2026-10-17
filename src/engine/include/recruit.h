/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_ENGINE_RECRUIT_H
#define DM_ENGINE_RECRUIT_H
/* Prefixes: recruit_ */

/* Candidate champions: the champion data from the text of a mirror. */

#include "champion.h"
#include <stdbool.h>
#include <stdint.h>

/* Portraits in one row of the portrait graphic. */
#define PORTRAITS_PER_ROW 8

/* Candidate champion data from the mirror text, as champion_add_candidate_to_party sets it. */
typedef struct {
    char name[CHAMPION_NAME_SIZE];
    char title[CHAMPION_TITLE_SIZE];
    uint8_t male;
    int16_t current_health, maximum_health;
    int16_t current_stamina, maximum_stamina;
    int16_t current_mana, maximum_mana;
    uint8_t statistics[STAT_COUNT][STAT_VALUE_COUNT];
    int32_t experience[SKILL_COUNT]; /* base skills: sum of their hidden skills */
} RecruitData;

/* Decodes char_count letters, 4 bits each. Each character adds c - 'A' to the low byte only. */
uint16_t recruit_decoded_value(const char *text, int16_t char_count); /* exported for the tests */
/* Reads name, title, gender, bar values, statistics and skills from a champion text.   Returns false when the text is too short. */
bool recruit_parse_text(const char *text, RecruitData *out); /* exported for the tests */
/* Decodes a champion text thing and reads the candidate data from it. */
bool recruit_parse(uint16_t text_thing, RecruitData *out);
/* The first text thing on the square, or THING_NONE. */
uint16_t recruit_text_thing(int16_t map, int16_t x, int16_t y);
/* A champion portrait sensor keeps the portrait index in its data. Else -1. */
int16_t recruit_portrait_index(uint16_t sensor_thing); /* exported for the tests */
/* Top-left of the portrait in the champion portraits graphic. */
void recruit_portrait_origin(int16_t portrait_index, int16_t *out_x, int16_t *out_y);

#endif
