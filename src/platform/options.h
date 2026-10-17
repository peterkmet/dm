/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_PLATFORM_OPTIONS_H
#define DM_PLATFORM_OPTIONS_H
/* Prefixes: options_ */

/* The command line of the game: one table of options, --help, and an error for an unknown option or a bad
   value. No SDL, so the tests call the parser through the library. */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/* The options of one run; a field that the command line does not give keeps its "none" value. */
typedef struct {
    int64_t frames;             /* --frames: stop after this many frames; -1 runs until quit */
    int16_t graphic;            /* --show: the graphic to show; -1 none */
    bool view;                  /* --view: the dungeon view with free movement */
    int16_t adventure;          /* --adventure: ADVENTURE_*, -1 not given */
    int16_t mode;               /* --mode: GAME_MODE_*, -1 not given */
    const char *data_dir;       /* --data: the game files; NULL not given */
    const char *saves_dir;      /* --saves: the save folder; NULL not given */
    const char *control_socket; /* --control: the socket path; NULL not given */
    const char *filter;         /* --filter: none, scale2x, scale3x; NULL not given */
} Options;

/* Result of options_parse. */
typedef enum {
    OPTIONS_RUN,  /* options holds the run */
    OPTIONS_HELP, /* --help: print the help and stop */
    OPTIONS_ERROR /* an error line went to stderr: stop with exit code 1 */
} OptionsResult;

/* Reads argv into options. */
OptionsResult options_parse(int argc, char **argv, Options *options);
/* Prints one line per option with its value and meaning. */
void options_print_help(FILE *out);
/* The game mode of a --mode value, or -1. */
int16_t options_find_mode(const char *name);
/* The name of a game mode for the log. */
const char *options_get_mode_name(int16_t mode);

#endif
