#include <stdio.h>
#include <string.h>
#include "startend.h"
#include "adventure.h"
#include "advanced.h"
#include "anim.h"
#include "champion.h"
#include "command.h"
#include "csbimage.h"
#include "dialog.h"
#include "dungeon.h"
#include "ftl.h"
#include "game.h"
#include "gameloop.h"
#include "group.h"
#include "ids.h"
#include "inventory.h"
#include "layout.h"
#include "map.h"
#include "message.h"
#include "music.h"
#include "projectile.h"
#include "savegame.h"
#include "sound.h"
#include "text.h"
#include "timeline_process.h"
#include "ui.h"
#include "video.h"
#include "view.h"

/* KAOS data addresses. */
#define CREDITS_PALETTE 0x100a46       /* g_credits_palette[16] */
#define BUTTON_BORDER 0x102988         /* outer border x, y of an endgame button */
#define BUTTON_INNER_BORDER 0x10298c   /* inner border x, y */

#define GFX_CREDITS 5
#define GFX_THE_END 6
#define GFX_CHAMPION_FRAME 346
#define ZONE_THE_END 5
#define ZONE_RESTART_BUTTON 437
#define ZONE_QUIT_BUTTON 438
#define ZONE_CHAMPION_FRAME_FIRST 412  /* + champion */
#define ZONE_CHAMPION_PORTRAIT_FIRST 416

#define QUIT_CONFIRM_TICKS 100         /* QUIT asks for confirmation when no save or load is this recent */
#define DEATH_WAIT_FRAMES 240
#define THE_END_WAIT_FRAMES 300
#define RESTART_WAIT_FRAMES 900
#define FUSE_TEXT_FRAMES 780
#define FUSE_END_FRAMES 600
#define SENSOR_DELAY_FRAMES 60         /* frames per unit of the endgame sensor delay */
#define FUSE_LORD_HEALTH 10000
#define FUSE_FIRST_ATTACK 55
#define FUSE_ATTACK_STEP 40
#define FUSE_LAST_ATTACK 255
#define FUSE_MAGICAL_LIGHT 200
#define FUSE_SHIELD 100
#define FUSE_TEXT_COUNT 8
#define FUSE_TEXT_SIZE 200
#define DECODE_MESSAGE_INVISIBLE ((int16_t)0x8001) /* decode_text: message mode, also invisible texts */
#define MAX_SKILL_LEVEL 16
#define ENDGAME_FONT_A 1               /* the endgame font has A-Z at codes 1-26 */
#define ENDGAME_TEXT_BACKGROUND COLOR_DARKEST_GRAY
#define ENDGAME_NAME_X 87
#define ENDGAME_SKILL_X 105
#define ENDGAME_ROW_HEIGHT 48
#define ENDGAME_NAME_Y 14
#define ENDGAME_SKILL_LINE 8
#define THE_END_BACKGROUND 0x004       /* dark blue of the THE END palette */
#define THE_END_WHITE 0xfff
#define BUTTON_FRAME_COLOR 0x86f
#define MOUSE_ENTRY_BOX 2              /* x1, x2, y1, y2 after the command word */
#define MOUSE_BOX_WORDS 4
#define ENDGAME_TEXT_SIZE 64
#define FUSE_FLASH_ROUNDS 3            /* the lord flashes 4 times per round; round n lasts n ticks per flash */
#define FUSE_FLASHES_PER_ROUND 4
#define SAVE_DIR_SIZE 1024
#define CSB_RESTART_TEXT_X 110         /* RESTART THIS GAME text position of the CSB endgame */
#define CSB_RESTART_TEXT_Y 154
#define CSB_ENDING_FILE "ENDA.DAT"     /* the CSB ending animation, in the disk A folder */

/* Choices of the save dialog without the format choice. */
enum { SAVE_CHOICE_CANCEL = 1, SAVE_CHOICE_SAVE = 2, SAVE_CHOICE_QUIT = 3 };
enum { QUIT_CHOICE_CANCEL = 2 };
enum { LOAD_CHOICE_CANCEL = 2 };
enum { SOUND_SCREAM_DEATH = 7, SOUND_FUSE = 14 };

void (*g_frame_hook)(void);
int16_t g_quit_requested, g_restart_requested, g_return_to_entrance, g_waiting_frame;
static char save_dir[SAVE_DIR_SIZE] = ".";

