/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "data/csb_text.h"
#include "data/bytes.h"
#include "data/ids.h"
#include "data/kaos_addresses.h"
#include "data/tables.h"
#include <stdlib.h>
#include <string.h>

/* APPB addresses of the CSB texts and the DM text index of each. Sources and code references:
   docs/csb/engine-diff.md "Text". */
#define CSB_TEXT_COUNT (TEXT_CREATURE_NAME_FIRST + CREATURE_TYPE_COUNT) /* CSB has no memory error texts 135-139 */
#define CSB_APPB_ACTION_NAMES  0x1048b4                                 /* NUL-separated action names, index 0 = "N" */
#define CSB_APPB_DISK_MESSAGES 0x104d08 /* pointers to the load, save, and disk messages */
#define CSB_APPB_CLASS_NAMES   0x102d56 /* pointers to the class names, in base skill order */

/* Parts of a joined text: a class text is a prefix, the class name and a suffix; the other texts have one part. */
#define PART_PREFIX 0
#define PART_NAME   1
#define PART_SUFFIX 2
#define TEXT_PARTS  3

/* Single strings: DM text index, APPB address. */
static const struct {
    int16_t text;
    uint32_t appb;
} single_texts[] = {
    {TEXT_WAKE_UP,                       0x104680},
    {TEXT_GAME_FROZEN,                   0x104688},
    {TEXT_PARTY_FACING,                  0x1026ae},
    {TEXT_CONSUMABLE,                    0x10267c},
    {TEXT_CONSUMABLE + 1,                0x102688}, /* POISONED */
    {TEXT_CONSUMABLE + 2,                0x102692}, /* BROKEN */
    {TEXT_CONSUMABLE + 3,                0x10269a}, /* CURSED */
    {TEXT_AND,                           0x10264e},
    {TEXT_BURNT_OUT,                     0x1026a2},
    {TEXT_WATERSKIN_EMPTY,               0x1026bc},
    {TEXT_WATERSKIN_EMPTY + 1,           0x1026c4}, /* (ALMOST EMPTY) */
    {TEXT_WATERSKIN_EMPTY + 2,           0x1026d4}, /* (ALMOST FULL) */
    {TEXT_WATERSKIN_EMPTY + 3,           0x1026e2}, /* (FULL) */
    {TEXT_LOAD,                          0x102dc2},
    {TEXT_WEIGHS,                        0x1026ee},
    {TEXT_DECIMAL_POINT,                 0x1026f6},
    {TEXT_KG_PERIOD,                     0x1026f8},
    {TEXT_KG,                            0x102dcc},
    {TEXT_HEALTH,                        0x102740},
    {TEXT_STAMINA,                       0x102748},
    {TEXT_MANA,                          0x102750},
    {TEXT_SPELL_FAILURE_MEANINGLESS + 1, 0x104ba2},
    {TEXT_SPELL_FAILURE_FLASK + 1,       0x104bc0},
    {TEXT_SPELL_FAILURE_MAGIC_MAP + 1,   0x104bea},
    {TEXT_FLIP_HEADS,                    0x104b2a},
    {TEXT_FLIP_TAILS,                    0x104b3e},
    {TEXT_CANT_REACH,                    0x104b64},
    {TEXT_NEED_AMMO,                     0x104b70},
    {TEXT_RESURRECTED + 1,               0x102dec},
    {TEXT_REINCARNATED + 1,              0x102dfa},
    {TEXT_RESTART_THIS_GAME,             0x104c56},
    {TEXT_NO_CREATURE,                   0x102654},
};

/* Pointer tables: DM text index of the first string, APPB table, count, and the KAOS string index
   table that gets the DM text indices, or 0. */
