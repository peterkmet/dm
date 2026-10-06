#ifndef MESSAGE_H
#define MESSAGE_H
#include <stdint.h>
#include "graphics.h"

#define MESSAGE_ROWS 4
#define MESSAGE_LAST_ROW (MESSAGE_ROWS - 1)
#define MESSAGE_ROW_HEIGHT 7   /* g_message_area_row_height */
#define MESSAGE_ROW_WIDTH 320  /* g_message_area_row_bitmap_width */
#define MESSAGE_COLS (MESSAGE_ROW_WIDTH / TEXT_CHAR_WIDTH)
#define MESSAGE_LIFETIME 70    /* game ticks until a row expires */
#define MESSAGE_NO_EXPIRY UINT32_MAX
#define MESSAGE_WRAP_INDENT 12  /* x where a wrapped row continues */

/* Rule-level model of the message area (zone 15). A row is one character cell per
   TEXT_CHAR_WIDTH pixels; text 0 means an empty cell. */
typedef struct {
    char text[MESSAGE_ROWS][MESSAGE_COLS];
    uint8_t color[MESSAGE_ROWS][MESSAGE_COLS];
    uint32_t expiry[MESSAGE_ROWS];
    int16_t cursor_row, cursor_x;
} MessageArea;

extern MessageArea g_message;

void message_clear(void);
void message_move_cursor(int16_t column, int16_t row);
void message_clear_expired_rows(uint32_t game_time);
void message_print(int16_t color, const char *s, uint32_t game_time);
void message_print_number(int16_t color, int32_t value, uint32_t game_time);
void message_new_line(uint32_t game_time);
int message_next_word_line(const char *s, char *line, int *pos, int max_width);

#endif
