/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_ENGINE_MESSAGE_H
#define DM_ENGINE_MESSAGE_H
/* Prefixes: message_ */
#include "data/graphics.h"
#include <stdint.h>

/* Rows, sizes, lifetime and colors of the message lines. */
#define MESSAGE_ROWS               4
#define MESSAGE_LAST_ROW           (MESSAGE_ROWS - 1)
#define MESSAGE_ROW_HEIGHT         7   /* pixel rows of one message line */
#define MESSAGE_ROW_WIDTH          320 /* pixels of one message line */
#define MESSAGE_COLS               (MESSAGE_ROW_WIDTH / FONT_CHAR_WIDTH)
#define MESSAGE_LIFETIME           70 /* game ticks until a row expires */
#define MESSAGE_NO_EXPIRY          UINT32_MAX
#define MESSAGE_WRAP_INDENT        12 /* x where a wrapped row continues */
#define MESSAGE_COLOR_CYAN         4  /* color of the spell and action messages */
#define MESSAGE_COLOR_DUNGEON_TEXT 15 /* white */

/* Rule-level model of the message area (zone 15). A row is one character cell per
   FONT_CHAR_WIDTH pixels; text 0 means an empty cell. */
typedef struct {
    char text[MESSAGE_ROWS][MESSAGE_COLS];
    uint8_t color[MESSAGE_ROWS][MESSAGE_COLS];
    uint32_t expiry[MESSAGE_ROWS];
    int16_t cursor_row, cursor_x;
} MessageArea;

extern MessageArea g_message;

/* Empties all rows and puts the cursor at the start of the last row. */
void message_clear(void);
/* Moves the cursor to a character column and a row, clamped to the area. */
void message_move_cursor(int16_t column, int16_t row); /* exported for the tests */
/* Empties the rows whose display time has run out. */
void message_clear_expired_rows(void);
/* Prints s with word wrap; '\n' starts a new row, a wrapped row continues indented. */
void message_print(int16_t color, const char *s);
/* Starts a new row. */
void message_new_line(void);
/* Copies from s + *pos the words that fit in max_width pixels into line, moves *pos, and
   returns the line width. A line ends at '\n' or '\0' (*pos stays on it) or after the last
   space that fits (*pos skips the space). */
int16_t message_next_word_line(const char *s, char *line, int16_t *pos, int16_t max_width);
/* Prints the text of a text record in the color of the dungeon texts. */
void message_print_dungeon_text(uint16_t text_thing);

#endif