static const struct {
    int16_t text;
    uint32_t appb;
    uint8_t count;
    uint32_t kaos_indices;
} table_texts[] = {
    {TEXT_CLASS_FIRST,         CSB_APPB_CLASS_NAMES, SKILL_BASE_COUNT,
     KAOS_BASE_SKILL_STRING_INDICES                                                                       }, /* FIGHTER, NINJA, PRIEST, WIZARD */
    {TEXT_SKILL_LEVEL_FIRST,   0x102524,             TEXT_SKILL_LEVEL_COUNT,
     KAOS_SKILL_LEVEL_STRING_INDICES                                                                      }, /* NEOPHYTE ... ARCHMASTER */
    {TEXT_DIRECTION_FIRST,     0x102578,             4,                      KAOS_DIRECTION_STRING_INDICES}, /* NORTH, EAST, SOUTH, WEST */
    {TEXT_STATISTIC_FIRST,     0x102588,             7,                      KAOS_STATISTIC_STRING_INDICES}, /* L, STRENGTH ... ANTI-FIRE */
    {TEXT_CREATURE_NAME_FIRST, 0x102164,             CREATURE_TYPE_COUNT,    0                            },
};

/* DM text index of each pointer at CSB_APPB_DISK_MESSAGES. TEXT_SAVE and TEXT_QUIT are SAVE AND PLAY and SAVE AND QUIT. */
static const int16_t disk_message_texts[] = {
    TEXT_GAME_LOADED,
    TEXT_CANT_MODIFY_GAME_DISK,
    TEXT_NOT_MASTER_DISK,
    TEXT_CANT_FIND_SAVED_GAME,
    TEXT_UNABLE_TO_SAVE,
    TEXT_UNABLE_TO_FORMAT,
    TEXT_IS_GAME_DISK,
    TEXT_GAME_SAVE_DISK,
    TEXT_WRITE_PROTECTED,
    TEXT_DISK_UNREADABLE,
    TEXT_NOT_SAME_GAME,
    TEXT_GAME_DAMAGED,
    TEXT_PUT_SAVE_DISK,
    TEXT_PUT_GAME_DISK,
    TEXT_PUT_BLANK_DISK,
    TEXT_SAVING_GAME,
    TEXT_LOADING_GAME,
    TEXT_FORMATTING_DISK,
    TEXT_FORMAT_ANYWAY,
    TEXT_NO_DISK_IN_DRIVE,
    TEXT_LOAD_SAVED_GAME,
    TEXT_SAVE,
    TEXT_QUIT,
    TEXT_OK,
    TEXT_CANCEL,
    TEXT_FORMAT_FLOPPY,
};

/* Texts around a class name: DM text index, prefix, base skill, suffix. */
static const struct {
    int16_t text;
    uint32_t prefix;
    uint8_t skill;
    uint32_t suffix;
} class_texts[] = {
    {TEXT_SPELL_FAILURE_WIZARD + 1, 0x104b7a, SKILL_WIZARD,
     0x104b9a                                                        }, /* " NEEDS MORE PRACTICE WITH THIS " WIZARD " SPELL." */
    {TEXT_SPELL_FAILURE_PRIEST + 1, 0x104b7a, SKILL_PRIEST,  0x104b9a},
    {TEXT_LEVEL_GAINED + 1,         0x102d3e, SKILL_FIGHTER, 0x102d4e}, /* " JUST GAINED A " FIGHTER " LEVEL!" */
    {TEXT_LEVEL_GAINED + 3,         0x102d3e, SKILL_NINJA,   0x102d4e},
    {TEXT_LEVEL_GAINED + 5,         0x102d3e, SKILL_PRIEST,  0x102d4e},
    {TEXT_LEVEL_GAINED + 7,         0x102d3e, SKILL_WIZARD,  0x102d4e},
};

static uint8_t *strings, *action_names;
static uint16_t strings_size, action_names_size;

/* The NUL-terminated string at an APPB address, or NULL outside the module. */
static const char *get_appb_string(const FtlImage *appb, uint32_t addr)
{
    if (addr < appb->base || addr >= appb->base + appb->size)
        return NULL;
    const char *s = (const char *)appb->data + (addr - appb->base);
    return memchr(s, 0, appb->base + appb->size - addr) ? s : NULL;
}

/* The string that the APPB pointer at addr points to, or NULL. */
static const char *get_appb_pointed_string(const FtlImage *appb, uint32_t addr)
{
    const uint8_t *p = ftl_at(appb, addr, sizeof(uint32_t));
    return p ? get_appb_string(appb, be32(p)) : NULL;
}

