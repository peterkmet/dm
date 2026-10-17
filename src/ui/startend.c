/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "startend.h"
#include "adventure.h"
#include "ids.h"
#include "champion.h"
#include "dungeon.h"
#include "event.h"
#include "game.h"
#include "group.h"
#include "map.h"
#include "message.h"
#include "party.h"
#include "projectile.h"
#include "record.h"
#include "savegame.h"
#include "sound.h"
#include "advanced.h"
#include "anim.h"
#include "command.h"
#include "dialog.h"
#include "draw.h"
#include "frame.h"
#include "gameloop.h"
#include "layout.h"
#include "music.h"
#include "panel.h"
#include "slotlist.h"
#include "status.h"
#include "text.h"
#include "ui.h"
#include "video.h"
#include "view.h"
#include <stdio.h>
#include <string.h>

/* Endgame and fuse sequence: timing, layout and text sizes. */
#define QUIT_CONFIRM_TICKS      100 /* QUIT asks for confirmation when no save or load is this recent */
#define DEATH_WAIT_FRAMES       240
#define THE_END_WAIT_FRAMES     300
#define RESTART_WAIT_FRAMES     900
#define FUSE_TEXT_FRAMES        780
#define FUSE_END_FRAMES         600
#define SENSOR_DELAY_FRAMES     60 /* frames per unit of the endgame sensor delay */
#define FUSE_FIRST_ATTACK       55
#define FUSE_ATTACK_STEP        40
#define FUSE_TEXT_COUNT         8
#define FUSE_TEXT_SIZE          200
#define ENDGAME_FONT_A          1 /* the endgame font has A-Z at codes 1-26 */
#define ENDGAME_TEXT_BACKGROUND COLOR_DARKEST_GRAY
#define ENDGAME_NAME_X          87
#define ENDGAME_SKILL_X         105
#define ENDGAME_ROW_HEIGHT      48
#define ENDGAME_NAME_Y          14
#define ENDGAME_SKILL_LINE      8
#define ENDGAME_TEXT_SIZE       64
#define FUSE_FLASH_ROUNDS       3 /* the lord flashes 4 times per round; round n lasts n ticks per flash */
#define FUSE_FLASHES_PER_ROUND  4
#define SAVE_DIR_SIZE           1024
#define RESTART_TEXT_X          110 /* RESTART THIS GAME text position of the CSB endgame */
#define RESTART_TEXT_Y          154
#define ENDING_ANIMATION_FILE   "ENDA.DAT" /* the CSB ending animation, in the disk A folder */

/* Amiga 12-bit colors of the THE END screen. */
#define THE_END_BACKGROUND 0x004 /* dark blue of the THE END palette */
#define THE_END_WHITE      0xfff
#define BUTTON_FRAME_COLOR 0x86f

/* Choices of the save dialog without the format choice. */
#define SAVE_CHOICE_CANCEL 1
#define SAVE_CHOICE_SAVE   2
#define SAVE_CHOICE_QUIT   3
#define QUIT_CHOICE_CANCEL 2
#define LOAD_CHOICE_CANCEL 2

/* Choices of the save dialog of the advanced mode. */
#define ADVANCED_CHOICE_LOAD     1
#define ADVANCED_CHOICE_SAVE     2
#define ADVANCED_CHOICE_CONTINUE 3
#define ADVANCED_CHOICE_QUIT     4
#define ADVANCED_CHOICE_COUNT    4

bool g_restart_requested, g_return_to_entrance;
static char save_dir[SAVE_DIR_SIZE] = ".";
static int16_t current_slot; /* the slot of the last save or load from a slot in this run; RESTART loads it */

void startend_set_save_dir(const char *directory)
{
    snprintf(save_dir, sizeof save_dir, "%s", directory);
}
const char *startend_save_dir(void)
{
    return save_dir;
}

/* Waits until a key or a button is pressed. Returns false when there are no frames. */
static bool wait_for_input(void)
{
    command_discard_all_input();
    while (!g_input_received)
        if (!frame_wait())
            return false;
    return true;
}

/* The game asks before QUIT when neither a save nor a load is recent. Returns true to quit. */
static bool confirm_quit(void)
{
    if (g_session.last_load_time + QUIT_CONFIRM_TICKS >= g_game_time ||
        g_session.last_save_time + QUIT_CONFIRM_TICKS >= g_game_time)
        return true;
    dialog_draw(&(DialogText){
        {NULL,                      gfx_get_string(TEXT_NOT_SAVED_OK_TO_QUIT)},
        {gfx_get_string(TEXT_QUIT), gfx_get_string(TEXT_CANCEL)              }
    });
    return dialog_get_choice(2) != QUIT_CHOICE_CANCEL;
}

