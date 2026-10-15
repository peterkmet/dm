#include <string.h>
#include "dialog.h"
#include "command.h"
#include "ftl.h"
#include "ids.h"
#include "layout.h"
#include "message.h"
#include "startend.h"
#include "text.h"
#include "ui.h"
#include "video.h"
#include "view.h"

#define MAX_CHOICES       4
#define MESSAGE_COUNT     2
#define MESSAGE_LINE_SIZE 64

#define PRESS_FRAMES       5
#define WIDE_MESSAGE_SHIFT 3 /* a message wider than 7/8 of the zone takes two lines */

int16_t g_dialog_choice = DIALOG_NO_CHOICE;
bool g_game_screen_inactive = true;

/* dialog_draw_viewport: the viewport in the game, or centered on the inactive screen. */
static void draw_viewport(void)
{
    if (!g_game_screen_inactive) {
        ui_draw_viewport();
        return;
    }
    Bitmap screen = ui_get_screen();
    ui_set_screen_palette(NULL);
    ui_blit_zone(&screen, ui_get_viewport(), ZONE_DIALOG_ON_SCREEN, COLOR_NONE);
}

/* The dialog graphic has the dividers of three choices. For 1, 2, or 4 choices a part of the
   graphic is copied over them. */
static void patch_dividers(Bitmap *vp, int16_t count)
{
    static const int16_t patch_zone[MAX_CHOICES] = {ZONE_PATCH_ONE_CHOICE, ZONE_PATCH_TWO_CHOICES, 0,
                                                    ZONE_PATCH_FOUR_CHOICES};
    static const int16_t patch_layout[MAX_CHOICES] = {LAYOUT_PATCH_ONE_CHOICE, LAYOUT_PATCH_TWO_CHOICES, 0,
                                                      LAYOUT_PATCH_FOUR_CHOICES};
    static uint8_t copy_px[VIEWPORT_MAX_W * VIEWPORT_MAX_H];
    if (!patch_zone[count - 1])
        return;
    Bitmap copy = {vp->w, vp->h, copy_px};
    TablesRect source = tables_layout_bitmap(patch_layout[count - 1] - 1);
    int16_t sx = source.x;
    int16_t sy = source.y;
    int16_t w = source.w;
    int16_t h = source.h;
    Box b;
    memcpy(copy_px, vp->px, (size_t)vp->w * vp->h);
    if (layout_get_zone(patch_zone[count - 1], copy.w, copy.h, &b, &w, &h))
        draw_blit(&copy, sx + w, sy + h, vp, &b, COLOR_NONE, 0);
}

/* The choice texts in the zones for their count. */
static void draw_choices(Bitmap *vp, const char *choice[MAX_CHOICES], int16_t count)
{
    static const int16_t zones[MAX_CHOICES][MAX_CHOICES] = {
        {ZONE_CHOICE_SINGLE},
        {ZONE_CHOICE_TOP, ZONE_CHOICE_SINGLE},
        {ZONE_CHOICE_TOP, ZONE_CHOICE_BOTTOM_LEFT, ZONE_CHOICE_BOTTOM_RIGHT},
        {ZONE_CHOICE_FOUR_FIRST, ZONE_CHOICE_FOUR_FIRST + 1, ZONE_CHOICE_FOUR_FIRST + 2, ZONE_CHOICE_FOUR_FIRST + 3},
    };
    patch_dividers(vp, count);
    for (int16_t i = 0; i < count; i++)
        ui_print_zone(vp, zones[count - 1][i], COLOR_GOLD, COLOR_LIGHT_BROWN, choice[i]);
}

/* The two messages centered in the viewport, in the vertical middle of the message zone. */
static void draw_messages(Bitmap *vp, const char *message[MESSAGE_COUNT], int16_t zone)
{
    Box box;
    int width[MESSAGE_COUNT] = {0};
    int lines[MESSAGE_COUNT] = {0};
    int total = 0;
    int height;
    int pad = tables_message_line_pad();
    if (!ui_zone_box(zone, &box))
        return;
    for (int i = 0; i < MESSAGE_COUNT; i++) {
        if (!message[i] || !text_get_size(message[i], &width[i], &height))
            continue;
        lines[i] = box.w - (box.w >> WIDE_MESSAGE_SHIFT) < width[i] ? 2 : 1;
        total += (height + pad * 2 - TEXT_BASELINE_GAP + 1) * lines[i];
    }
    if (!total)
        return;
    int y = TEXT_LINE_HEIGHT + box.y + ((uint16_t)(box.h + pad * 2 - total) >> 1) -
            1; /* unsigned half as in the original */
    for (int i = 0; i < MESSAGE_COUNT; i++) {
        if (!lines[i])
            continue;
        uint8_t color = tables_message_color(i);
        int pos = 0;
        if (lines[i] == 2) {
            char line[MESSAGE_LINE_SIZE];
            int max = width[i] - (width[i] >> WIDE_MESSAGE_SHIFT);
            int line_width = message_next_word_line(message[i], line, &pos, max < box.w ? max : box.w);
            text_draw(vp, (vp->w - line_width) >> 1, y, color, COLOR_LIGHT_BROWN, line);
            y += pad + MESSAGE_ROW_HEIGHT;
            if (!text_get_size(message[i] + pos, &width[i], &height))
                continue;
        }
        text_draw(vp, (vp->w - width[i]) >> 1, y, color, COLOR_LIGHT_BROWN, message[i] + pos);
        y += pad + MESSAGE_ROW_HEIGHT;
    }
}

