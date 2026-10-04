#ifndef VIDEO_H
#define VIDEO_H
#include <stdint.h>

#define SCREEN_W 320
#define SCREEN_H 200

extern uint8_t fb[SCREEN_H][SCREEN_W];
/* Palette split of the copper list: rows palette_split_top to palette_split_bottom - 1 use the
   top palette (palette[0..15], the dungeon view), the other rows the bottom palette (palette[16..31],
   the interface). The default puts every row in the top palette. */
extern uint16_t palette[32];
extern int16_t palette_split_top, palette_split_bottom;

uint32_t video_argb(uint16_t amiga);
uint16_t video_color(int y, uint8_t c);
void video_layout(int out_w, int out_h, int *scale, int *ox, int *oy);
void video_map_mouse(int out_w, int out_h, int x, int y, int *gx, int *gy);

#endif