/* The save slots, the slot list and the current slot exist only in the advanced mode. */
static bool has_slots(void)
{
    return g_session.mode == GAME_MODE_ADVANCED;
}

/* The path of MINI.DAT in the CSB data folder. */
static void make_mini_path(char *out, size_t size)
{
    snprintf(out, size, "%s/%s", gameloop_get_data_dir(), CSB_MINI_FILE);
}

/* The reason why the prison party cannot make the adventure; NULL when the party is not the reason. */
static const char *get_refusal_reason(SavegameAdventureResult result)
{
    switch (result) {
    case SAVEGAME_ADVENTURE_NO_CHAMPIONS:
        return "CAN'T MAKE NEW ADVENTURE WITHOUT CHAMPIONS.";
    case SAVEGAME_ADVENTURE_NAMES_NOT_UNIQUE:
        return "EACH CHAMPION MUST HAVE A UNIQUE FIRST NAME!";
    default:
        return NULL;
    }
}

int startend_save_current_game(int16_t slot, const char **reason)
{
    char mini[SAVE_DIR_SIZE];
    *reason = NULL;
    if (g_adventure != ADVENTURE_CSB || g_dungeon_id != PRISON_DUNGEON_ID) {
        if (!savegame_save_slot(save_dir, slot))
            return 0;
        current_slot = slot;
        return SAVED_GAME;
    }
    make_mini_path(mini, sizeof mini);
    SavegameAdventureResult result = savegame_make_adventure(save_dir, mini, slot);
    if (result != SAVEGAME_ADVENTURE_MADE) {
        *reason = get_refusal_reason(result);
        return 0;
    }
    current_slot = slot;
    return SAVED_NEW_ADVENTURE;
}

/* The OK dialog that a save or a start cannot be made: the reason, else UNABLE TO SAVE. */
static void show_unable_dialog(const char *reason)
{
    dialog_draw(&(DialogText){
        {reason ? reason : gfx_get_string(TEXT_UNABLE_TO_SAVE), NULL},
        {gfx_get_string(TEXT_OK)}
    });
    dialog_get_choice(1);
}

bool startend_take_prison_stairs(void)
{
    char mini[SAVE_DIR_SIZE];
    make_mini_path(mini, sizeof mini);
    SavegameAdventureResult result = savegame_start_adventure(mini);
    if (result != SAVEGAME_ADVENTURE_MADE) {
        show_unable_dialog(get_refusal_reason(result));
        return false;
    }
    gameloop_continue_loaded_game();
    return true;
}

/* Shows the saving dialog and saves the current game to the slot; an unable-to-save dialog names the reason. Returns
   the SAVED_* result, 0 on failure. */
static int save_with_dialog(int16_t slot)
{
    const char *reason;
    dialog_draw(&(DialogText){
        {NULL, gfx_get_string(TEXT_SAVING_GAME)},
        {NULL}
    });
    int saved = startend_save_current_game(slot, &reason);
    if (!saved)
        show_unable_dialog(reason);
    return saved;
}

/* CSB save dialog: SAVE AND PLAY saves and goes on, SAVE AND QUIT saves and ends the game. The dialog
   closes after a save; CSB has no quit without a save. */
static void save_game_csb(void)
{
    command_freeze_game();
    dialog_draw(&(DialogText){
        {NULL, NULL},
        {gfx_get_string(TEXT_CANCEL), gfx_get_string(TEXT_SAVE), gfx_get_string(TEXT_QUIT)}
    });
    int16_t choice = dialog_get_choice(3);
    if (choice == SAVE_CHOICE_SAVE || choice == SAVE_CHOICE_QUIT) {
        int saved = save_with_dialog(0);
        if (saved && choice == SAVE_CHOICE_QUIT) {
            startend_run_endgame(false);
            return;
        }
        if (saved == SAVED_NEW_ADVENTURE)
            gameloop_start_loaded_game();
        if (saved && g_advanced_features)
            advanced_print_message("GAME SAVED");
    }
    command_unfreeze_game();
}

/* Loads a slot, or the quick save for SLOTLIST_QUICK_ROW, and makes a loaded slot the current slot. */
static SavegameResult load_slot(int16_t slot)
{
    if (slot == SLOTLIST_QUICK_ROW)
        savegame_forget(); /* a quick save is not a saved game: no RESTART until the first save */
    SavegameResult result =
        slot == SLOTLIST_QUICK_ROW ? savegame_quick_load(save_dir) : savegame_load_slot(save_dir, slot);
    if (result == SAVEGAME_LOADED && slot != SLOTLIST_QUICK_ROW)
        current_slot = slot;
    return result;
}

