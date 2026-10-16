/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_UI_GAMELOOP_H
#define DM_UI_GAMELOOP_H
/* Prefixes: gameloop_ */

/* Game start and the main loop: it loads the game data, starts a new or a loaded game, runs a tick, and draws what
   changed. It must not hold a game rule; the rules are in engine/. */

#include <stdbool.h>
#include <stdint.h>

#define POINTER_SIZE 32 /* largest pointer image, in pixels */

#define WAIT_FOR_INPUT_FRAMES 12 /* frames of one tick without input */

extern bool g_stop_waiting_for_input;       /* set by commands that end the wait for the next tick */
extern bool g_game_time_ticking;            /* false while the game is frozen */
extern int16_t g_wait_for_input_max_frames; /* frames of one tick without input: 12, 0 while sleeping */
extern bool g_game_frozen;                  /* stays set while the party sleeps; command_unfreeze_game keeps it then */
extern bool
    g_game_screen_shown; /* set: the game screen shows; clear: dialogs use the whole screen (entrance, endgame) */

/* Loads KAOS.FTL and Graphics.DAT from dir and sets up the interface. Returns true on success. */
bool gameloop_load_data(const char *dir);
/* The directory that gameloop_load_data read the game files from. */
const char *gameloop_get_data_dir(void);
/* Loads Dungeon.DAT from dir and starts a new game on the start square. Returns true on success. */
bool gameloop_start_new_game(const char *dir);
/* Starts a game that savegame_load restored: the party map, "GAME LOADED", the interface. */
void gameloop_start_loaded_game(void);
/* gameloop_start_loaded_game without the "GAME LOADED" dialog: the game goes on at once (quick load). */
void gameloop_continue_loaded_game(void);
/* The dungeon view of the party in the light of the party. */
void gameloop_draw_view(void);
/* Runs the waiting commands of one 50 Hz frame. Returns true when the wait for input is over and a
   game tick is due (gameloop_tick). */
bool gameloop_run_frame(void);
/* Runs all waiting commands and draws what they changed, without the wait for a tick. */
void gameloop_run_commands(void);
/* One game tick: map change, timeline, view, sounds, damage, time, champion effects, interface. A dead party
   ends in the endgame. */
void gameloop_tick(void);
/* The pointer image at screen position (x, y): px gets w x h colors (0 transparent) of the sprite
   palette. Returns false when the pointer is hidden. */
bool gameloop_get_pointer(int16_t x, int16_t y, bool in_game, uint8_t *px, int *w, int *h, int *hot_x, int *hot_y);
/* The palette of the mouse pointer. */
const uint16_t *gameloop_get_pointer_palette(void);

#endif
