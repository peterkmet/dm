/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "cheats.h"
#include "spells.h"
#include "tables.h"
#include "champion.h"
#include "game.h"
#include "inventory.h"
#include "magic.h"
#include "projectile.h"
#include "command.h"
#include "rows.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

/* The rows of the cheat page: the six cheats, then the items. */
#define ROW_LIGHT      0
#define ROW_IMMORTAL   1
#define ROW_FOOD       2
#define ROW_KILL       3
#define ROW_WALLS      4
#define ROW_MAP        5
#define ROW_QUICK_SAVE 6
#define ROW_QUICK_LOAD 7
#define ROW_SPELLS     8
#define ROW_CLOSE      9
#define PAGE_ROW_COUNT 10

/* The pages: the cheat page, then the spell pages with their spells and the NEXT, BACK and CLOSE rows. */
#define CHEAT_PAGE       0
#define SPELL_ITEM_COUNT 3
#define SPELL_ITEM_NEXT  0
#define SPELL_ITEM_BACK  1
#define SPELL_ITEM_CLOSE 2
#define SPELLS_PER_PAGE  (ROWS_MAX_COUNT - SPELL_ITEM_COUNT)

/* The text of a row and of a spell line. */
#define CHEAT_TEXT_SIZE     40
#define TITLE_SIZE          24
#define NUMBER_SIZE         4
#define CHEAT_LABEL_COLUMNS 20
#define ANY_POWER           "X"

/* The name of a spell, by its kind and type: the game files have no spell names. */
typedef struct {
    uint8_t kind, type;
    const char *name;
} SpellName;

static const SpellName spell_names[] = {
    {SPELL_KIND_POTION,     POTION_VEN_BOMB,             "VEN POTION"       },
    {SPELL_KIND_POTION,     POTION_ROS,                  "ROS POTION"       },
    {SPELL_KIND_POTION,     POTION_KU,                   "KU POTION"        },
    {SPELL_KIND_POTION,     POTION_DANE,                 "DANE POTION"      },
    {SPELL_KIND_POTION,     POTION_NETA,                 "NETA POTION"      },
    {SPELL_KIND_POTION,     POTION_ANTIVENIN,            "ANTIVENIN"        },
    {SPELL_KIND_POTION,     POTION_MON,                  "MON POTION"       },
    {SPELL_KIND_POTION,     POTION_YA,                   "YA POTION"        },
    {SPELL_KIND_POTION,     POTION_EE,                   "EE POTION"        },
    {SPELL_KIND_POTION,     POTION_VI,                   "VI POTION"        },
    {SPELL_KIND_PROJECTILE, EXPLOSION_FIREBALL,          "FIREBALL"         },
    {SPELL_KIND_PROJECTILE, EXPLOSION_LIGHTNING_BOLT,    "LIGHTNING BOLT"   },
    {SPELL_KIND_PROJECTILE, EXPLOSION_HARM_NON_MATERIAL, "HARM NON-MATERIAL"},
    {SPELL_KIND_PROJECTILE, EXPLOSION_OPEN_DOOR,         "OPEN DOOR"        },
    {SPELL_KIND_PROJECTILE, EXPLOSION_POISON_BOLT,       "POISON BOLT"      },
    {SPELL_KIND_PROJECTILE, EXPLOSION_POISON_CLOUD,      "POISON CLOUD"     },
    {SPELL_KIND_OTHER,      SPELL_OTHER_LIGHT,           "LIGHT"            },
    {SPELL_KIND_OTHER,      SPELL_OTHER_DARKNESS,        "DARKNESS"         },
    {SPELL_KIND_OTHER,      SPELL_OTHER_THIEVES_EYE,     "SEE THROUGH WALLS"},
    {SPELL_KIND_OTHER,      SPELL_OTHER_INVISIBILITY,    "INVISIBILITY"     },
    {SPELL_KIND_OTHER,      SPELL_OTHER_PARTY_SHIELD,    "PARTY SHIELD"     },
    {SPELL_KIND_OTHER,      SPELL_OTHER_MAGIC_TORCH,     "MAGIC TORCH"      },
    {SPELL_KIND_OTHER,      SPELL_OTHER_FOOTPRINTS,      "FOOTPRINTS"       },
    {SPELL_KIND_OTHER,      SPELL_OTHER_ZOKATHRA,        "ZOKATHRA"         },
    {SPELL_KIND_OTHER,      SPELL_OTHER_FIRE_SHIELD,     "FIRE SHIELD"      },
    {SPELL_KIND_MAGIC_MAP,  MAGIC_MAP_DAIN,              "MAP DETAILS"      },
    {SPELL_KIND_MAGIC_MAP,  MAGIC_MAP_ROS,               "MAP TRAPS"        },
    {SPELL_KIND_MAGIC_MAP,  MAGIC_MAP_KU,                "MAP CREATURES"    },
    {SPELL_KIND_MAGIC_MAP,  MAGIC_MAP_NETA,              "MAP HOLD"         },
};

