#include <stdio.h>
#include <string.h>
#include "message.h"

#define MESSAGE_WRAP_LINE_SIZE 100 /* one wrapped line: at most MESSAGE_COLS characters */
#define NUMBER_TEXT_SIZE       14  /* a 32-bit number with its sign */

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

/* Moves the cursor to a character column and a row, clamped to the area. */
void message_move_cursor(int16_t column, int16_t row)
{
    if (column < 0)
        column = 0;
    if (column > MESSAGE_COLS - 1)
        column = MESSAGE_COLS - 1;
    int16_t x = column * TEXT_CHAR_WIDTH;
    int16_t max = MESSAGE_ROW_WIDTH - TEXT_CHAR_WIDTH;
    g_message.cursor_x = x < max ? x : max;
    g_message.cursor_row = row < 0 ? 0 : row > MESSAGE_LAST_ROW ? MESSAGE_LAST_ROW : row;
}

/* Empties all rows and puts the cursor at the start of the last row. */
void message_clear(void)
{
    message_move_cursor(0, MESSAGE_LAST_ROW);
    for (int r = 0; r < MESSAGE_ROWS; r++)
        clear_row(r);
}

/* Empties the rows whose display time has run out. */
void message_clear_expired_rows(uint32_t game_time)
{
    for (int r = 0; r < MESSAGE_ROWS; r++)
        if (g_message.expiry[r] != MESSAGE_NO_EXPIRY && g_message.expiry[r] <= game_time)
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
static void print_string(int16_t color, const char *s, uint32_t game_time)
{
    int r = g_message.cursor_row;
    /* ponytail: cells are TEXT_CHAR_WIDTH apart; the clamped cursor x 314 maps to cell 52 (x 312) */
    for (int c = g_message.cursor_x / TEXT_CHAR_WIDTH; *s && c < MESSAGE_COLS; s++, c++) {
        g_message.text[r][c] = *s;
        g_message.color[r][c] = color;
    }
    g_message.expiry[r] = game_time + MESSAGE_LIFETIME;
}

/* Copies from s + *pos the words that fit in max_width pixels into line, moves *pos, and
   returns the line width. A line ends at '\n' or '\0' (*pos stays on it) or after the last
   space that fits (*pos skips the space). */
int message_next_word_line(const char *s, char *line, int *pos, int max_width)
{
    int w = 0;
    int space = -1;
    int space_w = 0;
    int space_n = 0;
    int p = *pos;
    int n = 0;
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
        if (max_width <= w + TEXT_CHAR_WIDTH)
            break;
        w += TEXT_CHAR_WIDTH;
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

/* Prints s with word wrap; '\n' starts a new row, a wrapped row continues indented. */
void message_print(int16_t color, const char *s, uint32_t game_time)
{
    char line[MESSAGE_WRAP_LINE_SIZE];
    int pos = 0;
    while (s[pos]) {
        int w = message_next_word_line(s, line, &pos, MESSAGE_ROW_WIDTH - g_message.cursor_x);
        if (s[pos] == '\n') {
            start_new_row();
            print_string(color, line, game_time);
            g_message.cursor_x = 0;
            pos++;
        } else if (s[pos] == 0) {
            print_string(color, line, game_time);
            g_message.cursor_x += w;
        } else {
            print_string(color, line, game_time);
            start_new_row();
            g_message.cursor_x = MESSAGE_WRAP_INDENT;
        }
    }
}

/* Prints a number and a space. */
void message_print_number(int16_t color, int32_t value, uint32_t game_time)
{
    char buf[NUMBER_TEXT_SIZE];
    snprintf(buf, sizeof buf, "%d", (int)value);
    message_print(color, buf, game_time);
    message_print(color, " ", game_time);
}

/* Starts a new row. */
void message_new_line(uint32_t game_time)
{
    message_print(0, "\n", game_time);
}
