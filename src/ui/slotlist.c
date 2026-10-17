/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "slotlist.h"
#include "graphics.h"
#include "ids.h"
#include "savegame.h"
#include "command.h"
#include "dialog.h"
#include "draw.h"
#include "frame.h"
#include "gameloop.h"
#include "layout.h"
#include "text.h"
#include "ui.h"
#include <stdio.h>

/* The rows in the message panel of the dialog graphic, in viewport pixels. */
#define LIST_ROWS      (SAVEGAME_SLOT_COUNT + 1)
#define ROW_BOX_LEFT   12  /* the left edge of the message panel */
#define ROW_BOX_WIDTH  200 /* the width of the message panel */
#define ROW_TEXT_LEFT  14
#define ROW_TOP        14 /* the top of the first row */
#define ROW_HEIGHT     8  /* the font rows and 2 blank rows */
#define SLOT_TEXT_SIZE 40
#define LABEL_SIZE     8

/* The row colors. */
#define SLOT_COLOR         COLOR_GOLD
#define DISABLED_COLOR     COLOR_GRAY
#define PRESSED_COLOR      COLOR_YELLOW
#define ROW_BACKGROUND     COLOR_LIGHT_BROWN
#define PRESSED_BACKGROUND COLOR_BROWN

/* The CANCEL choice: the one choice of the dialog. */
#define CANCEL_CHOICE_COUNT 1
#define CANCEL_CHOICE       1

static struct {
    bool open;     /* the list waits for a click */
    bool closed;   /* a click or Escape closed the list */
    int16_t row;   /* the row that closed the list, -1 for CANCEL */
    int16_t count; /* 8 rows in the save list, 9 in the load list */
    char text[LIST_ROWS][SLOT_TEXT_SIZE];
    bool selectable[LIST_ROWS];
} list;

/* The text of a row: the slot number or QUICK, then EMPTY, DAMAGED, or the level and the file time. */
static void format_row(char *text, int16_t row, SavegameInfoResult result, const SavegameSlotInfo *info)
{
    char label[LABEL_SIZE];
    if (row == SLOTLIST_QUICK_ROW)
        snprintf(label, sizeof label, "QUICK");
    else
        snprintf(label, sizeof label, "%d", row + 1);
    if (result == SAVEGAME_INFO_EMPTY)
        snprintf(text, SLOT_TEXT_SIZE, "%s  EMPTY", label);
    else if (result == SAVEGAME_INFO_DAMAGED)
        snprintf(text, SLOT_TEXT_SIZE, "%s  DAMAGED", label);
    else if (!info->has_time)
        snprintf(text, SLOT_TEXT_SIZE, "%s  LEVEL %d", label, info->level + 1);
    else
        snprintf(text, SLOT_TEXT_SIZE, "%s  LEVEL %d  %04d-%02d-%02d %02d:%02d", label, info->level + 1,
                 info->time.year, info->time.month, info->time.day, info->time.hour, info->time.minute);
}

/* Reads the slot infos in directory into the row texts; in the load list only a slot with a game can be selected. */
static void read_rows(const char *directory, bool load)
{
    list.count = load ? LIST_ROWS : SAVEGAME_SLOT_COUNT;
    for (int16_t row = 0; row < list.count; row++) {
        SavegameSlotInfo info;
        SavegameInfoResult result = row == SLOTLIST_QUICK_ROW ? savegame_read_quick_info(directory, &info)
                                                              : savegame_read_slot_info(directory, row, &info);
        format_row(list.text[row], row, result, &info);
        list.selectable[row] = !load || result == SAVEGAME_INFO_READ;
    }
}

/* The box of a row in the viewport. */
static Box get_row_box(int16_t row)
{
    return (Box){ROW_BOX_LEFT, ROW_TOP + row * ROW_HEIGHT, ROW_BOX_WIDTH, ROW_HEIGHT};
}

/* Draws a row on its background color. */
static void show_row(Bitmap *viewport, int16_t row, uint8_t color, uint8_t background)
{
    Box box = get_row_box(row);
    draw_fill(viewport, &box, background);
    text_draw(viewport, (Point){ROW_TEXT_LEFT, box.y + FONT_LINE_HEIGHT - FONT_BASELINE_GAP}, color, background,
              list.text[row]);
}

/* Draws the dialog graphic with CANCEL, then the rows in the message panel, and shows the viewport. */
static void show_list(void)
{
    Bitmap *viewport = ui_get_viewport();
    dialog_draw(&(DialogText){
        {NULL, NULL},
        {gfx_get_string(TEXT_CANCEL)}
    });
    for (int16_t row = 0; row < list.count; row++)
        show_row(viewport, row, list.selectable[row] ? SLOT_COLOR : DISABLED_COLOR, ROW_BACKGROUND);
    dialog_show_viewport();
}

/* Shows the row or CANCEL that closed the list pressed for the frames of a pressed dialog choice. */
static void show_pressed(void)
{
    Box cancel;
    if (list.row < 0) {
        if (dialog_get_choice_box(CANCEL_CHOICE_COUNT, CANCEL_CHOICE, &cancel))
            dialog_show_pressed(&cancel);
        return;
    }
    show_row(ui_get_viewport(), list.row, PRESSED_COLOR, PRESSED_BACKGROUND);
    dialog_show_viewport();
    frame_wait_count(DIALOG_PRESS_FRAMES);
}

int16_t slotlist_choose(const char *directory, bool load)
{
    read_rows(directory, load);
    show_list();
    command_save_input();
    command_set_only_mouse_input(-1);
    command_discard_all_input();
    list.closed = false;
    list.row = -1;
    list.open = true;
    while (!list.closed && frame_wait())
        ;
    list.open = false;
    if (list.closed)
        show_pressed();
    command_restore_input();
    command_discard_all_input();
    return list.closed ? list.row : -1;
}

/* The top left corner of the viewport on the screen: its zone in the game, else centered on the inactive screen. */
static Point get_viewport_origin(void)
{
    Bitmap *viewport = ui_get_viewport();
    ZoneClip placed;
    int16_t zone = g_game_screen_shown ? ZONE_VIEWPORT : ZONE_DIALOG_ON_SCREEN;
    if (!layout_get_zone(zone, &(ZoneItem){viewport->w, viewport->h, 0, 0}, &placed))
        return (Point){0, 0};
    return (Point){placed.box.x - placed.source_x, placed.box.y - placed.source_y};
}

bool slotlist_process_click(int16_t x, int16_t y, int16_t kind)
{
    if (!list.open)
        return false;
    if (kind != CLICK_LEFT_PRESS || list.closed)
        return true;
    Point origin = get_viewport_origin();
    int16_t vx = x - origin.x;
    int16_t vy = y - origin.y;
    Box cancel;
    if (dialog_get_choice_box(CANCEL_CHOICE_COUNT, CANCEL_CHOICE, &cancel) && box_contains(&cancel, vx, vy)) {
        list.closed = true;
        return true;
    }
    for (int16_t row = 0; row < list.count; row++) {
        Box box = get_row_box(row);
        if (list.selectable[row] && box_contains(&box, vx, vy)) {
            list.row = row;
            list.closed = true;
        }
    }
    return true;
}

bool slotlist_process_key(uint16_t key)
{
    if (!list.open)
        return false;
    if (key == KEY_ESCAPE)
        list.closed = true;
    return true;
}