static const char *const cheat_labels[ROW_QUICK_SAVE] = {"FULL LIGHT",   "IMMORTAL",           "FOOD AND WATER",
                                                         "ONE-HIT KILL", "WALK THROUGH WALLS", "REVEAL MAP"};
static const uint8_t cheat_bits[ROW_QUICK_SAVE] = {CHEAT_LIGHT, CHEAT_IMMORTAL, CHEAT_FOOD,
                                                   CHEAT_KILL,  CHEAT_WALLS,    CHEAT_MAP};
static const char *const spell_items[SPELL_ITEM_COUNT] = {"NEXT", "BACK", "CLOSE"};

static struct {
    int16_t page; /* CHEAT_PAGE, or a spell page from 1 */
    int16_t row;  /* the selected row */
} screen;

/* The spells of the loaded game. */
static int16_t count_spells(void)
{
    int16_t count = 0;
    while (tables_get_spell(count))
        count++;
    return count;
}

static int16_t count_spell_pages(void)
{
    return (count_spells() + SPELLS_PER_PAGE - 1) / SPELLS_PER_PAGE;
}

/* The spells on the shown spell page. */
static int16_t count_page_spells(void)
{
    int16_t left = count_spells() - (screen.page - 1) * SPELLS_PER_PAGE;
    return left < SPELLS_PER_PAGE ? left : SPELLS_PER_PAGE;
}

/* The rows of the shown page. */
static int16_t count_rows(void)
{
    return screen.page == CHEAT_PAGE ? PAGE_ROW_COUNT : count_page_spells() + SPELL_ITEM_COUNT;
}

/* Appends text to the line of size bytes; a full line keeps its start. */
static void append_text(char *line, size_t size, const char *text)
{
    size_t length = strlen(line);
    snprintf(line + length, size - length, "%s", text);
}

/* The port name of a spell, or "?" for a spell that the table does not know. */
static const char *get_spell_name(const Spell *spell)
{
    for (size_t i = 0; i < sizeof spell_names / sizeof *spell_names; i++)
        if (spell_names[i].kind == spell->kind && spell_names[i].type == spell->type)
            return spell_names[i].name;
    return "?";
}

/* Queries */

void cheats_get_spell_line(const Spell *spell, char *line, size_t size)
{
    char numbers[CHEAT_TEXT_SIZE] = "(";
    line[0] = '\0';
    for (uint16_t step = 0; step < SYMBOL_STEP_COUNT; step++) {
        uint8_t symbol = spell_get_packed_symbol(spell->symbols, step);
        if (!symbol && step)
            break;
        char number[NUMBER_SIZE] = ANY_POWER; /* a spell without a power symbol takes any power */
        if (symbol) {
            int16_t index = symbol - SPELL_SYMBOL_FIRST;
            if (line[0])
                append_text(line, size, " ");
            append_text(line, size, g_rune_names[index]);
            snprintf(number, sizeof number, "%d", index % SPELL_SYMBOLS_PER_STEP + 1);
        }
        if (step)
            append_text(numbers, sizeof numbers, ",");
        append_text(numbers, sizeof numbers, number);
    }
    append_text(line, size, " ");
    append_text(line, size, numbers);
    append_text(line, size, ") ");
    append_text(line, size, get_spell_name(spell));
}

/* Commands */

/* Draws the cheat page with the state of each cheat. */
static void show_cheat_page(void)
{
    char texts[ROW_QUICK_SAVE][CHEAT_TEXT_SIZE];
    const char *rows[PAGE_ROW_COUNT] = {[ROW_QUICK_SAVE] = "QUICK SAVE F5",
                                        [ROW_QUICK_LOAD] = "QUICK LOAD F9",
                                        [ROW_SPELLS] = "SPELLS",
                                        [ROW_CLOSE] = "CLOSE"};
    uint8_t cheats = game_get_cheats();
    for (int16_t i = ROW_LIGHT; i < ROW_QUICK_SAVE; i++) {
        snprintf(texts[i], CHEAT_TEXT_SIZE, "%-*s%s", CHEAT_LABEL_COLUMNS, cheat_labels[i],
                 cheats & cheat_bits[i] ? "ON" : "OFF");
        rows[i] = texts[i];
    }
    rows_draw("CHEATS", rows, PAGE_ROW_COUNT, screen.row);
}

