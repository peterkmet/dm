/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui/dialog.h"
#include "data/ids.h"
#include "data/tables.h"
#include "engine/message.h"
#include "ui/command.h"
#include "ui/draw.h"
#include "ui/frame.h"
#include "ui/gameloop.h"
#include "ui/layout.h"
#include "ui/text.h"
#include "ui/ui.h"
#include "ui/video.h"
#include "ui/view.h"
#include <string.h>

#define MAX_CHOICES       4
#define MESSAGE_COUNT     2
#define MESSAGE_LINE_SIZE 64

#define PRESS_FRAMES       5
#define WIDE_MESSAGE_SHIFT 3 /* a message wider than 7/8 of the zone takes two lines */

int16_t g_dialog_choice = DIALOG_NO_CHOICE;

/* The viewport in the game, or centered on the inactive screen. */
static void show_viewport(void)
{
    if (g_game_screen_shown) {
        ui_draw_viewport();
        return;
    }
    Bitmap screen = video_get_screen();
    ui_set_screen_palette(NULL);
    ui_blit_zone(&screen, ui_get_viewport(), ZONE_DIALOG_ON_SCREEN, COLOR_NONE);
}

/* The dialog graphic has the dividers of three choices. For 1, 2, or 4 choices a part of the
   graphic is copied over them. */
static void patch_dividers(Bitmap *viewport, int16_t count)
{
    static const int16_t patch_zone[MAX_CHOICES] = {ZONE_PATCH_ONE_CHOICE, ZONE_PATCH_TWO_CHOICES, 0,
                                                    ZONE_PATCH_FOUR_CHOICES};
    static const int16_t patch_layout[MAX_CHOICES] = {LAYOUT_PATCH_ONE_CHOICE, LAYOUT_PATCH_TWO_CHOICES, 0,
                                                      LAYOUT_PATCH_FOUR_CHOICES};
    static uint8_t copy_px[VIEWPORT_MAX_W * VIEWPORT_MAX_H];
    if (!patch_zone[count - 1])
        return;
    Bitmap copy = {viewport->w, viewport->h, copy_px};
    Box source = tables_get_layout_bitmap(patch_layout[count - 1] - 1);
    int16_t sx = source.x;
    int16_t sy = source.y;
    int16_t w = source.w;
    int16_t h = source.h;
    Box b;
    memcpy(copy_px, viewport->px, (size_t)viewport->w * viewport->h);
    if (layout_get_zone(patch_zone[count - 1], copy.w, copy.h, &b, &w, &h))
        draw_blit(&copy, sx + w, sy + h, viewport, &b, COLOR_NONE, 0);
}

/* The choice texts in the zones for their count. */
static void show_choices(Bitmap *viewport, const char *choice[MAX_CHOICES], int16_t count)
{
    static const int16_t zones[MAX_CHOICES][MAX_CHOICES] = {
        {ZONE_CHOICE_SINGLE},
        {ZONE_CHOICE_TOP, ZONE_CHOICE_SINGLE},
        {ZONE_CHOICE_TOP, ZONE_CHOICE_BOTTOM_LEFT, ZONE_CHOICE_BOTTOM_RIGHT},
        {ZONE_CHOICE_FOUR_FIRST, ZONE_CHOICE_FOUR_FIRST + 1, ZONE_CHOICE_FOUR_FIRST + 2, ZONE_CHOICE_FOUR_FIRST + 3},
    };
    patch_dividers(viewport, count);
    for (int16_t i = 0; i < count; i++)
        ui_print_zone(viewport, zones[count - 1][i], COLOR_GOLD, COLOR_LIGHT_BROWN, choice[i]);
}

/* The two messages centered in the viewport, in the vertical middle of the message zone. */
static void show_messages(Bitmap *viewport, const char *message[MESSAGE_COUNT], int16_t zone)
{
    Box box;
    int16_t width[MESSAGE_COUNT] = {0};
    int lines[MESSAGE_COUNT] = {0};
    int total = 0;
    int16_t height;
    int pad = tables_get_message_line_pad();
    if (!ui_zone_box(zone, &box))
        return;
    for (int i = 0; i < MESSAGE_COUNT; i++) {
        if (!message[i] || !text_get_size(message[i], &width[i], &height))
            continue;
        lines[i] = box.w - (box.w >> WIDE_MESSAGE_SHIFT) < width[i] ? 2 : 1;
        total += (height + pad * 2 - FONT_BASELINE_GAP + 1) * lines[i];
    }
    if (!total)
        return;
    int y = FONT_LINE_HEIGHT + box.y + ((uint16_t)(box.h + pad * 2 - total) >> 1) -
            1; /* unsigned half as in the original */
    for (int i = 0; i < MESSAGE_COUNT; i++) {
        if (!lines[i])
            continue;
        uint8_t color = tables_get_message_color(i);
        int16_t pos = 0;
        if (lines[i] == 2) {
            char line[MESSAGE_LINE_SIZE];
            int16_t max = width[i] - (width[i] >> WIDE_MESSAGE_SHIFT);
            int16_t line_width = message_next_word_line(message[i], line, &pos, max < box.w ? max : box.w);
            text_draw(viewport, (viewport->w - line_width) >> 1, y, color, COLOR_LIGHT_BROWN, line);
            y += pad + MESSAGE_ROW_HEIGHT;
            if (!text_get_size(message[i] + pos, &width[i], &height))
                continue;
        }
        text_draw(viewport, (viewport->w - width[i]) >> 1, y, color, COLOR_LIGHT_BROWN, message[i] + pos);
        y += pad + MESSAGE_ROW_HEIGHT;
    }
}

