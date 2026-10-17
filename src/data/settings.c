/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "settings.h"
#include "adventure.h"
#include "file.h"
#include <stdio.h>
#include <string.h>

/* The text of the settings file. */
#define LINE_SIZE      64 /* longer lines are unreadable */
#define TEMPORARY_FILE "SETTINGS.TMP"

static const char *const music_names[SETTINGS_MUSIC_COUNT] = {"original", "super-nes", "fm-towns"};

/* The volume step of a value text, or -1 when it is not a step. */
static int16_t parse_volume(const char *value)
{
    if (strlen(value) != 1 || value[0] < '0' || value[0] > '0' + SETTINGS_VOLUME_FULL)
        return -1;
    return (int16_t)(value[0] - '0');
}

/* The music choice of a value text, or -1 for an unknown name. */
static int16_t parse_music(const char *value)
{
    for (int16_t i = 0; i < SETTINGS_MUSIC_COUNT; i++)
        if (!strcmp(value, music_names[i]))
            return i;
    return -1;
}

/* Sets the value of one line key=value; an unknown key or a bad value changes nothing. */
static void parse_line(char *line, Settings *settings)
{
    char *value = strchr(line, '=');
    if (!value)
        return;
    *value++ = '\0';
    if (!strcmp(line, "sound_volume") && parse_volume(value) >= 0)
        settings->sound_volume = parse_volume(value);
    else if (!strcmp(line, "music_volume") && parse_volume(value) >= 0)
        settings->music_volume = parse_volume(value);
    else if (!strcmp(line, "music") && parse_music(value) >= 0)
        settings->music = parse_music(value);
}

/* Queries */

Settings settings_parse(const char *file_text, size_t length)
{
    Settings settings = {SETTINGS_VOLUME_FULL, SETTINGS_VOLUME_FULL, SETTINGS_MUSIC_ORIGINAL};
    if (length > SETTINGS_MAX_SIZE)
        return settings;
    char line[LINE_SIZE];
    size_t used = 0;
    bool too_long = false;
    for (size_t i = 0; i <= length; i++) {
        char c = i < length ? file_text[i] : '\n'; /* the last line needs no final newline */
        if (c != '\n') {
            too_long |= used + 1 >= sizeof line;
            if (!too_long && c != '\r')
                line[used++] = c;
            continue;
        }
        line[used] = '\0';
        if (!too_long)
            parse_line(line, &settings);
        used = 0;
        too_long = false;
    }
    if (settings.music == SETTINGS_MUSIC_SUPER_NES && g_adventure == ADVENTURE_CSB)
        settings.music = SETTINGS_MUSIC_ORIGINAL; /* CSB has no Super NES soundtrack */
    return settings;
}

Settings settings_load(const char *dir)
{
    char path[PATH_SIZE], text[SETTINGS_MAX_SIZE + 1];
    size_t length = 0;
    if (snprintf(path, sizeof path, "%s/%s", dir, SETTINGS_FILE) < (int)sizeof path) {
        FILE *file = fopen(path, "rb");
        if (file) {
            length = fread(text, 1, sizeof text, file); /* one byte above the limit tells a file that is too large */
            fclose(file);
        }
    }
    return settings_parse(text, length);
}

/* Commands */

bool settings_save(const char *dir, const Settings *settings)
{
    char path[PATH_SIZE], temporary[PATH_SIZE], text[SETTINGS_MAX_SIZE];
    if (snprintf(path, sizeof path, "%s/%s", dir, SETTINGS_FILE) >= (int)sizeof path ||
        snprintf(temporary, sizeof temporary, "%s/%s", dir, TEMPORARY_FILE) >= (int)sizeof temporary)
        return false;
    int length = snprintf(text, sizeof text, "sound_volume=%d\nmusic_volume=%d\nmusic=%s\n", settings->sound_volume,
                          settings->music_volume, music_names[settings->music]);
    return file_write(temporary, (const uint8_t *)text, (size_t)length) && file_replace(temporary, path);
}
