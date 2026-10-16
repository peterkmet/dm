/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui/startend.h"
#include "data/adventure.h"
#include "data/csb_image.h"
#include "data/ftl.h"
#include "data/ids.h"
#include "engine/champion.h"
#include "engine/dungeon.h"
#include "engine/event.h"
#include "engine/game.h"
#include "engine/group.h"
#include "engine/inventory.h"
#include "engine/map.h"
#include "engine/message.h"
#include "engine/party.h"
#include "engine/projectile.h"
#include "engine/record.h"
#include "engine/savegame.h"
#include "engine/sound.h"
#include "ui/advanced.h"
#include "ui/anim.h"
#include "ui/command.h"
#include "ui/dialog.h"
#include "ui/draw.h"
#include "ui/gameloop.h"
#include "ui/layout.h"
#include "ui/music.h"
#include "ui/text.h"
#include "ui/ui.h"
#include "ui/video.h"
#include "ui/view.h"
#include <stdio.h>
#include <string.h>

#define QUIT_CONFIRM_TICKS       100 /* QUIT asks for confirmation when no save or load is this recent */
#define DEATH_WAIT_FRAMES        240
#define THE_END_WAIT_FRAMES      300
#define RESTART_WAIT_FRAMES      900
#define FUSE_TEXT_FRAMES         780
#define FUSE_END_FRAMES          600
#define SENSOR_DELAY_FRAMES      60 /* frames per unit of the endgame sensor delay */
#define FUSE_LORD_HEALTH         10000
#define FUSE_FIRST_ATTACK        55
#define FUSE_ATTACK_STEP         40
#define FUSE_TEXT_COUNT          8
#define FUSE_TEXT_SIZE           200
#define DECODE_MESSAGE_INVISIBLE ((int16_t)0x8001) /* dungeon_decode_text: message mode, also invisible texts */
#define ENDGAME_FONT_A           1                 /* the endgame font has A-Z at codes 1-26 */
#define ENDGAME_TEXT_BACKGROUND  COLOR_DARKEST_GRAY
#define ENDGAME_NAME_X           87
#define ENDGAME_SKILL_X          105
#define ENDGAME_ROW_HEIGHT       48
#define ENDGAME_NAME_Y           14
#define ENDGAME_SKILL_LINE       8
#define THE_END_BACKGROUND       0x004 /* dark blue of the THE END palette */
#define THE_END_WHITE            0xfff
#define BUTTON_FRAME_COLOR       0x86f
#define ENDGAME_TEXT_SIZE        64
#define FUSE_FLASH_ROUNDS        3 /* the lord flashes 4 times per round; round n lasts n ticks per flash */
#define FUSE_FLASHES_PER_ROUND   4
#define SAVE_DIR_SIZE            1024
#define CSB_RESTART_TEXT_X       110 /* RESTART THIS GAME text position of the CSB endgame */
#define CSB_RESTART_TEXT_Y       154
#define CSB_ENDING_FILE          "ENDA.DAT" /* the CSB ending animation, in the disk A folder */

/* Choices of the save dialog without the format choice. */
#define SAVE_CHOICE_CANCEL 1
#define SAVE_CHOICE_SAVE   2
#define SAVE_CHOICE_QUIT   3
#define QUIT_CHOICE_CANCEL 2
#define LOAD_CHOICE_CANCEL 2
#define SOUND_FUSE         14

void (*g_frame_hook)(void);
bool g_quit_requested, g_restart_requested, g_return_to_entrance;
int16_t g_waiting_frame;
static char save_dir[SAVE_DIR_SIZE] = ".";

void startend_set_save_dir(const char *dir)
{
    snprintf(save_dir, sizeof save_dir, "%s", dir);
}
const char *startend_save_dir(void)
{
    return save_dir;
}

bool wait_frame(void)
{
    if (!g_frame_hook || g_quit_requested)
        return false;
    g_waiting_frame++;
    g_frame_hook();
    g_waiting_frame--;
    return !g_quit_requested;
}

void wait_frames(int16_t count)
{
    while (count-- > 0 && wait_frame())
        ;
}

/* Waits until a key or a button is pressed. Returns false when there are no frames. */
static bool wait_for_input(void)
{
    command_discard_all_input();
    while (!g_input_received)
        if (!wait_frame())
            return false;
    return true;
}