void startend_set_save_dir(const char *dir) { snprintf(save_dir, sizeof save_dir, "%s", dir); }
const char *startend_save_dir(void) { return save_dir; }

int wait_frame(void)
{
    if (!g_frame_hook || g_quit_requested) return 0;
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

/* Waits until a key or a button is pressed. Returns 0 when there are no frames. */
static int wait_for_input(void)
{
    command_discard_all_input();
    while (!g_input_received)
        if (!wait_frame()) return 0;
    return 1;
}

/* The game asks before QUIT when neither a save nor a load is recent. Returns 1 to quit. */
static int confirm_quit(void)
{
    if (g_last_load_game_time + QUIT_CONFIRM_TICKS >= g_game_time || g_last_save_game_time + QUIT_CONFIRM_TICKS >= g_game_time) return 1;
    dialog_draw(NULL, ui_string(TEXT_NOT_SAVED_OK_TO_QUIT), ui_string(TEXT_QUIT), ui_string(TEXT_CANCEL), NULL, NULL);
    return dialog_get_choice(2) != QUIT_CHOICE_CANCEL;
}

int save_current_game(const char **reason)
{
    char mini[SAVE_DIR_SIZE];
    *reason = NULL;
    if (g_adventure != ADVENTURE_CSB || g_dungeon_id != CSB_PRISON_DUNGEON_ID) return savegame_save(save_dir) ? SAVED_GAME : 0;
    snprintf(mini, sizeof mini, "%s/%s", gameloop_data_dir(), CSB_MINI_FILE);
    switch (savegame_make_adventure(save_dir, mini)) {
    case ADVENTURE_MADE: return SAVED_NEW_ADVENTURE;
    case ADVENTURE_NO_CHAMPIONS: *reason = "CAN'T MAKE NEW ADVENTURE WITHOUT CHAMPIONS."; return 0;
    case ADVENTURE_NAMES_NOT_UNIQUE: *reason = "EACH CHAMPION MUST HAVE A UNIQUE FIRST NAME!"; return 0;
    default: return 0;
    }
}

/* CSB save_game: SAVE AND PLAY saves and goes on, SAVE AND QUIT saves and ends the game. The dialog
   closes after a save; CSB has no quit without a save. */
static void save_game_csb(void)
{
    game_freeze();
    dialog_draw(NULL, NULL, ui_string(TEXT_CANCEL), ui_string(TEXT_SAVE), ui_string(TEXT_QUIT), NULL);
    int16_t choice = dialog_get_choice(3);
    if (choice == SAVE_CHOICE_SAVE || choice == SAVE_CHOICE_QUIT) {
        dialog_draw(NULL, ui_string(TEXT_SAVING_GAME), NULL, NULL, NULL, NULL);
        const char *reason;
        int saved = save_current_game(&reason);
        if (!saved) {
            dialog_draw(reason ? reason : ui_string(TEXT_UNABLE_TO_SAVE), NULL, ui_string(TEXT_OK), NULL, NULL, NULL);
            dialog_get_choice(1);
        } else if (choice == SAVE_CHOICE_QUIT) {
            endgame(0);
            return;
        } else if (saved == SAVED_NEW_ADVENTURE) {
            gameloop_start_loaded_game();
        }
        if (saved && g_advanced_features) advanced_print_message("GAME SAVED");
    }
    game_unfreeze();
}

/* save_game without disks: the dialog stays after a save until CANCEL or QUIT. */
void save_game(void)
{
    const char *message = NULL;
    if (g_adventure == ADVENTURE_CSB) {
        save_game_csb();
        return;
    }
    game_freeze();
    for (;;) {
        dialog_draw(message, NULL, ui_string(TEXT_CANCEL), ui_string(TEXT_SAVE), ui_string(TEXT_QUIT), NULL);
        message = NULL;
        int16_t choice = dialog_get_choice(3);
        if (choice == SAVE_CHOICE_SAVE) {
            dialog_draw(NULL, ui_string(TEXT_SAVING_GAME), NULL, NULL, NULL, NULL);
            if (!savegame_save(save_dir)) {
                dialog_draw(ui_string(TEXT_UNABLE_TO_SAVE), NULL, ui_string(TEXT_OK), NULL, NULL, NULL);
                dialog_get_choice(1);
            } else if (g_advanced_features) {
                message = "GAME SAVED";
            }
            continue;
        }
        if (choice == SAVE_CHOICE_QUIT) {
            if (!confirm_quit()) continue;
            g_saved_game_exists = 0;
            endgame(0);
            return;
        }
        break;
    }
    game_unfreeze();
}

int16_t load_saved_game(void)
{
    g_game_screen_inactive = 1;
    for (;;) {
        int16_t result = savegame_load(save_dir);
        if (result == SAVEGAME_LOADED) {
            dialog_draw(NULL, ui_string(TEXT_LOADING_GAME), NULL, NULL, NULL, NULL);
            return 1;
        }
        if (result == SAVEGAME_DAMAGED) {
            /* The original quits here; the port goes back to the entrance. */
            dialog_draw(ui_string(TEXT_GAME_DAMAGED), NULL, ui_string(TEXT_OK), NULL, NULL, NULL);
            dialog_get_choice(1);
            return 0;
        }
        dialog_draw(ui_string(TEXT_CANT_FIND_SAVED_GAME), NULL, ui_string(TEXT_LOAD_SAVED_GAME), ui_string(TEXT_CANCEL), NULL, NULL);
        int16_t choice = dialog_get_choice(2);
        if (!choice || choice == LOAD_CHOICE_CANCEL) return 0;
    }
}

/* endgame_print_text: capitals in the endgame font on the dark background. */
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
    uint8_t *box = kaos_ptr(ui_mouse_table_address(UI_INPUT_ENDGAME) + (uint32_t)entry_index * MOUSE_ENTRY_BYTES + MOUSE_ENTRY_BOX);
    int16_t words[MOUSE_BOX_WORDS] = { b->x, (int16_t)(b->x + b->w - 1), b->y, (int16_t)(b->y + b->h - 1) };
    for (int i = 0; i < MOUSE_BOX_WORDS; i++) {
        box[2 * i] = (uint8_t)((uint16_t)words[i] >> 8);
        box[2 * i + 1] = (uint8_t)words[i];
    }
}

