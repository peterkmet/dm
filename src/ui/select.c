/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui/select.h"
#include "data/adventure.h"
#include "data/graphics.h"
#include "engine/game.h"
#include "ui/advanced.h"
#include "ui/text.h"
#include "ui/ui.h"
#include "ui/video.h"
#include <string.h>

/* Selection screen layout. */
#define ITEM_MAX_COUNT 3   /* items of the longest screen */
#define HEADING_TOP    56  /* top of the heading line */
#define ITEM_TOP       88  /* top of the first item box */
#define ITEM_STEP      16  /* pixels from one item box to the next */
#define ITEM_MARGIN    8   /* box pixels left and right of the item text */
#define LINE_HEIGHT    12  /* height of an item box and of the heading and message lines */
#define MESSAGE_TOP    152 /* top of the message line */

typedef struct {
    const char *heading;
    const char *items[ITEM_MAX_COUNT];
    int count;
} SelectScreen;

/* Item order equals ADVENTURE_*, GAME_MODE_*, and CSB_*. */
static const SelectScreen screens[] = {
    [SELECT_SCREEN_ADVENTURE] = {"ADVENTURE",          {"DUNGEON MASTER", "CHAOS STRIKES BACK"}, 2},
    [SELECT_SCREEN_MODE] = {"MODE",               {"VANILLA", "ADVANCED", "LLM"},           3},
    [SELECT_SCREEN_CSB] = {"CHAOS STRIKES BACK", {"NEW ADVENTURE", "RESUME", "QUIT"},      3},
};

/* The interface colors that the screens use: the KAOS palette is not loaded yet. */
static const uint16_t select_palette[PALETTE_COLORS] = {
    [COLOR_DARK_GRAY] = RGB_DARK_GRAY,
    [COLOR_LIGHT_GRAY] = RGB_LIGHT_GRAY,
    [COLOR_GOLD] = RGB_GOLD,
};

static int16_t screen = SELECT_SCREEN_DONE, cursor, csb_choice;
static bool csb_enabled, adventure_given, mode_given;
static bool llm_enabled;
static const char *message;
/* --adventure and --mode, -1 when absent */
static int16_t adventure_flag = -1;
static int16_t mode_flag = -1;

/* Only CHAOS STRIKES BACK can be disabled: its files are missing. */
static bool is_item_enabled(int item)
{
    return screen != SELECT_SCREEN_ADVENTURE || item != ADVENTURE_CSB || csb_enabled;
}

/* The items of screen s: the mode screen has no LLM item where the control socket is missing. */
static int item_count(int s)
{
    return screens[s].count - (s == SELECT_SCREEN_MODE && !llm_enabled);
}

/* Shows screen s with the cursor on the first item, which is always enabled. */
static void show(int s)
{
    screen = s;
    cursor = 0;
    message = NULL;
}

/* After the mode: the CSB entrance, or the end of the selection for DM. */
static void show_after_mode(void)
{
    show(g_adventure == ADVENTURE_CSB ? SELECT_SCREEN_CSB : SELECT_SCREEN_DONE);
}

static void set_mode(int16_t mode)
{
    game_set_mode(mode);
    g_advanced_features = mode != GAME_MODE_VANILLA;
}

void select_start(int16_t adventure, int16_t mode, bool csb_present, bool llm_mode_available)
{
    adventure_flag = adventure;
    mode_flag = mode;
    csb_enabled = csb_present;
    llm_enabled = llm_mode_available;
    adventure_given = adventure >= 0;
    mode_given = mode >= 0;
    g_adventure = adventure_given ? adventure : ADVENTURE_DM;
    if (mode_given)
        set_mode(mode);
    if (!adventure_given && !mode_given)
        show(SELECT_SCREEN_ADVENTURE);
    else if (!mode_given)
        show(SELECT_SCREEN_MODE);
    else
        show_after_mode();
}

void select_start_csb(const char *text)
{
    mode_given = true; /* the game mode is applied: Escape stays on the CSB screen */
    show(SELECT_SCREEN_CSB);
    message = text;
}

bool select_restart(void)
{
    select_start(adventure_flag, mode_flag, csb_enabled, llm_enabled);
    return screen != SELECT_SCREEN_DONE;
}

int16_t select_screen(void)
{
    return screen;
}
int16_t select_csb_choice(void)
{
    return csb_choice;
}

/* Runs the item at the cursor. */
static void activate(void)
{
    switch (screen) {
    case SELECT_SCREEN_ADVENTURE:
        g_adventure = cursor;
        show(SELECT_SCREEN_MODE);
        break;
    case SELECT_SCREEN_MODE:
        set_mode(cursor);
        show_after_mode();
        break;
    case SELECT_SCREEN_CSB:
        csb_choice = cursor; /* the program decides what QUIT does */
        show(SELECT_SCREEN_DONE);
        break;
    }
}

/* Moves the cursor by step to the next enabled item. It stops at the list ends. */
static void move(int step)
{
    for (int i = cursor + step; i >= 0 && i < item_count(screen); i += step)
        if (is_item_enabled(i)) {
            cursor = i;
            return;
        }
}

void select_key(int16_t action)
{
    if (screen == SELECT_SCREEN_DONE)
        return;
    message = NULL;
    if (action == SELECT_KEY_UP)
        move(-1);
    else if (action == SELECT_KEY_DOWN)
        move(1);
    else if (action == SELECT_KEY_ENTER)
        activate();
    else if (action == SELECT_KEY_ESCAPE && screen == SELECT_SCREEN_MODE && !adventure_given)
        show(SELECT_SCREEN_ADVENTURE);
    else if (action == SELECT_KEY_ESCAPE && screen == SELECT_SCREEN_CSB && !mode_given)
        show(SELECT_SCREEN_MODE);
}

bool select_item_box(int16_t item, Box *box)
{
    int16_t w, h;
    if (screen == SELECT_SCREEN_DONE || item < 0 || item >= item_count(screen))
        return false;
    if (!text_get_size(screens[screen].items[item], &w, &h))
        return false;
    box->w = w + 2 * ITEM_MARGIN;
    box->h = LINE_HEIGHT;
    box->x = (SCREEN_W - box->w) / 2;
    box->y = ITEM_TOP + item * ITEM_STEP;
    return true;
}

void select_click(int16_t x, int16_t y)
{
    Box b;
    for (int16_t i = 0; select_item_box(i, &b); i++) {
        if (!box_contains(&b, x, y))
            continue;
        if (is_item_enabled(i)) {
            cursor = i;
            activate();
        }
        return;
    }
}

void select_draw(void)
{
    Box b;
    memset(g_screen.px, COLOR_BLACK, sizeof g_screen.px);
    ui_set_screen_palette(select_palette);
    if (screen == SELECT_SCREEN_DONE)
        return;
    text_draw_box(&(Box){0, HEADING_TOP, SCREEN_W, LINE_HEIGHT}, true, COLOR_LIGHT_GRAY, COLOR_BLACK,
                  screens[screen].heading);
    for (int i = 0; select_item_box(i, &b); i++) {
        uint8_t color = !is_item_enabled(i) ? COLOR_DARK_GRAY : i == cursor ? COLOR_GOLD : COLOR_LIGHT_GRAY;
        text_draw_box(&b, true, color, COLOR_BLACK, screens[screen].items[i]);
    }
    if (message)
        text_draw_box(&(Box){0, MESSAGE_TOP, SCREEN_W, LINE_HEIGHT}, true, COLOR_LIGHT_GRAY, COLOR_BLACK, message);
}