/* The game asks before QUIT when neither a save nor a load is recent. Returns true to quit. */
static bool confirm_quit(void)
{
    if (g_last_load_game_time + QUIT_CONFIRM_TICKS >= g_game_time ||
        g_last_save_game_time + QUIT_CONFIRM_TICKS >= g_game_time)
        return true;
    dialog_draw(NULL, gfx_get_string(TEXT_NOT_SAVED_OK_TO_QUIT), gfx_get_string(TEXT_QUIT), gfx_get_string(TEXT_CANCEL),
                NULL, NULL);
    return dialog_get_choice(2) != QUIT_CHOICE_CANCEL;
}

int startend_save_current_game(const char **reason)
{
    char mini[SAVE_DIR_SIZE];
    *reason = NULL;
    if (g_adventure != ADVENTURE_CSB || g_dungeon_id != PRISON_DUNGEON_ID)
        return savegame_save(save_dir) ? SAVED_GAME : 0;
    snprintf(mini, sizeof mini, "%s/%s", gameloop_get_data_dir(), CSB_MINI_FILE);
    switch (savegame_make_adventure(save_dir, mini)) {
    case SAVEGAME_ADVENTURE_MADE:
        return SAVED_NEW_ADVENTURE;
    case SAVEGAME_ADVENTURE_NO_CHAMPIONS:
        *reason = "CAN'T MAKE NEW ADVENTURE WITHOUT CHAMPIONS.";
        return 0;
    case SAVEGAME_ADVENTURE_NAMES_NOT_UNIQUE:
        *reason = "EACH CHAMPION MUST HAVE A UNIQUE FIRST NAME!";
        return 0;
    default:
        return 0;
    }
}

/* CSB save_game: SAVE AND PLAY saves and goes on, SAVE AND QUIT saves and ends the game. The dialog
   closes after a save; CSB has no quit without a save. */
static void save_game_csb(void)
{
    command_freeze_game();
    dialog_draw(NULL, NULL, gfx_get_string(TEXT_CANCEL), gfx_get_string(TEXT_SAVE), gfx_get_string(TEXT_QUIT), NULL);
    int16_t choice = dialog_get_choice(3);
    if (choice == SAVE_CHOICE_SAVE || choice == SAVE_CHOICE_QUIT) {
        dialog_draw(NULL, gfx_get_string(TEXT_SAVING_GAME), NULL, NULL, NULL, NULL);
        const char *reason;
        int saved = startend_save_current_game(&reason);
        if (!saved) {
            dialog_draw(reason ? reason : gfx_get_string(TEXT_UNABLE_TO_SAVE), NULL, gfx_get_string(TEXT_OK), NULL,
                        NULL, NULL);
            dialog_get_choice(1);
        } else if (choice == SAVE_CHOICE_QUIT) {
            startend_run_endgame(0);
            return;
        } else if (saved == SAVED_NEW_ADVENTURE) {
            gameloop_start_loaded_game();
        }
        if (saved && g_advanced_features)
            advanced_print_message("GAME SAVED");
    }
    command_unfreeze_game();
}

/* save_game without disks: the dialog stays after a save until CANCEL or QUIT. */
void startend_save_game(void)
{
    const char *message = NULL;
    if (g_adventure == ADVENTURE_CSB) {
        save_game_csb();
        return;
    }
    command_freeze_game();
    for (;;) {
        dialog_draw(message, NULL, gfx_get_string(TEXT_CANCEL), gfx_get_string(TEXT_SAVE), gfx_get_string(TEXT_QUIT),
                    NULL);
        message = NULL;
        int16_t choice = dialog_get_choice(3);
        if (choice == SAVE_CHOICE_SAVE) {
            dialog_draw(NULL, gfx_get_string(TEXT_SAVING_GAME), NULL, NULL, NULL, NULL);
            if (!savegame_save(save_dir)) {
                dialog_draw(gfx_get_string(TEXT_UNABLE_TO_SAVE), NULL, gfx_get_string(TEXT_OK), NULL, NULL, NULL);
                dialog_get_choice(1);
            } else if (g_advanced_features) {
                message = "GAME SAVED";
            }
            continue;
        }
        if (choice == SAVE_CHOICE_QUIT) {
            if (!confirm_quit())
                continue;
            savegame_forget();
            startend_run_endgame(0);
            return;
        }
        break;
    }
    command_unfreeze_game();
}

