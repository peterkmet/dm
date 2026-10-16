/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "engine/message.h"
#include "engine/dungeon.h"
#include "engine/game.h"
#include <stdio.h>
#include <string.h>

/* Size of the wrapped line buffer. */
#define MESSAGE_WRAP_LINE_SIZE 100 /* one wrapped line: at most MESSAGE_COLS characters */

MessageArea g_message = {
    .cursor_row = MESSAGE_LAST_ROW,
    .expiry = {MESSAGE_NO_EXPIRY, MESSAGE_NO_EXPIRY, MESSAGE_NO_EXPIRY, MESSAGE_NO_EXPIRY}
};

/* Empties row r and stops its expiry. */
static void clear_row(int r)
{
    memset(g_message.text[r], 0, sizeof g_message.text[r]);
    memset(g_message.color[r], 0, sizeof g_message.color[r]);
    g_message.expiry[r] = MESSAGE_NO_EXPIRY;
}

void message_move_cursor(int16_t column, int16_t row)
{
    if (column < 0)
        column = 0;
    if (column > MESSAGE_COLS - 1)
        column = MESSAGE_COLS - 1;
    int16_t x = column * FONT_CHAR_WIDTH;
    int16_t max = MESSAGE_ROW_WIDTH - FONT_CHAR_WIDTH;
    g_message.cursor_x = x < max ? x : max;
    g_message.cursor_row = row < 0 ? 0 : row > MESSAGE_LAST_ROW ? MESSAGE_LAST_ROW : row;
}

void message_clear(void)
{
    message_move_cursor(0, MESSAGE_LAST_ROW);
    for (int r = 0; r < MESSAGE_ROWS; r++)
        clear_row(r);
}

void message_clear_expired_rows(void)
{
    for (int r = 0; r < MESSAGE_ROWS; r++)
        if (g_message.expiry[r] != MESSAGE_NO_EXPIRY && g_message.expiry[r] <= g_game_time)
            clear_row(r);
}

/* Moves the cursor down one row; on the last row the area scrolls up one row. */
static void start_new_row(void)
{
    if (g_message.cursor_row != MESSAGE_LAST_ROW) {
        g_message.cursor_row++;
        return;
    }
    memmove(g_message.text[0], g_message.text[1], sizeof g_message.text[0] * MESSAGE_LAST_ROW);
    memmove(g_message.color[0], g_message.color[1], sizeof g_message.color[0] * MESSAGE_LAST_ROW);
    memmove(g_message.expiry, g_message.expiry + 1, sizeof g_message.expiry[0] * MESSAGE_LAST_ROW);
    clear_row(MESSAGE_LAST_ROW);
}

/* Writes s at the cursor without moving it and restarts the row's display time. */
static void print_string(int16_t color, const char *s)
{
    int r = g_message.cursor_row;
    /* ponytail: cells are FONT_CHAR_WIDTH apart; the clamped cursor x 314 maps to cell 52 (x 312) */
    for (int c = g_message.cursor_x / FONT_CHAR_WIDTH; *s && c < MESSAGE_COLS; s++, c++) {
        g_message.text[r][c] = *s;
        g_message.color[r][c] = color;
    }
    g_message.expiry[r] = g_game_time + MESSAGE_LIFETIME;
}

int16_t message_next_word_line(const char *s, char *line, int16_t *pos, int16_t max_width)
{
    int16_t w = 0;
    int16_t space = -1;
    int16_t space_w = 0;
    int16_t space_n = 0;
    int16_t p = *pos;
    int16_t n = 0;
    for (;; p++, n++) {
        line[n] = 0;
        if (s[p] == '\n' || s[p] == 0) {
            *pos = p;
            return w;
        }
        if (s[p] == ' ') {
            space = p;
            space_w = w;
            space_n = n;
        }
        if (max_width <= w + FONT_CHAR_WIDTH)
            break;
        w += FONT_CHAR_WIDTH;
        line[n] = s[p];
    }
    if (space == -1) {
        *pos = p;
        return w;
    }
    *pos = space + 1;
    line[space_n] = 0;
    return space_w;
}

void message_print(int16_t color, const char *s)
{
    char line[MESSAGE_WRAP_LINE_SIZE];
    int16_t pos = 0;
    while (s[pos]) {
        int16_t w = message_next_word_line(s, line, &pos, MESSAGE_ROW_WIDTH - g_message.cursor_x);
        if (s[pos] == '\n') {
            start_new_row();
            print_string(color, line);
            g_message.cursor_x = 0;
            pos++;
        } else if (s[pos] == 0) {
            print_string(color, line);
            g_message.cursor_x += w;
        } else {
            print_string(color, line);
            start_new_row();
            g_message.cursor_x = MESSAGE_WRAP_INDENT;
        }
    }
}

void message_new_line(void)
{
    message_print(0, "\n");
}

void message_print_dungeon_text(uint16_t text_thing)
{
    char buffer[TEXT_SIZE];
    dungeon_decode_text(buffer, sizeof buffer, text_thing, TEXT_MODE_MESSAGE);
    message_print(MESSAGE_COLOR_DUNGEON_TEXT, buffer);
}