/* A framed button with a text; its box becomes the click box of the endgame mouse table entry. */
static void endgame_draw_button(int16_t zone, const char *text, int16_t entry_index)
{
    Bitmap screen = ui_screen();
    int tw, th;
    Box b;
    if (!text_size(text, &tw, &th)) return;
    int16_t w = (int16_t)tw, h = (int16_t)th, inner_x = kaos_s16(BUTTON_INNER_BORDER), inner_y = kaos_s16(BUTTON_INNER_BORDER + 2);
    if (!layout_get_zone(zone, 0, 0, &b, &w, &h)) return;
    int16_t dx = (int16_t)(inner_x + kaos_s16(BUTTON_BORDER)), dy = (int16_t)(inner_y + kaos_s16(BUTTON_BORDER + 2));
    Box outer = { (int16_t)(b.x - dx), (int16_t)(b.y - dy), (int16_t)(b.w + 2 * dx), (int16_t)(b.h + 2 * dy) };
    Box inner = { (int16_t)(outer.x + inner_x), (int16_t)(outer.y + inner_y), (int16_t)(outer.w - 2 * inner_x), (int16_t)(outer.h - 2 * inner_y) };
    draw_fill(&screen, &outer, COLOR_DARK_GRAY);
    draw_fill(&screen, &inner, COLOR_BLACK);
    ui_print_zone(&screen, zone, COLOR_CYAN, COLOR_BLACK, text);
    set_endgame_click_box(entry_index, &inner);
}

/* The base skill levels above 1 of a champion, one line each below the name. */
static void print_champion_skills(uint16_t champion_index, int y)
{
    for (int16_t skill = 0; skill < SKILL_BASE_COUNT; skill++) {
        int16_t level = champion_get_skill_level((int16_t)champion_index, (uint16_t)(skill | SKILL_IGNORE_OBJECT_MODIFIERS | SKILL_IGNORE_TEMPORARY_EXPERIENCE));
        if (level > MAX_SKILL_LEVEL) level = MAX_SKILL_LEVEL;
        if (level == 1) continue;
        char line[ENDGAME_TEXT_SIZE];
        snprintf(line, sizeof line, "%s %s", ui_string(kaos_s16(SKILL_LEVEL_STRINGS + 2 * (level - 2))),
                 ui_string(kaos_s16(BASE_SKILL_STRINGS + 2 * skill)));
        y += ENDGAME_SKILL_LINE;
        endgame_print_text(ENDGAME_SKILL_X, y, COLOR_LIGHT_GRAY, line);
    }
}