/* Joins the parts of each text into one block of NUL-terminated strings. */
static bool join_texts(const char *parts[CSB_TEXT_COUNT][TEXT_PARTS])
{
    size_t size = CSB_TEXT_COUNT;
    for (int t = 0; t < CSB_TEXT_COUNT; t++)
        for (int k = 0; k < TEXT_PARTS; k++)
            size += parts[t][k] ? strlen(parts[t][k]) : 0;
    free(strings);
    strings = malloc(size);
    if (!strings)
        return false;
    uint8_t *o = strings;
    for (int t = 0; t < CSB_TEXT_COUNT; t++) {
        for (int k = 0; k < TEXT_PARTS; k++) {
            size_t n = parts[t][k] ? strlen(parts[t][k]) : 0;
            memcpy(o, parts[t][k] ? parts[t][k] : "", n);
            o += n;
        }
        *o++ = 0;
    }
    strings_size = size;
    return true;
}

/* Copies the action names list. */
static bool copy_action_names(const FtlImage *appb)
{
    uint32_t end = CSB_APPB_ACTION_NAMES;
    for (int i = 0; i < ACTION_COUNT; i++) {
        const char *s = get_appb_string(appb, end);
        if (!s)
            return false;
        end += (uint32_t)strlen(s) + 1;
    }
    free(action_names);
    action_names_size = end - CSB_APPB_ACTION_NAMES;
    action_names = malloc(action_names_size);
    if (!action_names)
        return false;
    const uint8_t *names = ftl_at(appb, CSB_APPB_ACTION_NAMES, action_names_size);
    if (!names) {
        free(action_names);
        action_names = NULL;
        return false;
    }
    memcpy(action_names, names, action_names_size);
    return true;
}

bool csb_text_build(const FtlImage *appb, FtlImage *out)
{
    const char *parts[CSB_TEXT_COUNT][TEXT_PARTS] = {{0}};
    for (int i = 0; i < ARRAY_COUNT(single_texts); i++)
        if (!(parts[single_texts[i].text][0] = get_appb_string(appb, single_texts[i].appb)))
            return false;
    for (int i = 0; i < ARRAY_COUNT(table_texts); i++) {
        for (int k = 0; k < table_texts[i].count; k++) {
            int16_t text = table_texts[i].text + k;
            if (!(parts[text][0] = get_appb_pointed_string(appb, table_texts[i].appb + 4 * k)))
                return false;
            /* the string index table gives the text in the CSB order */
            if (table_texts[i].kaos_indices) {
                uint8_t *p = ftl_at(out, table_texts[i].kaos_indices + 2 * k, sizeof(uint16_t));
                if (!p)
                    return false;
                wr16(p, text);
            }
        }
    }
    for (int i = 0; i < ARRAY_COUNT(disk_message_texts); i++)
        if (!(parts[disk_message_texts[i]][0] = get_appb_pointed_string(appb, CSB_APPB_DISK_MESSAGES + 4 * i)))
            return false;
    for (int i = 0; i < ARRAY_COUNT(class_texts); i++) {
        const char **p = parts[class_texts[i].text];
        p[PART_PREFIX] = get_appb_string(appb, class_texts[i].prefix);
        p[PART_NAME] = get_appb_pointed_string(appb, CSB_APPB_CLASS_NAMES + 4 * class_texts[i].skill);
        p[PART_SUFFIX] = get_appb_string(appb, class_texts[i].suffix);
        if (!p[PART_PREFIX] || !p[PART_NAME] || !p[PART_SUFFIX])
            return false;
    }
    return join_texts(parts) && copy_action_names(appb);
}

const uint8_t *csb_text_get_item(int graphic, uint16_t *size)
{
    if (graphic == GFX_STRINGS) {
        *size = strings_size;
        return strings;
    }
    if (graphic == GFX_ACTION_NAMES) {
        *size = action_names_size;
        return action_names;
    }
    return NULL;
}
