/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_UI_COMMAND_H
#define DM_UI_COMMAND_H
/* Prefixes: command_ */
#include "engine/move.h"
#include <stdbool.h>
#include <stdint.h>

/* Player input as the original command table (KAOS range 8, COMMAND.C): clicks and keys become
   queued commands; command_process_queue runs them. Movement commands (1-6) are in move.h. */

#define COMMAND_NONE                         0
#define COMMAND_TOGGLE_INVENTORY_FIRST       7 /* + champion 0-3 */
#define COMMAND_CLOSE_INVENTORY              11
#define COMMAND_CLICK_STATUS_BOX_FIRST       12 /* + champion */
#define COMMAND_SET_LEADER_FIRST             16 /* + champion */
#define COMMAND_CLICK_SLOT_BOX_FIRST         20 /* + slot box */
#define COMMAND_CLICK_SLOT_BOX_LAST          65
#define COMMAND_CLICK_MOUTH                  70
#define COMMAND_CLICK_EYE                    71
#define COMMAND_CLICK_DUNGEON_VIEW           80
#define COMMAND_CLICK_PANEL                  81
#define COMMAND_TOGGLE_INVENTORY_LEADER      83
#define COMMAND_CLICK_SPELL_AREA             100
#define COMMAND_SPELL_SYMBOL_FIRST           101 /* + symbol 0-5 */
#define COMMAND_DELETE_SYMBOL                107
#define COMMAND_CAST_SPELL                   108
#define COMMAND_CHANGE_MAGIC_CASTER          109
#define COMMAND_CLICK_ACTION_AREA            111
#define COMMAND_PASS                         112
#define COMMAND_ACTION_FIRST                 113 /* + action list index 0-2 */
#define COMMAND_SELECT_ACTING_CHAMPION_FIRST 116 /* + champion */
#define COMMAND_CLICK_CHAMPION_ICON_FIRST    125 /* + cell relative to the party direction */
#define COMMAND_CLICK_CHAMPION_ICON_LAST     128
#define COMMAND_RIGHT_RELEASE                129
#define COMMAND_SAVE_GAME                    140
#define COMMAND_SLEEP                        145
#define COMMAND_WAKE_UP                      146
#define COMMAND_FREEZE_GAME                  147
#define COMMAND_UNFREEZE_GAME                148
#define COMMAND_ENTRANCE_ENTER               200
#define COMMAND_ENTRANCE_ENTER_DUNGEON_B     201
#define COMMAND_ENTRANCE_RESUME              202
#define COMMAND_ENTRANCE_CREDITS             203
#define COMMAND_DIALOG_CHOICE_FIRST          210 /* + choice 0-3 */
#define COMMAND_RESTART_GAME                 215
#define COMMAND_QUIT                         216
#define COMMAND_LEFT_RELEASE                 254

/* Click kinds of the input handler. */
#define CLICK_RIGHT_PRESS   1
#define CLICK_LEFT_PRESS    2
#define CLICK_LEFT_RELEASE  4
#define CLICK_RIGHT_RELEASE 8

/* Key words: Amiga raw key in the high byte, ASCII in the low byte. */
#define KEY_RETURN    0x4400
#define KEY_ESCAPE    0x4500
#define KEY_DEL       0x4600
#define KEY_HELP      0x5f00
#define KEY_UP        0x4c00
#define KEY_DOWN      0x4d00
#define KEY_RIGHT     0x4e00
#define KEY_LEFT      0x4f00
#define KEY_F1        0x5000
#define KEY_RAW_SHIFT 8

/* The key word of a raw key and an ASCII code. */
static inline uint16_t command_key(uint16_t raw, uint16_t ascii)
{
    return raw << KEY_RAW_SHIFT | ascii;
}
/* The key word of F1-F4: the key of the champion with this index. */
static inline uint16_t command_champion_key(int16_t champion_index)
{
    return KEY_F1 + (champion_index << KEY_RAW_SHIFT);
}

/* Result of the entrance (g_new_game): ENTRANCE_WAITING until a button is pressed. */
#define ENTRANCE_RESUME   0
#define ENTRANCE_NEW_GAME 1
#define ENTRANCE_WAITING  99

extern bool g_stop_waiting_for_input;       /* set by commands that end the wait for the next tick */
extern bool g_game_time_ticking;            /* false while the game is frozen */
extern int16_t g_wait_for_input_max_frames; /* frames of one tick without input: 12, 0 while sleeping */
extern bool g_game_frozen;
extern int16_t g_new_game;
extern bool g_input_received; /* a key or button press since the last discard */
extern uint16_t g_last_key;   /* the last key word since the last discard, 0: none */

void command_set_entrance_input(void);
void command_set_game_input(void);
void command_reset(void);
/* Dialogs and the endgame: only one mouse table; the saved tables come back with restore. */
void command_set_only_mouse_input(int16_t table);
void command_save_input(void);
void command_restore_input(void);
void command_process_click(int16_t x, int16_t y, int16_t kind);
void command_process_key(uint16_t key);
bool command_process_queue(void);
void command_discard_all_input(void);
void command_highlight_disable(void);
void command_wake_up(void);

extern bool g_mouse_pointer_hidden; /* the eye, or an imaginary wall, is pressed */

void command_freeze_game(void);
void command_unfreeze_game(void);
void command_toggle_inventory(int16_t champion_index);
void command_click_on_mouth(void);
void command_release_dead_champion(uint16_t champion_index, int16_t leader_index);

#endif