/* The won game: each champion in a frame with the portrait, name, title, and base skill levels. */
static void draw_champion_parade(void)
{
    Bitmap screen = ui_screen();
    int16_t pw = kaos_s16(PORTRAIT_SIZE), ph = kaos_s16(PORTRAIT_SIZE + 2);
    memset(fb, COLOR_DARKEST_GRAY, sizeof fb);
    for (uint16_t i = 0; i < g_party_champion_count; i++) {
        const Champion *c = &g_champions[i];
        ui_blit_graphic(&screen, GFX_CHAMPION_FRAME, (int16_t)(ZONE_CHAMPION_FRAME_FIRST + i), COLOR_FLESH);
        int16_t w = pw, h = ph;
        Box b;
        Bitmap portrait = champion_portrait((int16_t)i);
        if (layout_get_zone((int16_t)(ZONE_CHAMPION_PORTRAIT_FIRST + i), pw, ph, &b, &w, &h))
            draw_blit(&portrait, w, h, &screen, &b, COLOR_DARK_GRAY, 0);
        char name[sizeof c->name + 1] = { 0 }, title[sizeof c->title + 1] = { 0 };
        memcpy(name, c->name, sizeof c->name);
        memcpy(title, c->title, sizeof c->title);
        int name_y = i * ENDGAME_ROW_HEIGHT + ENDGAME_NAME_Y;
        int title_x = ENDGAME_NAME_X + (int)strlen(name) * TEXT_CHAR_WIDTH;
        if (title[0] != ',' && title[0] != ';' && title[0] != '-') title_x += TEXT_CHAR_WIDTH; /* a space unless punctuation follows */
        endgame_print_text(ENDGAME_NAME_X, name_y, COLOR_GOLD, name);
        endgame_print_text(title_x, name_y, COLOR_GOLD, title);
        print_champion_skills(i, name_y + 1);
    }
    ui_set_screen_palette(NULL);
}

/* The box of a CSB endgame button: x, y, width, height words. */
static Box csb_box(uint32_t address)
{
    return (Box){ kaos_s16(address), kaos_s16(address + 2), kaos_s16(address + 4), kaos_s16(address + 6) };
}

/* CSB: RESTART THIS GAME in its fixed boxes. The click box is in the endgame mouse table. */
static void csb_draw_restart_button(void)
{
    Bitmap screen = ui_screen();
    Box outer = csb_box(CSB_RESTART_BOXES), inner = csb_box(CSB_RESTART_BOXES + CSB_BOX_BYTES);
    draw_fill(&screen, &outer, COLOR_DARK_GRAY);
    draw_fill(&screen, &inner, COLOR_BLACK);
    text_draw_screen(CSB_RESTART_TEXT_X, CSB_RESTART_TEXT_Y, COLOR_CYAN, COLOR_BLACK, ui_string(TEXT_RESTART_THIS_GAME));
}

/* THE END on a dark blue screen, with RESTART THIS GAME and QUIT when a saved game exists. CSB has no QUIT. */
static void draw_the_end(int with_buttons)
{
    Bitmap screen = ui_screen();
    uint16_t colors[PALETTE_COLORS];
    for (int i = 0; i < PALETTE_COLORS; i++) colors[i] = THE_END_BACKGROUND;
    colors[COLOR_WHITE] = THE_END_WHITE;
    memset(fb, COLOR_BLACK, sizeof fb);
    ui_blit_graphic(&screen, GFX_THE_END, ZONE_THE_END, COLOR_NONE);
    if (with_buttons) {
        if (g_adventure == ADVENTURE_CSB) {
            csb_draw_restart_button();
        } else {
            endgame_draw_button(ZONE_RESTART_BUTTON, ui_string(TEXT_RESTART_THIS_GAME), 0);
            endgame_draw_button(ZONE_QUIT_BUTTON, ui_string(TEXT_QUIT), 1);
        }
        colors[COLOR_DARK_GRAY] = BUTTON_FRAME_COLOR;
        colors[COLOR_CYAN] = THE_END_WHITE;
    }
    ui_set_screen_palette(colors);
}