bool startend_load_saved_game(void)
{
    g_game_screen_inactive = true;
    for (;;) {
        SavegameResult result = savegame_load(save_dir);
        if (result == SAVEGAME_LOADED) {
            dialog_draw(NULL, gfx_get_string(TEXT_LOADING_GAME), NULL, NULL, NULL, NULL);
            return true;
        }
        if (result == SAVEGAME_DAMAGED) {
            /* The original quits here; the port goes back to the entrance. */
            dialog_draw(gfx_get_string(TEXT_GAME_DAMAGED), NULL, gfx_get_string(TEXT_OK), NULL, NULL, NULL);
            dialog_get_choice(1);
            return false;
        }
        dialog_draw(gfx_get_string(TEXT_CANT_FIND_SAVED_GAME), NULL, gfx_get_string(TEXT_LOAD_SAVED_GAME),
                    gfx_get_string(TEXT_CANCEL), NULL, NULL);
        int16_t choice = dialog_get_choice(2);
        if (!choice || choice == LOAD_CHOICE_CANCEL)
            return false;
    }
}

/* Capitals in the endgame font on the dark background. */
static void endgame_print_text(int x, int y, uint8_t color, const char *text)
{
    char s[ENDGAME_TEXT_SIZE];
    int n = 0;
    for (; text[n] && n < (int)sizeof s - 1; n++)
        s[n] = text[n] >= 'A' && text[n] <= 'Z' ? (char)(text[n] - 'A' + ENDGAME_FONT_A) : text[n];
    s[n] = 0;
    text_draw_screen(x, y, color, ENDGAME_TEXT_BACKGROUND, s);
}

/* Writes a box into an entry of the endgame mouse table: x1, x2, y1, y2 as big-endian words. */
static void set_endgame_click_box(int16_t entry_index, const Box *b)
{
    tables_set_mouse_input_box(TABLES_MOUSE_ENDGAME, entry_index, b);
}

/* A framed button with a text; its box becomes the click box of the endgame mouse table entry. */
static void endgame_draw_button(int16_t zone, const char *text, int16_t entry_index)
{
    Bitmap screen = ui_get_screen();
    int tw, th;
    Box button;
    if (!text_get_size(text, &tw, &th))
        return;
    int16_t w = tw;
    int16_t h = th;
    int16_t inner_x = tables_get_button_inner_border(TABLES_AXIS_X);
    int16_t inner_y = tables_get_button_inner_border(TABLES_AXIS_Y);
    if (!layout_get_zone(zone, 0, 0, &button, &w, &h))
        return;
    int16_t dx = inner_x + tables_get_button_border(TABLES_AXIS_X);
    int16_t dy = inner_y + tables_get_button_border(TABLES_AXIS_Y);
    Box outer = {button.x - dx, button.y - dy, button.w + 2 * dx, button.h + 2 * dy};
    Box inner = {outer.x + inner_x, outer.y + inner_y, outer.w - 2 * inner_x, outer.h - 2 * inner_y};
    draw_fill(&screen, &outer, COLOR_DARK_GRAY);
    draw_fill(&screen, &inner, COLOR_BLACK);
    ui_print_zone(&screen, zone, COLOR_CYAN, COLOR_BLACK, text);
    set_endgame_click_box(entry_index, &inner);
}

/* The base skill levels above 1 of a champion, one line each below the name. */
static void print_champion_skills(uint16_t champion_index, int y)
{
    for (int16_t skill = 0; skill < SKILL_BASE_COUNT; skill++) {
        int16_t level = champion_get_skill_level(champion_index, skill, true, true);
        if (level == 1)
            continue;
        char line[ENDGAME_TEXT_SIZE];
        snprintf(line, sizeof line, "%s %s", ui_get_skill_rank_name(level),
                 gfx_get_string(tables_get_base_skill_text(skill)));
        y += ENDGAME_SKILL_LINE;
        endgame_print_text(ENDGAME_SKILL_X, y, COLOR_LIGHT_GRAY, line);
    }
}