/* Draws the shown spell page: its spells, then NEXT, BACK and CLOSE. */
static void show_spell_page(void)
{
    char title[TITLE_SIZE], texts[SPELLS_PER_PAGE][CHEAT_TEXT_SIZE];
    const char *rows[ROWS_MAX_COUNT];
    int16_t count = count_page_spells();
    int16_t first = (screen.page - 1) * SPELLS_PER_PAGE;
    for (int16_t i = 0; i < count; i++) {
        cheats_get_spell_line(tables_get_spell(first + i), texts[i], CHEAT_TEXT_SIZE);
        rows[i] = texts[i];
    }
    for (int16_t i = 0; i < SPELL_ITEM_COUNT; i++)
        rows[count + i] = spell_items[i];
    snprintf(title, sizeof title, "SPELLS %d/%d", screen.page, count_spell_pages());
    rows_draw(title, rows, count + SPELL_ITEM_COUNT, screen.row);
}

/* Draws the shown page. */
static void show_page(void)
{
    if (screen.page == CHEAT_PAGE)
        show_cheat_page();
    else
        show_spell_page();
}

/* Shows a page with the row selected; a spell page selects its NEXT row for -1. */
static void open_page(int16_t page, int16_t row)
{
    screen.page = page;
    screen.row = row >= 0 ? row : count_page_spells() + SPELL_ITEM_NEXT;
    show_page();
}

void cheats_open(void)
{
    open_page(CHEAT_PAGE, ROW_LIGHT);
}

/* Runs a row of a spell page: a spell line casts the spell for the spell caster and closes the screen, so the
   player sees the effect; NEXT goes to the next spell page (after the last, the first), BACK to the page before
   (the cheat page before the first spell page). */
static CheatsResult run_spell_row(int16_t row)
{
    int16_t item = row - count_page_spells();
    if (item < 0) {
        magic_cast_cheat_spell(g_magic_caster_champion_index, (screen.page - 1) * SPELLS_PER_PAGE + row);
        return CHEATS_CLOSE;
    }
    if (item == SPELL_ITEM_NEXT)
        open_page(screen.page % count_spell_pages() + 1, -1);
    else if (item == SPELL_ITEM_BACK)
        open_page(screen.page - 1, screen.page == 1 ? ROW_SPELLS : -1);
    else if (item == SPELL_ITEM_CLOSE)
        return CHEATS_CLOSE;
    return CHEATS_STAY;
}

/* Runs the selected row: a cheat row switches its cheat, an item row runs its item. */
static CheatsResult run_row(int16_t row)
{
    if (screen.page != CHEAT_PAGE)
        return run_spell_row(row);
    switch (row) {
    case ROW_QUICK_SAVE:
        return CHEATS_QUICK_SAVE;
    case ROW_QUICK_LOAD:
        return CHEATS_QUICK_LOAD;
    case ROW_SPELLS:
        open_page(1, -1);
        return CHEATS_STAY;
    case ROW_CLOSE:
        return CHEATS_CLOSE;
    }
    game_toggle_cheat(cheat_bits[row]);
    show_page();
    return CHEATS_STAY;
}

CheatsResult cheats_process_key(uint16_t key)
{
    switch (key) {
    case KEY_C:
    case KEY_ESCAPE:
        return CHEATS_CLOSE;
    case KEY_F5:
        return CHEATS_QUICK_SAVE;
    case KEY_F9:
        return CHEATS_QUICK_LOAD;
    case KEY_RETURN:
        return run_row(screen.row);
    case KEY_UP:
        if (screen.row > 0)
            screen.row--;
        break;
    case KEY_DOWN:
        if (screen.row < count_rows() - 1)
            screen.row++;
        break;
    default:
        return CHEATS_STAY;
    }
    show_page();
    return CHEATS_STAY;
}

CheatsResult cheats_process_click(int16_t x, int16_t y)
{
    int16_t row = rows_find_row(x, y, count_rows());
    if (row < 0)
        return CHEATS_STAY;
    screen.row = row;
    show_page();
    return run_row(row);
}