/* Saves to a slot of the save list. DM stays in the dialog with GAME SAVED; CSB goes back to the game, and in the
   prison starts the new adventure. Returns true when the dialog closes. */
static bool save_from_list(const char **message)
{
    int16_t slot = slotlist_choose(save_dir, false);
    int saved = slot >= 0 ? save_with_dialog(slot) : 0;
    if (!saved)
        return false;
    if (g_adventure != ADVENTURE_CSB) {
        if (g_advanced_features)
            *message = "GAME SAVED";
        return false;
    }
    if (saved == SAVED_NEW_ADVENTURE)
        gameloop_start_loaded_game();
    if (g_advanced_features)
        advanced_print_message("GAME SAVED");
    return true;
}

/* The save dialog of the advanced mode in DM and CSB: LOAD and SAVE open the slot lists, CONTINUE goes back to the
   game, QUIT goes to the entrance or the CSB screen after the quit question. A loaded slot starts at once. */
static void save_game_advanced(void)
{
    const char *message = NULL;
    command_freeze_game();
    for (;;) {
        dialog_draw(&(DialogText){
            {message, NULL},
            {"LOAD", "SAVE", "CONTINUE", "QUIT"}
        });
        message = NULL;
        int16_t choice = dialog_get_choice(ADVANCED_CHOICE_COUNT);
        if (choice == ADVANCED_CHOICE_LOAD) {
            int16_t slot = slotlist_choose(save_dir, true);
            SavegameResult result = slot >= 0 ? load_slot(slot) : SAVEGAME_MISSING;
            if (result == SAVEGAME_LOADED) {
                gameloop_start_loaded_game();
                break;
            }
            if (slot >= 0) /* the file changed after the list read it; the running game stays */
                message = gfx_get_string(result == SAVEGAME_DAMAGED ? TEXT_GAME_DAMAGED : TEXT_CANT_FIND_SAVED_GAME);
            continue;
        }
        if (choice == ADVANCED_CHOICE_SAVE) {
            if (save_from_list(&message))
                break;
            continue;
        }
        if (choice == ADVANCED_CHOICE_QUIT) {
            if (!confirm_quit())
                continue;
            savegame_forget();
            startend_run_endgame(false);
            return;
        }
        break;
    }
    command_unfreeze_game();
}

void startend_save_game(void)
{
    const char *message = NULL;
    if (has_slots()) {
        save_game_advanced();
        return;
    }
    if (g_adventure == ADVENTURE_CSB) {
        save_game_csb();
        return;
    }
    command_freeze_game();
    for (;;) {
        dialog_draw(&(DialogText){
            {message, NULL},
            {gfx_get_string(TEXT_CANCEL), gfx_get_string(TEXT_SAVE), gfx_get_string(TEXT_QUIT)}
        });
        message = NULL;
        int16_t choice = dialog_get_choice(3);
        if (choice == SAVE_CHOICE_SAVE) {
            if (save_with_dialog(0) && g_advanced_features)
                message = "GAME SAVED";
            continue;
        }
        if (choice == SAVE_CHOICE_QUIT) {
            if (!confirm_quit())
                continue;
            savegame_forget();
            startend_run_endgame(false);
            return;
        }
        break;
    }
    command_unfreeze_game();
}

/* Loads a slot, or the quick save for SLOTLIST_QUICK_ROW; with choose, the load list chooses it first. The dialogs
   of RESUME: LOADING GAME, THE GAME IS DAMAGED!, and CAN'T FIND SAVED GAME, whose LOAD SAVED GAME tries again (in
   the advanced mode it opens the load list). Returns true when the game is loaded. */
static bool load_with_dialogs(int16_t slot, bool choose)
{
    g_game_screen_shown = false;
    for (;;) {
        if (choose)
            slot = slotlist_choose(save_dir, true);
        if (slot < 0)
            return false;
        SavegameResult result = load_slot(slot);
        if (result == SAVEGAME_LOADED) {
            dialog_draw(&(DialogText){
                {NULL, gfx_get_string(TEXT_LOADING_GAME)},
                {NULL}
            });
            return true;
        }
        if (result == SAVEGAME_DAMAGED) {
            /* The original quits here; the port goes back to the entrance. */
            dialog_draw(&(DialogText){
                {gfx_get_string(TEXT_GAME_DAMAGED), NULL},
                {gfx_get_string(TEXT_OK)}
            });
            dialog_get_choice(1);
            return false;
        }
        dialog_draw(&(DialogText){
            {gfx_get_string(TEXT_CANT_FIND_SAVED_GAME), NULL                       },
            {gfx_get_string(TEXT_LOAD_SAVED_GAME),      gfx_get_string(TEXT_CANCEL)}
        });
        int16_t choice = dialog_get_choice(2);
        if (!choice || choice == LOAD_CHOICE_CANCEL)
            return false;
        choose = has_slots();
    }
}

