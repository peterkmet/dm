#include <string.h>
#include "select.h"
#include "advanced.h"
#include "adventure.h"
#include "text.h"
#include "ui.h"
#include "video.h"

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
} Screen;

/* Item order equals ADVENTURE_*, GAME_MODE_*, and CSB_*. */
static const Screen screens[] = {
    [SCREEN_ADVENTURE] = {"ADVENTURE",          {"DUNGEON MASTER", "CHAOS STRIKES BACK"}, 2},
    [SCREEN_MODE] = {"MODE",               {"VANILLA", "ADVANCED", "LLM"},           3},
    [SCREEN_CSB] = {"CHAOS STRIKES BACK", {"NEW ADVENTURE", "RESUME", "QUIT"},      3},
};

/* The interface colors that the screens use: the KAOS palette is not loaded yet. */
static const uint16_t select_palette[PALETTE_COLORS] = {
    [COLOR_DARK_GRAY] = RGB_DARK_GRAY,
    [COLOR_LIGHT_GRAY] = RGB_LIGHT_GRAY,
    [COLOR_GOLD] = RGB_GOLD,
};

static int screen = SCREEN_DONE, cursor, csb_enabled, adventure_given, mode_given, csb_choice;
static const char *message;
static int16_t adventure_flag = -1, mode_flag = -1; /* --adventure and --mode, -1 when absent */

/* Only CHAOS STRIKES BACK can be disabled: its files are missing. */
static int enabled(int item)
{
    return screen != SCREEN_ADVENTURE || item != ADVENTURE_CSB || csb_enabled;
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
    show(g_adventure == ADVENTURE_CSB ? SCREEN_CSB : SCREEN_DONE);
}

static void set_mode(int16_t mode)
{
    g_game_mode = mode;
    g_advanced_features = mode != GAME_MODE_VANILLA;
}

void select_start(int16_t adventure, int16_t mode, int csb_present)
{
    adventure_flag = adventure;
    mode_flag = mode;
    csb_enabled = csb_present;
    adventure_given = adventure >= 0;
    mode_given = mode >= 0;
    g_adventure = adventure_given ? adventure : ADVENTURE_DM;
    if (mode_given)
        set_mode(mode);
    if (!adventure_given && !mode_given)
        show(SCREEN_ADVENTURE);
    else if (!mode_given)
        show(SCREEN_MODE);
    else
        show_after_mode();
}

void select_start_csb(const char *text)
{
    mode_given = 1; /* the game mode is applied: Escape stays on the CSB screen */
    show(SCREEN_CSB);
    message = text;
}

int select_restart(void)
{
    select_start(adventure_flag, mode_flag, csb_enabled);
    return screen != SCREEN_DONE;
}

int select_screen(void)
{
    return screen;
}
int select_csb_choice(void)
{
    return csb_choice;
}

/* Runs the item at the cursor. */
static void activate(void)
{
    switch (screen) {
    case SCREEN_ADVENTURE:
        g_adventure = cursor;
        show(SCREEN_MODE);
        break;
    case SCREEN_MODE:
        set_mode(cursor);
        show_after_mode();
        break;
    case SCREEN_CSB:
        csb_choice = cursor; /* the program decides what QUIT does */
        show(SCREEN_DONE);
        break;
    }
}

/* Moves the cursor by step to the next enabled item. It stops at the list ends. */
static void move(int step)
{
    for (int i = cursor + step; i >= 0 && i < screens[screen].count; i += step)
        if (enabled(i)) {
            cursor = i;
            return;
        }
}

void select_key(int key)
{
    if (screen == SCREEN_DONE)
        return;
    message = NULL;
    if (key == SELECT_KEY_UP)
        move(-1);
    else if (key == SELECT_KEY_DOWN)
        move(1);
    else if (key == SELECT_KEY_ENTER)
        activate();
    else if (key == SELECT_KEY_ESCAPE && screen == SCREEN_MODE && !adventure_given)
        show(SCREEN_ADVENTURE);
    else if (key == SELECT_KEY_ESCAPE && screen == SCREEN_CSB && !mode_given)
        show(SCREEN_MODE);
}

int select_item_box(int item, Box *box)
{
    int w, h;
    if (screen == SCREEN_DONE || item < 0 || item >= screens[screen].count)
        return 0;
    text_size(screens[screen].items[item], &w, &h);
    box->w = w + 2 * ITEM_MARGIN;
    box->h = LINE_HEIGHT;
    box->x = (SCREEN_W - box->w) / 2;
    box->y = ITEM_TOP + item * ITEM_STEP;
    return 1;
}

void select_click(int x, int y)
{
    Box b;
    for (int i = 0; select_item_box(i, &b); i++) {
        if (x < b.x || x >= b.x + b.w || y < b.y || y >= b.y + b.h)
            continue;
        if (enabled(i)) {
            cursor = i;
            activate();
        }
        return;
    }
}

void select_draw(void)
{
    Bitmap bm = ui_screen();
    Box b;
    memset(fb, COLOR_BLACK, sizeof fb);
    ui_set_screen_palette(select_palette);
    if (screen == SCREEN_DONE)
        return;
    text_draw_box(&bm, 0, HEADING_TOP, SCREEN_W, LINE_HEIGHT, 1, COLOR_LIGHT_GRAY, COLOR_BLACK,
                  screens[screen].heading);
    for (int i = 0; select_item_box(i, &b); i++) {
        uint8_t color = !enabled(i) ? COLOR_DARK_GRAY : i == cursor ? COLOR_GOLD : COLOR_LIGHT_GRAY;
        text_draw_box(&bm, b.x, b.y, b.w, b.h, 1, color, COLOR_BLACK, screens[screen].items[i]);
    }
    if (message)
        text_draw_box(&bm, 0, MESSAGE_TOP, SCREEN_W, LINE_HEIGHT, 1, COLOR_LIGHT_GRAY, COLOR_BLACK, message);
}
