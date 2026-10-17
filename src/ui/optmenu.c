/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "optmenu.h"
#include "adventure.h"
#include "settings.h"
#include "advanced.h"
#include "audio.h"
#include "command.h"
#include "music.h"
#include "rows.h"
#include "startend.h"
#include <stdio.h>

/* The rows of the menu. */
#define ROW_SOUND        0
#define ROW_MUSIC_VOLUME 1
#define ROW_MUSIC        2
#define ROW_CONTINUE     3
#define ROW_COUNT        4

/* The text of a row: the label column, the - and + columns of a volume row, the columns a click on them takes. */
#define ROW_TEXT_SIZE 40
#define LABEL_COLUMNS 14
#define MINUS_COLUMN  14
#define PLUS_COLUMN   20
#define SIGN_REACH    1 /* columns on each side of a sign that a click also takes */

static const char *const music_names[SETTINGS_MUSIC_COUNT] = {"ORIGINAL", "SUPER NES", "FM-TOWNS"};

static struct {
    int16_t row;     /* the selected row */
    Settings opened; /* the values when the menu opened */
} menu;

/* The values in use. */
static Settings get_current(void)
{
    return (Settings){g_audio_volume, g_music_volume, g_music_soundtrack};
}

/* Draws the menu with the current values. */
static void show_menu(void)
{
    char texts[ROW_COUNT][ROW_TEXT_SIZE];
    snprintf(texts[ROW_SOUND], ROW_TEXT_SIZE, "%-*s-  %d  +", LABEL_COLUMNS, "SOUND", g_audio_volume);
    snprintf(texts[ROW_MUSIC_VOLUME], ROW_TEXT_SIZE, "%-*s-  %d  +", LABEL_COLUMNS, "MUSIC VOLUME", g_music_volume);
    snprintf(texts[ROW_MUSIC], ROW_TEXT_SIZE, "%-*s%s", LABEL_COLUMNS, "MUSIC", music_names[g_music_soundtrack]);
    snprintf(texts[ROW_CONTINUE], ROW_TEXT_SIZE, "CONTINUE");
    const char *rows[ROW_COUNT] = {texts[ROW_SOUND], texts[ROW_MUSIC_VOLUME], texts[ROW_MUSIC], texts[ROW_CONTINUE]};
    rows_draw("OPTIONS", rows, ROW_COUNT, menu.row);
}

/* Commands */

void optmenu_open(void)
{
    menu.row = ROW_SOUND;
    menu.opened = get_current();
    show_menu();
}

/* Writes the settings file when a value changed since the menu opened; a failed write keeps the values. */
static void save_changes(void)
{
    Settings now = get_current();
    if (now.sound_volume == menu.opened.sound_volume && now.music_volume == menu.opened.music_volume &&
        now.music == menu.opened.music)
        return;
    if (!settings_save(startend_save_dir(), &now))
        advanced_print_message("UNABLE TO SAVE OPTIONS");
}

/* The volume one step lower (-1) or higher (1), within the steps. */
static int16_t step_volume(int16_t volume, int16_t step)
{
    int16_t next = volume + step;
    return next < SETTINGS_VOLUME_OFF ? SETTINGS_VOLUME_OFF : next > SETTINGS_VOLUME_FULL ? SETTINGS_VOLUME_FULL : next;
}

/* The music before (-1) or after (1) the current one, with wrap; CSB has no Super NES soundtrack. */
static int16_t step_music(int16_t step)
{
    int16_t music = g_music_soundtrack;
    do
        music = (music + step + SETTINGS_MUSIC_COUNT) % SETTINGS_MUSIC_COUNT;
    while (music == MUSIC_SUPER_NES && g_adventure == ADVENTURE_CSB);
    return music;
}

/* Changes the value of the selected row one step down (-1) or up (1). */
static void change_value(int16_t step)
{
    if (menu.row == ROW_SOUND)
        g_audio_volume = step_volume(g_audio_volume, step);
    else if (menu.row == ROW_MUSIC_VOLUME)
        g_music_volume = step_volume(g_music_volume, step);
    else if (menu.row == ROW_MUSIC)
        music_select(step_music(step));
}

bool optmenu_process_key(uint16_t key)
{
    switch (key) {
    case KEY_ESCAPE:
        save_changes();
        return true;
    case KEY_RETURN:
        if (menu.row != ROW_CONTINUE)
            return false;
        save_changes();
        return true;
    case KEY_UP:
        if (menu.row > ROW_SOUND)
            menu.row--;
        break;
    case KEY_DOWN:
        if (menu.row < ROW_CONTINUE)
            menu.row++;
        break;
    case KEY_LEFT:
    case KEY_RIGHT:
        change_value(key == KEY_LEFT ? -1 : 1);
        break;
    default:
        return false;
    }
    show_menu();
    return false;
}

/* True when the text column is on the sign column or next to it. */
static bool is_near_column(int16_t column, int16_t sign)
{
    return column >= sign - SIGN_REACH && column <= sign + SIGN_REACH;
}

bool optmenu_process_click(int16_t x, int16_t y)
{
    int16_t row = rows_find_row(x, y, ROW_COUNT);
    if (row < 0)
        return false;
    menu.row = row;
    if (row == ROW_CONTINUE) {
        save_changes();
        return true;
    }
    int16_t column = rows_find_column(x);
    if (row == ROW_MUSIC)
        change_value(1);
    else if (is_near_column(column, MINUS_COLUMN))
        change_value(-1);
    else if (is_near_column(column, PLUS_COLUMN))
        change_value(1);
    show_menu();
    return false;
}
