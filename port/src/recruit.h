#ifndef RECRUIT_H
#define RECRUIT_H
#include <stdint.h>

/* Candidate champion data from the mirror text, as champion_add_candidate_to_party sets it. */
typedef struct {
    char name[8];
    char title[20];
    uint8_t male;
    int16_t current_health, maximum_health;
    int16_t current_stamina, maximum_stamina;
    int16_t current_mana, maximum_mana;
    uint8_t statistics[7][3];  /* luck, strength, dexterity, wisdom, vitality, anti-magic, anti-fire: maximum, current, minimum */
    int32_t experience[20];    /* 0-3 base skills (sum of their hidden skills), 4-19 hidden skills */
} RecruitData;

uint16_t recruit_decoded_value(const char *text, int16_t char_count);
int16_t recruit_parse_text(const char *text, RecruitData *out);
int16_t recruit_parse(uint16_t text_thing, RecruitData *out);
uint16_t recruit_text_thing(int16_t map, int16_t x, int16_t y);
int16_t recruit_portrait_index(uint16_t sensor_thing);
void recruit_portrait_origin(int16_t portrait_index, int16_t *x, int16_t *y);

#endif
