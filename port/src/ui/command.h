#ifndef COMMAND_H
#define COMMAND_H
#include <stdint.h>
#include "move.h"

/* Player input as the original command table (KAOS range 8, COMMAND.C): clicks and keys become
   queued commands; command_process_queue runs them. Movement commands (1-6) are in move.h. */

enum {
    COMMAND_NONE = 0,
    COMMAND_TOGGLE_INVENTORY_FIRST = 7,    /* + champion 0-3 */
    COMMAND_CLOSE_INVENTORY = 11,
    COMMAND_CLICK_STATUS_BOX_FIRST = 12,   /* + champion */
    COMMAND_SET_LEADER_FIRST = 16,         /* + champion */
    COMMAND_CLICK_SLOT_BOX_FIRST = 20,     /* + slot box */
    COMMAND_CLICK_SLOT_BOX_LAST = 65,
    COMMAND_CLICK_MOUTH = 70,
    COMMAND_CLICK_EYE = 71,
    COMMAND_CLICK_DUNGEON_VIEW = 80,
    COMMAND_CLICK_PANEL = 81,
    COMMAND_TOGGLE_INVENTORY_LEADER = 83,
    COMMAND_CLICK_SPELL_AREA = 100,
    COMMAND_SPELL_SYMBOL_FIRST = 101,      /* + symbol 0-5 */
    COMMAND_DELETE_SYMBOL = 107,
    COMMAND_CAST_SPELL = 108,
    COMMAND_CHANGE_MAGIC_CASTER = 109,
    COMMAND_CLICK_ACTION_AREA = 111,
    COMMAND_PASS = 112,
    COMMAND_ACTION_FIRST = 113,            /* + action list index 0-2 */
    COMMAND_SELECT_ACTING_CHAMPION_FIRST = 116, /* + champion */
    COMMAND_CLICK_CHAMPION_ICON_FIRST = 125,    /* + cell relative to the party direction */
    COMMAND_CLICK_CHAMPION_ICON_LAST = 128,
    COMMAND_RIGHT_RELEASE = 129,
    COMMAND_SAVE_GAME = 140,
    COMMAND_SLEEP = 145,
    COMMAND_WAKE_UP = 146,
    COMMAND_FREEZE_GAME = 147,
    COMMAND_UNFREEZE_GAME = 148,
    COMMAND_ENTRANCE_ENTER = 200,
    COMMAND_ENTRANCE_ENTER_DUNGEON_B = 201,
    COMMAND_ENTRANCE_RESUME = 202,
    COMMAND_ENTRANCE_CREDITS = 203,
    COMMAND_DIALOG_CHOICE_FIRST = 210,     /* + choice 0-3 */
    COMMAND_RESTART_GAME = 215,
    COMMAND_QUIT = 216,
    COMMAND_LEFT_RELEASE = 254,
};

/* Click kinds of the input handler. */
enum { CLICK_RIGHT_PRESS = 1, CLICK_LEFT_PRESS = 2, CLICK_LEFT_RELEASE = 4, CLICK_RIGHT_RELEASE = 8 };

/* Key words: Amiga raw key in the high byte, ASCII in the low byte. */
enum {
    KEY_RETURN = 0x4400, KEY_ESCAPE = 0x4500, KEY_DEL = 0x4600, KEY_HELP = 0x5f00,
    KEY_UP = 0x4c00, KEY_DOWN = 0x4d00, KEY_RIGHT = 0x4e00, KEY_LEFT = 0x4f00, KEY_F1 = 0x5000,
};

/* Result of the entrance (g_new_game): ENTRANCE_WAITING until a button is pressed. */
enum { ENTRANCE_RESUME = 0, ENTRANCE_NEW_GAME = 1, ENTRANCE_WAITING = 99 };

extern int16_t g_stop_waiting_for_input;      /* set by commands that end the wait for the next tick */
extern int16_t g_game_time_ticking;           /* 0 while the game is frozen */
extern int16_t g_wait_for_input_max_frames;   /* frames of one tick without input: 12, 0 while sleeping */
extern int16_t g_game_frozen;
extern int16_t g_new_game;
extern int16_t g_input_received;              /* a key or button press since the last discard */
extern uint16_t g_last_key;                   /* the last key word since the last discard, 0: none */

void command_set_entrance_input(void);
void command_set_game_input(void);
void command_reset(void);
/* Dialogs and the endgame: only one mouse table; the saved tables come back with restore. */
void command_set_only_mouse_input(int16_t table);
void command_save_input(void);
void command_restore_input(void);
void command_process_click(int16_t x, int16_t y, int16_t kind);
void command_process_key(uint16_t key);
int command_process_queue(void);
void command_discard_all_input(void);
void command_highlight_disable(void);
void command_wake_up(void);

extern int16_t g_mouse_pointer_hidden;           /* the eye, or an imaginary wall, is pressed */

void game_freeze(void);
void game_unfreeze(void);
void inventory_toggle(int16_t champion_index);
void command_release_dead_champion(uint16_t champion_index, int16_t leader_index);

#endif