void dialog_draw(const char *message1, const char *message2, const char *choice1, const char *choice2,
                 const char *choice3, const char *choice4)
{
    Bitmap *viewport = ui_get_viewport();
    const Bitmap *graphic = gfx_get_bitmap(GFX_DIALOG);
    const char *choice[MAX_CHOICES] = {choice1, choice2, choice3, choice4};
    const char *message[MESSAGE_COUNT] = {message1, message2};
    Box all = {0, 0, viewport->w, viewport->h};
    if (graphic)
        draw_blit(graphic, 0, 0, viewport, &all, COLOR_NONE, 0);
    ui_print_zone(viewport, ZONE_VERSION, COLOR_GRAY, COLOR_DARK_GRAY, tables_get_text(TABLES_TEXT_VERSION));
    int16_t count = 1;
    while (count < MAX_CHOICES && choice[count])
        count++;
    if (!g_game_screen_shown)
        memset(g_screen.px, COLOR_BLACK, sizeof g_screen.px);
    if (choice1)
        show_choices(viewport, choice, count);
    show_messages(viewport, message, count == 1 ? ZONE_MESSAGE_ONE_CHOICE : ZONE_MESSAGE);
    show_viewport();
}

/* Fills a rectangle of the viewport with one color. */
static void fill(Bitmap *viewport, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color)
{
    Box b = {x, y, w, h};
    draw_fill(viewport, &b, color);
}

/* The button of the choice: a raised frame, then after PRESS_FRAMES frames a pressed frame. */
static void show_pressed_button(int16_t table, int16_t choice)
{
    Bitmap *viewport = ui_get_viewport();
    MouseInput entry;
    Box button;
    if (!tables_get_mouse_input(table, choice - 1, &entry) || !ui_zone_box(entry.x2, &button))
        return;
    int x = button.x;
    int y = button.y;
    int width = button.w;
    int height = button.h;
    fill(viewport, x - 2, y - 2, width + 5, 1, COLOR_LIGHT_BROWN);
    fill(viewport, x + width + 2, y - 2, 1, height + 4, COLOR_BLACK);
    fill(viewport, x - 4, y + height + 1, width + 7, 2, COLOR_BLACK);
    fill(viewport, x - 3, y - 2, 1, height + 3, COLOR_LIGHT_BROWN);
    fill(viewport, x - 4, y + height + 4, width + 9, 1, COLOR_LIGHT_GRAY);
    fill(viewport, x + width + 5, y - 3, 1, height + 8, COLOR_LIGHT_GRAY);
    show_viewport();
    frame_wait_count(PRESS_FRAMES);
    fill(viewport, x - 1, y - 1, width + 3, 1, COLOR_LIGHT_BROWN);
    fill(viewport, x - 2, y - 2, 1, height + 3, COLOR_LIGHT_BROWN);
    fill(viewport, x + width + 1, y - 2, 1, height + 3, COLOR_DARKEST_GRAY);
    fill(viewport, x - 3, y + height, width + 5, 1, COLOR_DARKEST_GRAY);
    fill(viewport, x - 3, y + height + 3, width + 7, 1, COLOR_DARK_GRAY);
    fill(viewport, x + width + 4, y - 2, 1, height + 6, COLOR_DARK_GRAY);
    show_viewport();
}

int16_t dialog_get_choice(int16_t choice_count)
{
    /* One mouse table per choice count, first for the game screen, then for the inactive screen. */
    int16_t table = TABLES_MOUSE_DIALOG_FIRST + !g_game_screen_shown * MAX_CHOICES + choice_count - 1;
    command_save_input();
    command_set_only_mouse_input(table);
    command_discard_all_input();
    g_dialog_choice = DIALOG_NO_CHOICE;
    while (g_dialog_choice == DIALOG_NO_CHOICE) {
        command_process_queue();
        if (choice_count == 1 && g_last_key == KEY_RETURN)
            g_dialog_choice = 1;
        if (g_dialog_choice == DIALOG_NO_CHOICE && !frame_wait())
            break;
    }
    int16_t choice = g_dialog_choice == DIALOG_NO_CHOICE ? 0 : g_dialog_choice;
    if (choice)
        show_pressed_button(table, choice);
    command_restore_input();
    command_discard_all_input();
    return choice;
}
