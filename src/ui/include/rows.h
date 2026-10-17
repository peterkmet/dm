/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_UI_ROWS_H
#define DM_UI_ROWS_H
/* Prefixes: rows_ */

/* The rows of text of the viewport screens (not in the original game): the options menu and the cheat screen.
   It draws a title and the rows in the viewport with the game font and the interface palette, and maps a mouse
   position to a row and a text column. It holds no menu state. */

#include <stdint.h>

/* The rows that fit below the title. */
#define ROWS_MAX_COUNT 11

/* Draws the title and the rows on black in the viewport, the selected row in yellow, then shows the viewport. */
void rows_draw(const char *title, const char *const *rows, int16_t count, int16_t selected);
/* The row of count rows at the screen position (x, y), or -1 outside the rows. */
int16_t rows_find_row(int16_t x, int16_t y, int16_t count);
/* The text column of a row at the screen x, or -1 left of the text. */
int16_t rows_find_column(int16_t x);

#endif
