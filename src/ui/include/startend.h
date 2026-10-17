/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_UI_STARTEND_H
#define DM_UI_STARTEND_H
/* Prefixes: startend_ */

/* Sequences outside the game loop: the save dialog, resuming a saved game, the endgame screens, and the fuse
   sequence with Lord Chaos. They wait frame by frame like the original; the program shows each frame through
   g_frame_hook (ui/frame.h). */

#include <stdbool.h>
#include <stdint.h>

extern bool g_restart_requested;  /* RESTART THIS GAME on the endgame screen */
extern bool g_return_to_entrance; /* the endgame ended: the program shows the entrance */

/* The directory of the save file of the adventure (DMGAME.DAT or CSBGAME.DAT). The default is the game data directory. */
void startend_set_save_dir(const char *directory);
/* The directory of the save file now in use. */
const char *startend_save_dir(void);

/* Writes the file of the save slot 0-7 (slot 0: the save file of the adventure) and makes the slot the current
   slot. In the CSB prison it makes the new adventure in the slot (CSBGAME.DAT for slot 0) and loads it; the caller
   starts it (gameloop_start_loaded_game). Returns SAVED_GAME, SAVED_NEW_ADVENTURE, or 0. On 0, *reason is the
   reason why the prison party cannot make the adventure, or NULL when the file cannot be written. */
int startend_save_current_game(int16_t slot, const char **reason);
/* Results of startend_save_current_game. */
#define SAVED_GAME          1
#define SAVED_NEW_ADVENTURE 2
/* The advanced CSB start by the prison stairs: the party goes into the CSB dungeon without a file, and the game
   goes on there (no GAME LOADED dialog). A refused party gets a dialog with OK. Returns true when the game started. */
bool startend_take_prison_stairs(void);
/* The save command: CANCEL, SAVE, QUIT. QUIT ends the game through the endgame. CSB: CANCEL, SAVE AND
   PLAY, SAVE AND QUIT. Without disks the dialog stays after a save until CANCEL or QUIT. In the advanced mode a
   save opens the save list, and the chosen slot becomes the current slot. */
void startend_save_game(void);
/* RESUME: loads the saved game with its dialogs; in the advanced mode the load list chooses a slot or the quick
   save first. Returns true when the game is loaded. */
bool startend_load_saved_game(void);
/* Draws the credits graphic on the whole screen; the caller sets the credits palette. */
void startend_draw_credits(void);
/* The end of the game: game over (party_dead true), won (g_session.won), or quit (party_dead false). It
   returns after RESTART THIS GAME loaded the saved game, or after the credits with g_return_to_entrance set:
   the port shows the entrance after the credits instead of quitting (deliberate deviation). */
void startend_run_endgame(bool party_dead);
/* The party fused Lord Chaos: the final explosions, the texts of square (0, 0), then the endgame. */
void startend_run_fuse_sequence(void);
/* The endgame sensor: waits delay seconds, then the won endgame. CSB: the ending animation (ENDA.DAT) at once. */
void startend_run_endgame_sensor(uint16_t delay);

#endif