bool startend_load_saved_game(void)
{
    return load_with_dialogs(0, has_slots());
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
    Bitmap screen = video_get_screen();
    int16_t w, h;
    ZoneClip placed;
    if (!text_get_size(text, &w, &h))
        return;
    int16_t inner_x = tables_get_button_inner_border(TABLES_AXIS_X);
    int16_t inner_y = tables_get_button_inner_border(TABLES_AXIS_Y);
    if (!layout_get_zone(zone, &(ZoneItem){0, 0, w, h}, &placed))
        return;
    Box button = placed.box;
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
        snprintf(line, sizeof line, "%s %s", panel_get_rank_name(level),
                 gfx_get_string(tables_get_base_skill_text(skill)));
        y += ENDGAME_SKILL_LINE;
        endgame_print_text(ENDGAME_SKILL_X, y, COLOR_LIGHT_GRAY, line);
    }
}

/* The won game: each champion in a frame with the portrait, name, title, and base skill levels. */
static void show_champion_parade(void)
{
    Bitmap screen = video_get_screen();
    int16_t portrait_width = tables_get_portrait_width();
    int16_t portrait_height = tables_get_portrait_height();
    memset(g_screen.px, COLOR_DARKEST_GRAY, sizeof g_screen.px);
    for (uint16_t i = 0; i < g_party_champion_count; i++) {
        const Champion *champion = &g_champions[i];
        ui_blit_graphic(&screen, GFX_CHAMPION_FRAME, ZONE_CHAMPION_FRAME_FIRST + i, COLOR_FLESH);
        ZoneClip placed;
        Bitmap portrait = ui_get_champion_portrait(i);
        if (layout_get_zone(ZONE_CHAMPION_PORTRAIT_FIRST + i,
                            &(ZoneItem){portrait_width, portrait_height, portrait_width, portrait_height}, &placed))
            draw_blit(&(BlitSource){&portrait, placed.source_x, placed.source_y, COLOR_DARK_GRAY, 0}, &screen,
                      &placed.box);
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
    Bitmap screen = video_get_screen();
    Box outer = get_csb_box(false);
    Box inner = get_csb_box(true);
    draw_fill(&screen, &outer, COLOR_DARK_GRAY);
    draw_fill(&screen, &inner, COLOR_BLACK);
    text_draw_screen(RESTART_TEXT_X, RESTART_TEXT_Y, COLOR_CYAN, COLOR_BLACK, gfx_get_string(TEXT_RESTART_THIS_GAME));
}

/* THE END on a dark blue screen, with RESTART THIS GAME and QUIT when a saved game exists. CSB has no QUIT. */
static void show_the_end(bool with_buttons)
{
    Bitmap screen = video_get_screen();
    uint16_t colors[PALETTE_COLORS];
    for (int i = 0; i < PALETTE_COLORS; i++)
        colors[i] = THE_END_BACKGROUND;
    colors[COLOR_WHITE] = THE_END_WHITE;
    memset(g_screen.px, COLOR_BLACK, sizeof g_screen.px);
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
        if (!g_restart_requested && !frame_wait())
            break;
    }
    command_set_only_mouse_input(-1);
    if (!g_restart_requested || g_quit_requested)
        return false;
    memset(g_screen.px, COLOR_BLACK, sizeof g_screen.px);
    bool loaded = load_with_dialogs(has_slots() ? current_slot : 0, false);
    g_restart_requested = false;
    if (!loaded)
        return false;
    gameloop_start_loaded_game();
    return true;
}

void startend_draw_credits(void)
{
    Bitmap screen = video_get_screen();
    ui_blit_graphic(&screen, GFX_CREDITS, ZONE_SCREEN, COLOR_NONE);
}

/* The credits screen until a key or a button is pressed. */
static void show_credits(void)
{
    startend_draw_credits();
    uint16_t credits_colors[PALETTE_COLORS];
    tables_get_palette(TABLES_PALETTE_CREDITS, credits_colors);
    ui_set_screen_palette(credits_colors);
    wait_for_input();
}

