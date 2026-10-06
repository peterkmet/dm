#include <stdio.h>
#include <string.h>
#include "message.h"
#include "text.h"

MessageArea g_message = { .cursor_row = MESSAGE_LAST_ROW, .expiry = { MESSAGE_NO_EXPIRY, MESSAGE_NO_EXPIRY, MESSAGE_NO_EXPIRY, MESSAGE_NO_EXPIRY } };

/* Empties row r and stops its expiry. */
static void clear_row(int r)
{
    memset(g_message.text[r], 0, sizeof g_message.text[r]);
    memset(g_message.color[r], 0, sizeof g_message.color[r]);
    g_message.expiry[r] = MESSAGE_NO_EXPIRY;
}

/* Moves the cursor to a character column and a row, clamped to the area. */
void message_move_cursor(int16_t column, int16_t row)
{
    if (column < 0) column = 0;
    if (column > MESSAGE_COLS - 1) column = MESSAGE_COLS - 1;
    int16_t x = column * TEXT_CHAR_WIDTH, max = MESSAGE_ROW_WIDTH - TEXT_CHAR_WIDTH;
    g_message.cursor_x = x < max ? x : max;
    g_message.cursor_row = row < 0 ? 0 : row > MESSAGE_LAST_ROW ? MESSAGE_LAST_ROW : row;
}

/* Empties all rows and puts the cursor at the start of the last row. */
void message_clear(void)
{
    message_move_cursor(0, MESSAGE_LAST_ROW);
    for (int r = 0; r < MESSAGE_ROWS; r++) clear_row(r);
}

/* Empties the rows whose display time has run out. */
void message_clear_expired_rows(uint32_t game_time)
{
    for (int r = 0; r < MESSAGE_ROWS; r++)
        if (g_message.expiry[r] != MESSAGE_NO_EXPIRY && g_message.expiry[r] <= game_time) clear_row(r);
}

/* Moves the cursor down one row; on the last row the area scrolls up one row. */
static void new_row(void)
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
static void print_string(int16_t color, const char *s, uint32_t game_time)
{
    int r = g_message.cursor_row;
    /* ponytail: cells are TEXT_CHAR_WIDTH apart; the clamped cursor x 314 maps to cell 52 (x 312) */
    for (int c = g_message.cursor_x / TEXT_CHAR_WIDTH; *s && c < MESSAGE_COLS; s++, c++) {
        g_message.text[r][c] = *s;
        g_message.color[r][c] = (uint8_t)color;
    }
    g_message.expiry[r] = game_time + MESSAGE_LIFETIME;
}

/* Prints s with word wrap; '\n' starts a new row, a wrapped row continues indented. */
void message_print(int16_t color, const char *s, uint32_t game_time)
{
    char line[100];
    int pos = 0;
    while (s[pos]) {
        int w = text_next_word_line(s, line, &pos, MESSAGE_ROW_WIDTH - g_message.cursor_x);
        if (s[pos] == '\n') {
            new_row();
            print_string(color, line, game_time);
            g_message.cursor_x = 0;
            pos++;
        } else if (s[pos] == 0) {
            print_string(color, line, game_time);
            g_message.cursor_x += w;
        } else {
            print_string(color, line, game_time);
            new_row();
            g_message.cursor_x = MESSAGE_WRAP_INDENT;
        }
    }
}

/* Prints a number and a space. */
void message_print_number(int16_t color, int32_t value, uint32_t game_time)
{
    char buf[14];
    snprintf(buf, sizeof buf, "%d", (int)value);
    message_print(color, buf, game_time);
    message_print(color, " ", game_time);
}

/* Starts a new row. */
void message_new_line(uint32_t game_time) { message_print(0, "\n", game_time); }

/* Draws the 4 rows into the zone at (x, y) in their scrolled state: the zone is cleared
   to color 0, then each cell is drawn with its color on color 0. */
void message_draw(Bitmap *bm, int x, int y)
{
    for (int py = y; py < y + MESSAGE_ROWS * MESSAGE_ROW_HEIGHT; py++)
        for (int px = x; px < x + MESSAGE_ROW_WIDTH; px++)
            if (py >= 0 && py < bm->h && px >= 0 && px < bm->w) bm->px[py * bm->w + px] = 0;
    for (int r = 0; r < MESSAGE_ROWS; r++)
        for (int c = 0; c < MESSAGE_COLS; c++)
            if (g_message.text[r][c]) {
                char ch[2] = { g_message.text[r][c], 0 };
                text_draw(bm, x + c * TEXT_CHAR_WIDTH, y + r * MESSAGE_ROW_HEIGHT + TEXT_LINE_HEIGHT - TEXT_BASELINE_GAP - 1,
                          g_message.color[r][c], 0, ch);
            }
}
