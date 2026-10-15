#ifndef VIDEO_H
#define VIDEO_H
/* Prefixes: video_ */
#include <stdint.h>

#define SCREEN_W          320
#define SCREEN_H          200
#define PALETTE_COLORS    16 /* colors of one palette half */
#define COLOR_INDEX_MASK  (PALETTE_COLORS - 1)
#define FRAMES_PER_SECOND 50 /* PAL vertical blank */
#define ARGB_RED_SHIFT    16 /* bit position of the red byte in a video_argb color */
#define ARGB_GREEN_SHIFT  8

extern uint8_t fb[SCREEN_H][SCREEN_W];
/* Palette split of the copper list: rows palette_split_top to palette_split_bottom - 1 use the
   top palette (palette[0..15], the dungeon view), the other rows the bottom palette (palette[16..31],
   the interface). The default puts every row in the top palette. */
extern uint16_t palette[2 * PALETTE_COLORS];
extern int16_t palette_split_top, palette_split_bottom;

/* Converts an Amiga 12-bit color (4 bits each of red, green, blue) to ARGB8888. */
uint32_t video_argb(uint16_t amiga);
/* Amiga color of index c on screen row y. */
uint16_t video_color(int y, uint8_t c);
/* Largest scale that fits the output with the screen's aspect ratio, centered (letterbox). */
void video_layout(int out_w, int out_h, float *scale, float *ox, float *oy);
/* Maps output pixel coordinates to game coordinates, clamped to the screen. */
void video_map_mouse(int out_w, int out_h, int x, int y, int *gx, int *gy);

#endif
