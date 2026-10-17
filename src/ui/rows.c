/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "rows.h"
#include "graphics.h"
#include "ids.h"
#include "text.h"
#include "ui.h"
#include <string.h>

/* The row layout in the viewport, in pixels. */
#define LEFT      8  /* the first text column */
#define TITLE_TOP 6  /* the top of the title */
#define TOP       22 /* the top of the first row */
#define HEIGHT    10 /* a row: the text line and the gap to the next row */

/* The text colors. */
#define TITLE_COLOR    COLOR_CYAN
#define ROW_COLOR      COLOR_WHITE
#define SELECTED_COLOR COLOR_YELLOW

/* Draws a text line with its top at y. */
static void show_line(Bitmap *viewport, int16_t y, uint8_t color, const char *line)
{
    text_draw(viewport, (Point){LEFT, y + FONT_LINE_HEIGHT - FONT_BASELINE_GAP}, color, COLOR_BLACK, line);
}

void rows_draw(const char *title, const char *const *rows, int16_t count, int16_t selected)
{
    Bitmap *viewport = ui_get_viewport();
    memset(viewport->px, COLOR_BLACK, (size_t)viewport->w * viewport->h);
    show_line(viewport, TITLE_TOP, TITLE_COLOR, title);
    for (int16_t i = 0; i < count && i < ROWS_MAX_COUNT; i++)
        show_line(viewport, TOP + i * HEIGHT + (HEIGHT - FONT_LINE_HEIGHT) / 2,
                  i == selected ? SELECTED_COLOR : ROW_COLOR, rows[i]);
    ui_draw_viewport();
}

int16_t rows_find_row(int16_t x, int16_t y, int16_t count)
{
    Box b;
    if (!ui_zone_box(ZONE_VIEWPORT, &b) || !box_contains(&b, x, y) || y - b.y < TOP)
        return -1;
    int16_t row = (y - b.y - TOP) / HEIGHT;
    return row < count && row < ROWS_MAX_COUNT ? row : -1;
}

int16_t rows_find_column(int16_t x)
{
    Box b;
    if (!ui_zone_box(ZONE_VIEWPORT, &b) || x - b.x < LEFT)
        return -1;
    return (x - b.x - LEFT) / FONT_CHAR_WIDTH;
}