void dialog_draw(const char *message1, const char *message2, const char *choice1, const char *choice2,
                 const char *choice3, const char *choice4)
{
    Bitmap *vp = ui_get_viewport();
    const Bitmap *graphic = gfx_get_bitmap(GFX_DIALOG);
    const char *choice[MAX_CHOICES] = {choice1, choice2, choice3, choice4};
    const char *message[MESSAGE_COUNT] = {message1, message2};
    Box all = {0, 0, vp->w, vp->h};
    if (graphic)
        draw_blit(graphic, 0, 0, vp, &all, COLOR_NONE, 0);
    ui_print_zone(vp, ZONE_VERSION, COLOR_GRAY, COLOR_DARK_GRAY, tables_text(TABLES_TEXT_VERSION));
    int16_t count = 1;
    while (count < MAX_CHOICES && choice[count])
        count++;
    if (g_game_screen_inactive)
        memset(fb, COLOR_BLACK, sizeof fb);
    if (choice1)
        draw_choices(vp, choice, count);
    draw_messages(vp, message, count == 1 ? ZONE_MESSAGE_ONE_CHOICE : ZONE_MESSAGE);
    draw_viewport();
}

/* Fills a rectangle of the viewport with one color. */
static void fill(Bitmap *vp, int x, int y, int w, int h, uint8_t color)
{
    Box b = {x, y, w, h};
    draw_fill(vp, &b, color);
}

/* The button of the choice: a raised frame, then after PRESS_FRAMES frames a pressed frame. */
static void draw_pressed_button(int16_t table, int16_t choice)
{
    Bitmap *vp = ui_get_viewport();
    MouseInput entry;
    Box b;
    if (!tables_mouse_input(table, choice - 1, &entry) || !ui_zone_box(entry.x2, &b))
        return; /* the zone word of an entry with a negative x1 */
    int x = b.x;
    int y = b.y;
    int w = b.w;
    int h = b.h;
    fill(vp, x - 2, y - 2, w + 5, 1, COLOR_LIGHT_BROWN);
    fill(vp, x + w + 2, y - 2, 1, h + 4, COLOR_BLACK);
    fill(vp, x - 4, y + h + 1, w + 7, 2, COLOR_BLACK);
    fill(vp, x - 3, y - 2, 1, h + 3, COLOR_LIGHT_BROWN);
    fill(vp, x - 4, y + h + 4, w + 9, 1, COLOR_LIGHT_GRAY);
    fill(vp, x + w + 5, y - 3, 1, h + 8, COLOR_LIGHT_GRAY);
    draw_viewport();
    wait_frames(PRESS_FRAMES);
    fill(vp, x - 1, y - 1, w + 3, 1, COLOR_LIGHT_BROWN);
    fill(vp, x - 2, y - 2, 1, h + 3, COLOR_LIGHT_BROWN);
    fill(vp, x + w + 1, y - 2, 1, h + 3, COLOR_DARKEST_GRAY);
    fill(vp, x - 3, y + h, w + 5, 1, COLOR_DARKEST_GRAY);
    fill(vp, x - 3, y + h + 3, w + 7, 1, COLOR_DARK_GRAY);
    fill(vp, x + w + 4, y - 2, 1, h + 6, COLOR_DARK_GRAY);
    draw_viewport();
}

int16_t dialog_get_choice(int16_t choice_count)
{
    /* One mouse table per choice count, first for the game screen, then for the inactive screen. */
    int16_t table = UI_INPUT_DIALOG_FIRST + g_game_screen_inactive * MAX_CHOICES + choice_count - 1;
    command_save_input();
    command_set_only_mouse_input(table);
    command_discard_all_input();
    g_dialog_choice = DIALOG_NO_CHOICE;
    while (g_dialog_choice == DIALOG_NO_CHOICE) {
        command_process_queue();
        if (choice_count == 1 && g_last_key == KEY_RETURN)
            g_dialog_choice = 1;
        if (g_dialog_choice == DIALOG_NO_CHOICE && !wait_frame())
            break;
    }
    int16_t choice = g_dialog_choice == DIALOG_NO_CHOICE ? 0 : g_dialog_choice;
    if (choice)
        draw_pressed_button(table, choice);
    command_restore_input();
    command_discard_all_input();
    return choice;
}