/* The won game: each champion in a frame with the portrait, name, title, and base skill levels. */
static void draw_champion_parade(void)
{
    Bitmap screen = ui_get_screen();
    int16_t portrait_width = tables_get_portrait_width();
    int16_t portrait_height = tables_get_portrait_height();
    memset(fb, COLOR_DARKEST_GRAY, sizeof fb);
    for (uint16_t i = 0; i < g_party_champion_count; i++) {
        const Champion *champion = &g_champions[i];
        ui_blit_graphic(&screen, GFX_CHAMPION_FRAME, ZONE_CHAMPION_FRAME_FIRST + i, COLOR_FLESH);
        int16_t w = portrait_width;
        int16_t h = portrait_height;
        Box b;
        Bitmap portrait = ui_draw_champion_portrait(i);
        if (layout_get_zone(ZONE_CHAMPION_PORTRAIT_FIRST + i, portrait_width, portrait_height, &b, &w, &h))
            draw_blit(&portrait, w, h, &screen, &b, COLOR_DARK_GRAY, 0);
        char name[sizeof champion->name + 1] = {0};
        char title[sizeof champion->title + 1] = {0};
        memcpy(name, champion->name, sizeof champion->name);
        memcpy(title, champion->title, sizeof champion->title);
        int name_y = i * ENDGAME_ROW_HEIGHT + ENDGAME_NAME_Y;
        int title_x = ENDGAME_NAME_X + (int)strlen(name) * FONT_CHAR_WIDTH;
        if (title[0] != ',' && title[0] != ';' && title[0] != '-')
            title_x += FONT_CHAR_WIDTH; /* a space unless punctuation follows */
        endgame_print_text(ENDGAME_NAME_X, name_y, COLOR_GOLD, name);
        endgame_print_text(title_x, name_y, COLOR_GOLD, title);
        print_champion_skills(i, name_y + 1);
    }
    ui_set_screen_palette(NULL);
}

/* The outer or the inner box of the CSB endgame button. */
static Box get_csb_box(bool inner)
{
    Box r = tables_get_csb_restart_box(inner);
    return (Box){r.x, r.y, r.w, r.h};
}

/* CSB: RESTART THIS GAME in its fixed boxes. The click box is in the endgame mouse table. */
static void csb_draw_restart_button(void)
{
    Bitmap screen = ui_get_screen();
    Box outer = get_csb_box(false);
    Box inner = get_csb_box(true);
    draw_fill(&screen, &outer, COLOR_DARK_GRAY);
    draw_fill(&screen, &inner, COLOR_BLACK);
    text_draw_screen(CSB_RESTART_TEXT_X, CSB_RESTART_TEXT_Y, COLOR_CYAN, COLOR_BLACK,
                     gfx_get_string(TEXT_RESTART_THIS_GAME));
}

/* THE END on a dark blue screen, with RESTART THIS GAME and QUIT when a saved game exists. CSB has no QUIT. */
static void draw_the_end(bool with_buttons)
{
    Bitmap screen = ui_get_screen();
    uint16_t colors[PALETTE_COLORS];
    for (int i = 0; i < PALETTE_COLORS; i++)
        colors[i] = THE_END_BACKGROUND;
    colors[COLOR_WHITE] = THE_END_WHITE;
    memset(fb, COLOR_BLACK, sizeof fb);
    ui_blit_graphic(&screen, GFX_THE_END, ZONE_DIALOG_ON_SCREEN, COLOR_NONE);
    if (with_buttons) {
        if (g_adventure == ADVENTURE_CSB) {
            csb_draw_restart_button();
        } else {
            endgame_draw_button(ZONE_RESTART_BUTTON, gfx_get_string(TEXT_RESTART_THIS_GAME), 0);
            endgame_draw_button(ZONE_QUIT_BUTTON, gfx_get_string(TEXT_QUIT), 1);
        }
        colors[COLOR_DARK_GRAY] = BUTTON_FRAME_COLOR;
        colors[COLOR_CYAN] = THE_END_WHITE;
    }
    ui_set_screen_palette(colors);
}