/* Waits up to 900 frames for RESTART THIS GAME or QUIT. Returns 1 when the saved game is loaded. */
static int wait_for_restart(void)
{
    command_set_only_mouse_input(UI_INPUT_ENDGAME);
    command_discard_all_input();
    g_restart_requested = 0;
    for (int16_t frames = RESTART_WAIT_FRAMES; --frames && !g_restart_requested;) {
        command_process_queue();
        if (g_quit_requested) { /* the QUIT button: the credits, then the entrance */
            g_quit_requested = 0;
            break;
        }
        if (!g_restart_requested && !wait_frame()) break;
    }
    command_set_only_mouse_input(-1);
    if (!g_restart_requested || g_quit_requested) return 0;
    memset(fb, COLOR_BLACK, sizeof fb);
    int16_t loaded = load_saved_game();
    g_restart_requested = 0;
    if (!loaded) return 0;
    gameloop_start_loaded_game();
    return 1;
}

/* The credits screen until a key or a button is pressed. */
static void show_credits(void)
{
    Bitmap screen = ui_screen();
    ui_blit_graphic(&screen, GFX_CREDITS, ZONE_SCREEN, COLOR_NONE);
    uint16_t credits_colors[PALETTE_COLORS];
    for (int i = 0; i < PALETTE_COLORS; i++) credits_colors[i] = kaos_u16(CREDITS_PALETTE + 2 * i);
    ui_set_screen_palette(credits_colors);
    wait_for_input();
}

/* The port shows the entrance after the credits instead of quitting (deliberate deviation). */
void endgame(int16_t party_dead)
{
    command_set_only_mouse_input(-1);
    if (party_dead) music_play_end(g_game_won);
    if (party_dead && !g_game_won) {
        sound_request_play(SOUND_SCREAM_DEATH, g_party_map_x, g_party_map_y, SOUND_MODE_NOW);
        wait_frames(DEATH_WAIT_FRAMES);
    }
    g_game_screen_inactive = 1;
    if (party_dead && g_game_won) {
        draw_champion_parade();
        wait_for_input();
    }
    if (party_dead) {
        draw_the_end(0);
        wait_frames(THE_END_WAIT_FRAMES);
        if (g_saved_game_exists) {
            draw_the_end(1);
            if (wait_for_restart()) return;
        }
    }
    show_credits();
    g_return_to_entrance = 1;
}

/* fuse_sequence_update: one game tick with the view and the sounds. */
static void fuse_update(void)
{
    timeline_process();
    gameloop_draw_view();
    sound_play_pending();
    g_game_time++;
    wait_frame();
}

/* The fluxcage explosions on a square disappear. */
static void remove_fluxcages(int16_t x, int16_t y)
{
    uint16_t t = get_square_first_object(x, y);
    while (t != THING_ENDOFLIST) {
        if (THING_TYPE(t) == THING_EXPLOSION && explosion_type(t) == EXPLOSION_FLUXCAGE) {
            unlink_thing_from_list(t, 0, x, y);
            thing_set_free(t);
            t = get_square_first_object(x, y);
            continue;
        }
        t = get_next_thing(t);
    }
}

/* Six explosions of rising power on the square of Lord Chaos. */
static void fuse_explosions(uint16_t type, int16_t x, int16_t y)
{
    for (uint16_t attack = FUSE_FIRST_ATTACK; attack <= FUSE_LAST_ATTACK; attack += FUSE_ATTACK_STEP) {
        explosion_create(EXPLOSION_THING(type), attack, (uint16_t)x, (uint16_t)y, CELL_CENTERED);
        fuse_update();
    }
}

/* The texts on square (0, 0) of the map, one after the other in the order of their first letter
   (the letter itself is not shown). */
