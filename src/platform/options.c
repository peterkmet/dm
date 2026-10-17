/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "platform/options.h"
#include "data/adventure.h"
#include "data/bytes.h"
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* Kinds of option values. */
#define VALUE_NONE      0 /* a flag */
#define VALUE_TEXT      1
#define VALUE_COUNT     2 /* an int64_t of 0 or more */
#define VALUE_INDEX     3 /* an int16_t of 0 or more */
#define VALUE_MODE      4 /* a game mode name */
#define VALUE_ADVENTURE 5 /* an adventure name */

typedef struct {
    const char *name;
    int16_t value; /* VALUE_* */
    size_t field;  /* offsetof the target in Options */
    const char *help;
} Option;

static const Option options_table[] = {
    {"--data", VALUE_TEXT, offsetof(Options, data_dir), "DIR   the game files (DM; CSB with --adventure csb)"},
    {"--saves", VALUE_TEXT, offsetof(Options, saves_dir), "DIR   the folder of the saved games"},
    {"--adventure", VALUE_ADVENTURE, offsetof(Options, adventure), "dm|csb  skip the adventure screen"},
    {"--mode", VALUE_MODE, offsetof(Options, mode), "vanilla|advanced|turnbased  skip the mode screen"},
    {"--control", VALUE_TEXT, offsetof(Options, control_socket), "PATH  the control socket of the turn-based mode"},
    {"--filter", VALUE_TEXT, offsetof(Options, filter), "none|scale2x|scale3x  the pixel-art upscale filter"},
    {"--frames", VALUE_COUNT, offsetof(Options, frames), "N     stop after N frames"},
    {"--show", VALUE_INDEX, offsetof(Options, graphic), "N     show graphic N of Graphics.DAT (developer)"},
    {"--view", VALUE_NONE, offsetof(Options, view), "      the dungeon view with free movement (developer)"},
    {"--help", VALUE_NONE, 0, "      print this help"},
};

/* The game mode names in GAME_MODE_* order. */
static const char *const mode_names[] = {"vanilla", "advanced", "turnbased"};

/* The table entry of an option name, or NULL. */
static const Option *find_option(const char *name)
{
    for (int i = 0; i < ARRAY_COUNT(options_table); i++)
        if (!strcmp(name, options_table[i].name))
            return &options_table[i];
    return NULL;
}

/* Reads a decimal number from 0 to max; false for other text. */
static bool read_number(const char *text, int64_t max, int64_t *number)
{
    char *end;
    errno = 0;
    int64_t value = strtoll(text, &end, 10);
    if (end == text || *end || errno || value < 0 || value > max)
        return false;
    *number = value;
    return true;
}

/* Writes the value text of one option into its field; false with an error line for a bad value. */
static bool write_value(const Option *option, const char *text, Options *options)
{
    unsigned char *target = (unsigned char *)options + option->field; /* the field at its offset */
    int64_t number;
    int16_t small;
    switch (option->value) {
    case VALUE_TEXT:
        memcpy(target, &text, sizeof text);
        return true;
    case VALUE_COUNT:
    case VALUE_INDEX:
        if (!read_number(text, option->value == VALUE_COUNT ? INT64_MAX : INT16_MAX, &number)) {
            fprintf(stderr, "%s needs a number of 0 or more\n", option->name);
            return false;
        }
        if (option->value == VALUE_COUNT) {
            memcpy(target, &number, sizeof number);
            return true;
        }
        small = (int16_t)number; /* read_number checked the range */
        memcpy(target, &small, sizeof small);
        return true;
    case VALUE_MODE:
        small = options_find_mode(text);
        if (small < 0) {
            fprintf(stderr, "--mode must be vanilla, advanced, or turnbased\n");
            return false;
        }
        memcpy(target, &small, sizeof small);
        return true;
    default: /* VALUE_ADVENTURE */
        small = adventure_find(text);
        if (small < 0) {
            fprintf(stderr, "--adventure must be dm or csb\n");
            return false;
        }
        memcpy(target, &small, sizeof small);
        return true;
    }
}

OptionsResult options_parse(int argc, char **argv, Options *options)
{
    *options = (Options){.frames = -1, .graphic = -1, .adventure = -1, .mode = -1};
    for (int i = 1; i < argc; i++) {
        const Option *option = find_option(argv[i]);
        if (!option) {
            fprintf(stderr, "unknown option: %s\n", argv[i]);
            return OPTIONS_ERROR;
        }
        if (!strcmp(option->name, "--help"))
            return OPTIONS_HELP;
        if (option->value == VALUE_NONE) {
            bool on = true;
            memcpy((unsigned char *)options + option->field, &on, sizeof on); /* the flag at its offset */
            continue;
        }
        if (i + 1 >= argc) {
            fprintf(stderr, "%s needs a value\n", option->name);
            return OPTIONS_ERROR;
        }
        i++;
        if (!write_value(option, argv[i], options))
            return OPTIONS_ERROR;
    }
    return OPTIONS_RUN;
}

void options_print_help(FILE *out)
{
    fprintf(out, "Usage: dm [options]\n");
    for (int i = 0; i < ARRAY_COUNT(options_table); i++)
        fprintf(out, "  %-12s %s\n", options_table[i].name, options_table[i].help);
}

int16_t options_find_mode(const char *name)
{
    for (int16_t mode = 0; mode < ARRAY_COUNT(mode_names); mode++)
        if (!strcmp(name, mode_names[mode]))
            return mode;
    return -1;
}

const char *options_get_mode_name(int16_t mode)
{
    return mode_names[mode];
}