/* Waits up to 900 frames for RESTART THIS GAME or QUIT. Returns true when the saved game is loaded. */
static bool wait_for_restart(void)
{
    command_set_only_mouse_input(TABLES_MOUSE_ENDGAME);
    command_discard_all_input();
    g_restart_requested = false;
    for (int16_t frames = RESTART_WAIT_FRAMES; --frames && !g_restart_requested;) {
        command_process_queue();
        if (g_quit_requested) { /* the QUIT button: the credits, then the entrance */
            g_quit_requested = false;
            break;
        }
        if (!g_restart_requested && !wait_frame())
            break;
    }
    command_set_only_mouse_input(-1);
    if (!g_restart_requested || g_quit_requested)
        return false;
    memset(fb, COLOR_BLACK, sizeof fb);
    int16_t loaded = startend_load_saved_game();
    g_restart_requested = false;
    if (!loaded)
        return false;
    gameloop_start_loaded_game();
    return true;
}

/* The credits screen until a key or a button is pressed. */
static void show_credits(void)
{
    Bitmap screen = ui_get_screen();
    ui_blit_graphic(&screen, GFX_CREDITS, ZONE_SCREEN, COLOR_NONE);
    uint16_t credits_colors[PALETTE_COLORS];
    tables_get_palette(TABLES_PALETTE_CREDITS, credits_colors);
    ui_set_screen_palette(credits_colors);
    wait_for_input();
}

/* The port shows the entrance after the credits instead of quitting (deliberate deviation). */
void startend_run_endgame(int16_t party_dead)
{
    command_set_only_mouse_input(-1);
    if (party_dead)
        music_play_end(g_game_won);
    if (party_dead && !g_game_won) {
        sound_request_play(SOUND_SCREAM, g_party_map_x, g_party_map_y, SOUND_MODE_NOW);
        wait_frames(DEATH_WAIT_FRAMES);
    }
    g_game_screen_inactive = true;
    if (party_dead && g_game_won) {
        draw_champion_parade();
        wait_for_input();
    }
    if (party_dead) {
        draw_the_end(false);
        wait_frames(THE_END_WAIT_FRAMES);
        if (g_saved_game_exists) {
            draw_the_end(true);
            if (wait_for_restart())
                return;
        }
    }
    show_credits();
    g_return_to_entrance = true;
}

/* One game tick with the view and the sounds. */
static void fuse_update(void)
{
    event_process_expired();
    gameloop_draw_view();
    sound_play_pending();
    game_increment_time();
    wait_frame();
}

/* The fluxcage explosions on a square disappear. */
static void remove_fluxcages(int16_t x, int16_t y)
{
    uint16_t t = map_get_square_first_object(x, y);
    while (t != THING_ENDOFLIST) {
        if (THING_TYPE(t) == THING_EXPLOSION && rec_explosion_type(t) == EXPLOSION_FLUXCAGE) {
            map_unlink_thing_from_list(t, 0, x, y);
            rec_thing_set_free(t);
            t = map_get_square_first_object(x, y);
            continue;
        }
        t = rec_thing_next(t);
    }
}

/* Six explosions of rising power on the square of Lord Chaos. */
static void draw_fuse_explosions(uint16_t type, int16_t x, int16_t y)
{
    for (uint16_t attack = FUSE_FIRST_ATTACK; attack <= FUSE_ACTION_ATTACK; attack += FUSE_ATTACK_STEP) {
        explosion_create(EXPLOSION_THING(type), attack, x, y, CELL_CENTERED);
        fuse_update();
    }
}

/* The texts on square (0, 0) of the map, one after the other in the order of their first letter
   (the letter itself is not shown). */
static void show_final_texts(void)
{
    uint16_t texts[FUSE_TEXT_COUNT];
    int16_t count = 0;
    for (uint16_t t = map_get_square_first_thing(0, 0); t != THING_ENDOFLIST && count < FUSE_TEXT_COUNT;
         t = rec_thing_next(t))
        if (THING_TYPE(t) == THING_TEXT)
            texts[count++] = t;
    char letter = 'A';
    for (int16_t shown = 0; shown < count; shown++) {
        for (int16_t i = 0; i < count; i++) {
            char text[FUSE_TEXT_SIZE];
            dungeon_decode_text(text, sizeof text, texts[i], DECODE_MESSAGE_INVISIBLE);
            if (text[1] != letter)
                continue;
            message_clear();
            text[1] = '\n';
            message_print(COLOR_WHITE, text + 1);
            fuse_update();
            wait_frames(FUSE_TEXT_FRAMES);
            letter++;
            break;
        }
    }
}