static void show_final_texts(void)
{
    uint16_t texts[FUSE_TEXT_COUNT];
    int16_t count = 0;
    for (uint16_t t = get_square_first_thing(0, 0); t != THING_ENDOFLIST && count < FUSE_TEXT_COUNT; t = get_next_thing(t))
        if (THING_TYPE(t) == THING_TEXT) texts[count++] = t;
    char letter = 'A';
    for (int16_t shown = 0; shown < count; shown++) {
        for (int16_t i = 0; i < count; i++) {
            char text[FUSE_TEXT_SIZE];
            decode_text(text, sizeof text, texts[i], DECODE_MESSAGE_INVISIBLE);
            if (text[1] != letter) continue;
            message_clear();
            text[1] = '\n';
            message_print(COLOR_WHITE, text + 1, g_game_time);
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
            group_set_creature_type(lord, flash & 1 ? CREATURE_LORD_ORDER : CREATURE_LORD_CHAOS);
            for (int16_t i = 0; i < ticks; i++) fuse_update();
        }
}

/* All creature groups of the map except the one on (x, y) disappear. */
static void delete_other_groups(int16_t x, int16_t y)
{
    for (int16_t gx = 0; gx < g_current_map_width; gx++)
        for (int16_t gy = 0; gy < g_current_map_height; gy++)
            if (group_get_thing(gx, gy) != THING_ENDOFLIST && (gx != x || gy != y)) group_delete(gx, gy);
}

void fuse_sequence(void)
{
    int16_t x = g_party_map_x, y = g_party_map_y;
    g_game_won = 1;
    if (g_inventory_champion_ordinal) inventory_toggle(CHAMPION_MAX_COUNT);
    command_highlight_disable();
    g_party.magical_light_amount = FUSE_MAGICAL_LIGHT;
    g_party.shield_defense = g_party.spell_shield_defense = g_party.fire_shield_defense = FUSE_SHIELD;
    champion_draw_all_states(CHAMPION_ATTR_STATUS_BOX);
    fuse_update();
    update_map_coordinates_after_relative_movement(g_party_direction, 1, 0, &x, &y);
    uint16_t lord = group_get_thing(x, y);
    if (lord == THING_ENDOFLIST) return;
    group_set_health(lord, 0, FUSE_LORD_HEALTH);
    set_group_cells(lord, CELLS_CENTERED, g_party_map_index);
    set_group_directions(lord, (uint8_t)NORMALIZE(g_party_direction + 2), g_party_map_index); /* faces the party */
    remove_fluxcages(g_party_map_x, g_party_map_y);
    remove_fluxcages(x, y);
    fuse_update();
    fuse_explosions(EXPLOSION_FIREBALL, x, y);
    sound_request_play(SOUND_FUSE, x, y, SOUND_MODE_AFTER_TICK);
    group_set_creature_type(lord, CREATURE_LORD_ORDER);
    fuse_update();
    fuse_explosions(EXPLOSION_HARM_NON_MATERIAL, x, y);
    flash_lords(lord, x, y);
    explosion_create(EXPLOSION_THING(EXPLOSION_FIREBALL), FUSE_LAST_ATTACK, (uint16_t)x, (uint16_t)y, CELL_CENTERED);
    explosion_create(EXPLOSION_THING(EXPLOSION_HARM_NON_MATERIAL), FUSE_LAST_ATTACK, (uint16_t)x, (uint16_t)y, CELL_CENTERED);
    fuse_update();
    group_set_creature_type(lord, CREATURE_GREY_LORD);
    fuse_update();
    g_do_not_draw_fluxcages = 1;
    fuse_update();
    delete_other_groups(x, y);
    fuse_update();
    show_final_texts();
    fuse_explosions(EXPLOSION_HARM_NON_MATERIAL, x, y);
    wait_frames(FUSE_END_FRAMES);
    g_saved_game_exists = 0;
    endgame(1);
}

/* CSB won game: the ending animation, then the CSB screen after a key or a click (the original stays on the last frame). */
static void csb_ending(void)
{
    char path[SAVE_DIR_SIZE];
    g_game_screen_inactive = 1;
    snprintf(path, sizeof path, "%s/%s", gameloop_data_dir(), CSB_ENDING_FILE);
    if (!anim_play(path)) wait_for_input();
    g_return_to_entrance = 1;
}

void endgame_sensor(uint16_t delay)
{
    if (g_adventure == ADVENTURE_CSB) { /* no delay, and the saved game stays */
        g_game_won = 1;
        csb_ending();
        return;
    }
    wait_frames((int16_t)(delay * SENSOR_DELAY_FRAMES));
    g_saved_game_exists = 0;
    g_game_won = 1;
    endgame(1);
}
