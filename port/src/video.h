#ifndef VIDEO_H
#define VIDEO_H
#include <stdint.h>

#define SCREEN_W 320
#define SCREEN_H 200

extern uint8_t fb[SCREEN_H][SCREEN_W];
extern uint16_t palette[16];

uint32_t video_argb(uint16_t amiga);
void video_layout(int out_w, int out_h, int *scale, int *ox, int *oy);
void video_map_mouse(int out_w, int out_h, int x, int y, int *gx, int *gy);

#endif