/* Lord Chaos and Lord Order flash in turn, slower each round, with the fuse sound. */
static void flash_lords(uint16_t lord, int16_t x, int16_t y)
{
    for (int16_t ticks = FUSE_FLASH_ROUNDS; ticks > 0; ticks--)
        for (int16_t flash = FUSE_FLASHES_PER_ROUND; flash > 0; flash--) {
            sound_request_play(SOUND_FUSE, x, y, SOUND_MODE_AFTER_TICK);
            rec_group_set_creature_type(lord, flash & 1 ? CREATURE_LORD_ORDER : CREATURE_LORD_CHAOS);
            for (int16_t i = 0; i < ticks; i++)
                fuse_update();
        }
}

/* All creature groups of the map except the one on (x, y) disappear. */
static void delete_other_groups(int16_t x, int16_t y)
{
    for (int16_t gx = 0; gx < g_current_map_width; gx++)
        for (int16_t gy = 0; gy < g_current_map_height; gy++)
            if (map_find_thing((Location){gx, gy}, THING_GROUP) != THING_ENDOFLIST && (gx != x || gy != y))
                group_delete(gx, gy);
}

void startend_run_fuse_sequence(void)
{
    int16_t x = g_party_map_x;
    int16_t y = g_party_map_y;
    if (g_inventory_champion_ordinal)
        command_toggle_inventory(CHAMPION_MAX_COUNT);
    command_highlight_disable();
    game_start_fuse_sequence();
    ui_draw_all_champion_states(CHAMPION_ATTR_STATUS_BOX);
    fuse_update();
    map_step(g_party_direction, 1, 0, &x, &y);
    uint16_t lord = map_find_thing((Location){x, y}, THING_GROUP);
    if (lord == THING_ENDOFLIST)
        return;
    rec_group_set_health(lord, 0, FUSE_LORD_HEALTH);
    group_set_cells(lord, CELL_CENTERED, g_party_map_index);
    group_set_directions(lord, (uint8_t)direction_back(g_party_direction), g_party_map_index); /* faces the party */
    remove_fluxcages(g_party_map_x, g_party_map_y);
    remove_fluxcages(x, y);
    fuse_update();
    draw_fuse_explosions(EXPLOSION_FIREBALL, x, y);
    sound_request_play(SOUND_FUSE, x, y, SOUND_MODE_AFTER_TICK);
    rec_group_set_creature_type(lord, CREATURE_LORD_ORDER);
    fuse_update();
    draw_fuse_explosions(EXPLOSION_HARM_NON_MATERIAL, x, y);
    flash_lords(lord, x, y);
    explosion_create(EXPLOSION_THING(EXPLOSION_FIREBALL), FUSE_ACTION_ATTACK, x, y, CELL_CENTERED);
    explosion_create(EXPLOSION_THING(EXPLOSION_HARM_NON_MATERIAL), FUSE_ACTION_ATTACK, x, y, CELL_CENTERED);
    fuse_update();
    rec_group_set_creature_type(lord, CREATURE_GREY_LORD);
    fuse_update();
    g_do_not_draw_fluxcages = true;
    fuse_update();
    delete_other_groups(x, y);
    fuse_update();
    show_final_texts();
    draw_fuse_explosions(EXPLOSION_HARM_NON_MATERIAL, x, y);
    wait_frames(FUSE_END_FRAMES);
    savegame_forget();
    startend_run_endgame(1);
}

/* CSB won game: the ending animation, then the CSB screen after a key or a click (the original stays on the last frame). */
static void play_csb_ending(void)
{
    char path[SAVE_DIR_SIZE];
    g_game_screen_inactive = true;
    snprintf(path, sizeof path, "%s/%s", gameloop_get_data_dir(), CSB_ENDING_FILE);
    if (!anim_play(path))
        wait_for_input();
    g_return_to_entrance = true;
}

void startend_run_endgame_sensor(uint16_t delay)
{
    if (g_adventure == ADVENTURE_CSB) { /* no delay */
        play_csb_ending();
        return;
    }
    wait_frames(delay * SENSOR_DELAY_FRAMES);
    startend_run_endgame(1);
}