void startend_run_endgame(bool party_dead)
{
    command_set_only_mouse_input(-1);
    if (party_dead)
        music_play_end(g_session.won);
    if (party_dead && !g_session.won) {
        sound_request_play(SOUND_SCREAM, (Location){g_party_map_x, g_party_map_y}, SOUND_MODE_NOW, 0);
        frame_wait_count(DEATH_WAIT_FRAMES);
    }
    g_game_screen_shown = false;
    if (party_dead && g_session.won) {
        show_champion_parade();
        wait_for_input();
    }
    if (party_dead) {
        show_the_end(false);
        frame_wait_count(THE_END_WAIT_FRAMES);
        if (g_session.saved_game_exists) {
            show_the_end(true);
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
    frame_wait();
}

/* Six explosions of rising power on the square of Lord Chaos. */
static void show_fuse_explosions(uint16_t type, int16_t x, int16_t y)
{
    for (uint16_t attack = FUSE_FIRST_ATTACK; attack <= FUSE_ACTION_ATTACK; attack += FUSE_ATTACK_STEP) {
        explosion_create(EXPLOSION_THING(type), attack, (Location){x, y}, (Location){x, y}, CELL_CENTERED);
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
            dungeon_decode_text(text, sizeof text, texts[i], TEXT_MODE_MESSAGE | TEXT_MODE_ANY_VISIBILITY);
            if (text[1] != letter)
                continue;
            message_clear();
            text[1] = '\n';
            message_print(COLOR_WHITE, text + 1);
            fuse_update();
            frame_wait_count(FUSE_TEXT_FRAMES);
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
            sound_request_play(SOUND_BUZZ, (Location){x, y}, SOUND_MODE_AFTER_TICK, 0);
            rec_group_set_creature_type(lord, flash & 1 ? CREATURE_LORD_ORDER : CREATURE_LORD_CHAOS);
            for (int16_t i = 0; i < ticks; i++)
                fuse_update();
        }
}

void startend_run_fuse_sequence(void)
{
    int16_t x = g_party_map_x;
    int16_t y = g_party_map_y;
    if (g_session.inventory_champion_index >= 0)
        command_close_inventory();
    command_highlight_disable();
    game_start_fuse_sequence();
    status_draw_all(CHAMPION_ATTR_STATUS_BOX);
    fuse_update();
    map_step(g_party_direction, 1, 0, &x, &y);
    uint16_t lord = map_find_thing((Location){x, y}, THING_GROUP);
    if (lord == THING_ENDOFLIST)
        return;
    game_prepare_fuse_lord(lord, (Location){x, y});
    fuse_update();
    show_fuse_explosions(EXPLOSION_FIREBALL, x, y);
    sound_request_play(SOUND_BUZZ, (Location){x, y}, SOUND_MODE_AFTER_TICK, 0);
    rec_group_set_creature_type(lord, CREATURE_LORD_ORDER);
    fuse_update();
    show_fuse_explosions(EXPLOSION_HARM_NON_MATERIAL, x, y);
    flash_lords(lord, x, y);
    Location at = {x, y};
    explosion_create(EXPLOSION_THING(EXPLOSION_FIREBALL), FUSE_ACTION_ATTACK, at, at, CELL_CENTERED);
    explosion_create(EXPLOSION_THING(EXPLOSION_HARM_NON_MATERIAL), FUSE_ACTION_ATTACK, at, at, CELL_CENTERED);
    fuse_update();
    rec_group_set_creature_type(lord, CREATURE_GREY_LORD);
    fuse_update();
    g_do_not_draw_fluxcages = true;
    fuse_update();
    group_delete_all_except((Location){x, y});
    fuse_update();
    show_final_texts();
    show_fuse_explosions(EXPLOSION_HARM_NON_MATERIAL, x, y);
    frame_wait_count(FUSE_END_FRAMES);
    savegame_forget();
    startend_run_endgame(true);
}

/* CSB won game: the ending animation, then the CSB screen after a key or a click (the original stays on the last frame). */
static void play_csb_ending(void)
{
    char path[SAVE_DIR_SIZE];
    g_game_screen_shown = false;
    snprintf(path, sizeof path, "%s/%s", gameloop_get_data_dir(), ENDING_ANIMATION_FILE);
    if (anim_play(path))
        wait_for_input();
    g_return_to_entrance = true;
}

void startend_run_endgame_sensor(uint16_t delay)
{
    if (g_adventure == ADVENTURE_CSB) { /* no delay */
        play_csb_ending();
        return;
    }
    frame_wait_count(delay * SENSOR_DELAY_FRAMES);
    startend_run_endgame(true);
}
