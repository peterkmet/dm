#include "recruit.h"
#include "dungeon.h"
#include "ftl.h"
#include "map.h"
#include <string.h>

#define G_PORTRAIT_WIDTH 0x102958
#define G_PORTRAIT_HEIGHT 0x10295a

/* Offsets in the values line of a champion text, counted from its leading '\n'. */
#define VALUES_GENDER 1
#define VALUES_HEALTH 3
#define VALUES_STAMINA 7
#define VALUES_MANA 11
#define VALUES_STATISTICS 16  /* 2 characters per statistic */
#define VALUES_SKILLS 31      /* 1 character per hidden skill */
#define VALUES_LENGTH 47
#define BAR_VALUE_CHARS 4
#define STATISTIC_CHARS 2
#define RECRUIT_STAT_MINIMUM 30
#define SKILL_BASE_EXPERIENCE 125     /* experience of skill level 1, doubled per level */
#define HIDDEN_SKILLS_PER_BASE 4
#define VALUE_BITS_PER_CHAR 4         /* a value character holds 4 bits */
#define RECRUIT_TEXT_SIZE 128         /* a champion text fits */

/* Decodes char_count letters, 4 bits each. Each character adds c - 'A' to the low byte only. */
uint16_t recruit_decoded_value(const char *text, int16_t char_count) {
    uint16_t v = 0;
    do {
        uint16_t s = v << VALUE_BITS_PER_CHAR;
        v = s >> 8 << 8 | (uint8_t)(s + *text++ - 'A');
    } while (--char_count != 0);
    return v;
}

/* Copies up to the next '\n' into dst (truncated to dst_size - 1); returns the '\n' or NULL. */
static const char *copy_line(const char *p, char *dst, int dst_size, int *len) {
    for (; *p != '\n'; p++) {
        if (*p == '\0')
            return NULL;
        if (*len < dst_size - 1)
            dst[(*len)++] = *p;
    }
    dst[*len] = '\0';
    return p;
}

/* Experience of a hidden skill level, as the original 125 << level with a 68000 LSL.L:
   the count is modulo 64, and counts of 32 and more give 0. */
static int32_t hidden_skill_experience(uint16_t level) {
    if (level == 0)
        return 0;
    uint16_t count = level & SHIFT_COUNT_MASK;
    return count <= LONG_SHIFT_MAXIMUM ? (int32_t)((uint32_t)SKILL_BASE_EXPERIENCE << count) : 0;
}

/* Reads name, title, gender, bar values, statistics and skills from a champion text.
   Returns 0 when the text is too short. */
int16_t recruit_parse_text(const char *text, RecruitData *out) {
    memset(out, 0, sizeof *out);
    int n = 0, t = 0;
    const char *p = copy_line(text, out->name, sizeof out->name, &n);
    if (p)
        p = copy_line(p + 1, out->title, sizeof out->title, &t);
    if (p)
        p = copy_line(p + 1, out->title, sizeof out->title, &t);
    if (!p || strlen(p) < VALUES_LENGTH)
        return 0;
    out->male = p[VALUES_GENDER] == 'M';
    out->current_health = out->maximum_health = recruit_decoded_value(p + VALUES_HEALTH, BAR_VALUE_CHARS);
    out->current_stamina = out->maximum_stamina = recruit_decoded_value(p + VALUES_STAMINA, BAR_VALUE_CHARS);
    out->current_mana = out->maximum_mana = recruit_decoded_value(p + VALUES_MANA, BAR_VALUE_CHARS);
    for (int i = 0; i < STAT_COUNT; i++) {
        int16_t v = recruit_decoded_value(p + VALUES_STATISTICS + i * STATISTIC_CHARS, STATISTIC_CHARS);
        out->statistics[i][STAT_MAXIMUM] = out->statistics[i][STAT_CURRENT] = (uint8_t)(v > RECRUIT_STAT_MINIMUM ? v : RECRUIT_STAT_MINIMUM);
        out->statistics[i][STAT_MINIMUM] = RECRUIT_STAT_MINIMUM;
    }
    for (int s = SKILL_BASE_COUNT; s < SKILL_COUNT; s++)
        out->experience[s] = hidden_skill_experience(p[VALUES_SKILLS + s - SKILL_BASE_COUNT] - 'A');
    for (int b = 0; b < SKILL_BASE_COUNT; b++)
        for (int h = 0; h < HIDDEN_SKILLS_PER_BASE; h++)
            out->experience[b] += out->experience[SKILL_BASE_COUNT + b * HIDDEN_SKILLS_PER_BASE + h];
    return 1;
}

/* Decodes a champion text thing and reads the candidate data from it. */
int16_t recruit_parse(uint16_t text_thing, RecruitData *out) {
    char text[RECRUIT_TEXT_SIZE];
    return decode_text(text, sizeof text, text_thing, TEXT_MODE_SCROLL | TEXT_MODE_ANY_VISIBILITY) && recruit_parse_text(text, out);
}

/* The first text thing on the square, or THING_NONE. */
uint16_t recruit_text_thing(int16_t map, int16_t x, int16_t y) {
    uint16_t t = dungeon_square_first_thing(map, x, y);
    while (t != THING_ENDOFLIST && THING_TYPE(t) != THING_TEXT)
        t = get_next_thing(t);
    return t == THING_ENDOFLIST ? THING_NONE : t;
}

/* A champion portrait sensor keeps the portrait index in its data. Else -1. */
int16_t recruit_portrait_index(uint16_t sensor_thing) {
    return rec_sensor_type(sensor_thing) == SENSOR_WALL_CHAMPION_PORTRAIT ? rec_sensor_data(sensor_thing) : -1;
}

/* Top-left of the portrait in the champion portraits graphic. */
void recruit_portrait_origin(int16_t portrait_index, int16_t *x, int16_t *y) {
    *x = (portrait_index & (PORTRAITS_PER_ROW - 1)) * kaos_s16(G_PORTRAIT_WIDTH);
    *y = (uint16_t)portrait_index / PORTRAITS_PER_ROW * kaos_s16(G_PORTRAIT_HEIGHT);   /* unsigned division as in the original */

}
