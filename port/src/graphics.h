#ifndef GRAPHICS_H
#define GRAPHICS_H
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint16_t w, h;
    uint8_t *px; /* w * h bytes, palette index 0-15 */
} Bitmap;

int gfx_open(const char *path);
int gfx_count(void);
const uint8_t *gfx_item(int i, uint16_t *size);
int gfx_expand(const uint8_t *packed, size_t len, Bitmap *bm);
int gfx_load_bitmap(int i, Bitmap *bm);
void gfx_free_bitmap(Bitmap *bm);

#endif
