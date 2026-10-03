#include "recruit.h"
#include "dungeon.h"
#include "ftl.h"
#include <string.h>

#define G_PORTRAIT_WIDTH 0x102958
#define G_PORTRAIT_HEIGHT 0x10295a

/* Each character adds c - 'A' to the low byte of the shifted value. */
uint16_t recruit_decoded_value(const char *text, int16_t char_count) {
    uint16_t v = 0;
    do {
        uint16_t s = (uint16_t)(v << 4);
        v = (uint16_t)((s & 0xff00) | (uint8_t)(s + *text++ - 'A'));
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

int16_t recruit_parse_text(const char *text, RecruitData *out) {
    memset(out, 0, sizeof *out);
    int n = 0, t = 0;
    const char *p = copy_line(text, out->name, sizeof out->name, &n);
    if (p)
        p = copy_line(p + 1, out->title, sizeof out->title, &t);
    if (p)
        p = copy_line(p + 1, out->title, sizeof out->title, &t);
    if (!p || strlen(p) < 47)
        return 0;
    out->male = p[1] == 'M';
    out->current_health = out->maximum_health = (int16_t)recruit_decoded_value(p + 3, 4);
    out->current_stamina = out->maximum_stamina = (int16_t)recruit_decoded_value(p + 7, 4);
    out->current_mana = out->maximum_mana = (int16_t)recruit_decoded_value(p + 11, 4);
    p += 16;
    for (int i = 0; i < 7; i++, p += 2) {
        int16_t v = (int16_t)recruit_decoded_value(p, 2);
        out->statistics[i][0] = out->statistics[i][1] = (uint8_t)(v > 30 ? v : 30);
        out->statistics[i][2] = 30;
    }
    for (int s = 4; s < 20; s++) {
        uint16_t level = (uint16_t)(*++p - 'A');
        /* 68000 LSL.L uses the count modulo 64; counts of 32 and more give 0. */
        if (level != 0)
            out->experience[s] = (level & 0x3f) < 32 ? (int32_t)(125u << (level & 0x3f)) : 0;
    }
    for (int b = 0; b < 4; b++)
        for (int h = 0; h < 4; h++)
            out->experience[b] += out->experience[4 + b * 4 + h];
    return 1;
}

int16_t recruit_parse(uint16_t text_thing, RecruitData *out) {
    char text[128];
    return decode_text(text, sizeof text, text_thing, (int16_t)0x8002) && recruit_parse_text(text, out);
}

/* The first text thing on the square, or 0xFFFF. */
uint16_t recruit_text_thing(int16_t map, int16_t x, int16_t y) {
    uint16_t t = dungeon_square_first_thing(map, x, y);
    while (t != 0xfffe && (t & 0x3c00) != 0x800) {
        uint8_t *d = get_thing_data(t);
        t = (uint16_t)(d[0] << 8 | d[1]);
    }
    return t == 0xfffe ? 0xffff : t;
}

/* Sensor type 127 shows a champion portrait; its data is the portrait index. Else -1. */
int16_t recruit_portrait_index(uint16_t sensor_thing) {
    uint8_t *d = get_thing_data(sensor_thing);
    uint16_t w = (uint16_t)(d[2] << 8 | d[3]);
    return (w & 0x7f) == 0x7f ? (int16_t)(w >> 7) : -1;
}

/* Top-left of the portrait in graphic 26: 8 portraits per row. */
void recruit_portrait_origin(int16_t portrait_index, int16_t *x, int16_t *y) {
    *x = (int16_t)((portrait_index & 7) * kaos_s16(G_PORTRAIT_WIDTH));
    *y = (int16_t)(((uint16_t)portrait_index >> 3) * kaos_s16(G_PORTRAIT_HEIGHT));
}
